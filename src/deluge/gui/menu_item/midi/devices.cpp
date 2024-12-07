/*
 * Copyright © 2021-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "devices.h"
#include "device.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "io/debug/log.h"
#include "io/midi/cable_types/din.h"
#include "io/midi/cable_types/usb_device_cable.h"
#include "io/midi/cable_types/usb_hosted.h"
#include "io/midi/midi_device.h"
#include "io/midi/midi_device_manager.h"
#include "io/midi/midi_root_complex.h"
#include "io/midi/root_complex/usb_hosted.h"
#include "io/midi/root_complex/usb_peripheral.h"
#include "util/container/static_vector.hpp"
#include <string_view>

extern deluge::gui::menu_item::midi::Device midiDeviceMenu;

namespace deluge::gui::menu_item::midi {

static constexpr int32_t lowestDeviceNum = -3;

void Devices::beginSession(MenuItem* navigatedBackwardFrom) {
	bool found = false;
	if (navigatedBackwardFrom != nullptr) {
		// This will technically do the wrong thing when we're in peripheral mode (it'll set the max index to 2 instead
		// of 0, which would be accurate) but it should be harmless -- `Devices::getCable` should just return nullptr in
		// that case which we handle fine already anyway.
		auto maxIndex =
		    (MIDIDeviceManager::rootUSB != nullptr) ? MIDIDeviceManager::rootUSB->getNumCables() : lowestDeviceNum + 1;
		for (int32_t idx = lowestDeviceNum; idx < maxIndex; idx++) {
			if (getCable(idx) == soundEditor.currentMIDICable) {
				found = true;
				this->setValue(idx);
				break;
			}
		}
	}

	if (!found) {
		this->setValue(lowestDeviceNum); // Start on "DIN". That's the only one that'll always be there.
	}

	soundEditor.currentMIDICable = getCable(this->getValue());
	if (display->haveOLED()) {
		currentScroll = this->getValue();
	}
	else {
		drawValue();
	}
}

void Devices::selectEncoderAction(int32_t offset) {
	offset = std::clamp<int32_t>(offset, -1, 1);

	auto maxIndex = (MIDIDeviceManager::rootUSB == nullptr) ? 0 : MIDIDeviceManager::rootUSB->getNumCables();

	do {
		int32_t newValue = this->getValue() + offset;

		if (newValue >= maxIndex) {
			if (display->haveOLED()) {
				return;
			}
			newValue = lowestDeviceNum;
		}
		else if (newValue < lowestDeviceNum) {
			if (display->haveOLED()) {
				return;
			}
			newValue = maxIndex - 1;
		}

		this->setValue(newValue);

		soundEditor.currentMIDICable = getCable(this->getValue());

	} while (soundEditor.currentMIDICable == nullptr && soundEditor.currentMIDICable->connectionFlags == 0);
	// Don't show devices which aren't connected. Sometimes we won't even have a name to display for them.

	if (display->haveOLED()) {
		if (this->getValue() < currentScroll) {
			currentScroll = this->getValue();
		}
		//
		if (offset >= 0) {
			int32_t d = this->getValue();
			int32_t numSeen = 1;
			while (d > lowestDeviceNum) {
				d--;
				if (d == currentScroll) {
					break;
				}
				auto device = getCable(d);
				if (!(device && device->connectionFlags)) {
					continue;
				}
				numSeen++;
				if (numSeen >= kOLEDMenuNumOptionsVisible) {
					currentScroll = d;
					break;
				}
			}
		}
	}

	drawValue();
}

MIDICable* Devices::getCable(int32_t deviceIndex) {
	if (deviceIndex < lowestDeviceNum) {
		D_PRINTLN("impossible device request");
		return nullptr;
	}

	if (deviceIndex == -3) {
		return &MIDIDeviceManager::rootDin.cable;
	}

	if (MIDIDeviceManager::rootUSB != nullptr) {
		auto& rootUSB = *MIDIDeviceManager::rootUSB;
		if (deviceIndex < 0) {
			if (rootUSB.getType() == RootComplexType::RC_USB_PERIPHERAL && deviceIndex >= -2) {
				return rootUSB.getCable(deviceIndex + 2);
			}
			return nullptr;
		}

		if (rootUSB.getType() == RootComplexType::RC_USB_HOST) {
			auto& usb = static_cast<MIDIRootComplexUSBHosted&>(rootUSB);
			return usb.getCable(deviceIndex);
		}
	}

	return nullptr;
}

void Devices::drawValue() {
	if (display->haveOLED()) {
		renderUIsForOled();
	}
	else {
		char const* displayName = soundEditor.currentMIDICable->getDisplayName();
		display->setScrollingText(displayName);
	}
}

MenuItem* Devices::selectButtonPress() {
	return &midiDeviceMenu;
}

void Devices::drawPixelsForOled() {
	static_vector<std::string_view, kOLEDMenuNumOptionsVisible> itemNames = {};

	int32_t selectedRow = -1;

	auto device_idx = currentScroll;
	size_t row = 0;
	auto max_index = (MIDIDeviceManager::rootUSB == nullptr) ? 0 : MIDIDeviceManager::rootUSB->getNumCables();
	while (row < kOLEDMenuNumOptionsVisible && device_idx < static_cast<ptrdiff_t>(max_index)) {
		MIDICable* cable = getCable(device_idx);
		if (cable != nullptr && cable->connectionFlags != 0u) {
			itemNames.push_back(cable->getDisplayName());
			if (device_idx == this->getValue()) {
				selectedRow = static_cast<int32_t>(row);
			}
			row++;
		}
		device_idx++;
	}

	drawItemsForOled(itemNames, selectedRow);
}

} // namespace deluge::gui::menu_item::midi
