// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata.h"

#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/fixed_flat_map.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

namespace {

static constexpr auto kMouseMetadata = base::MakeFixedFlatMap<VendorProductId,
                                                              MouseMetadata>({
    // Fake data for testing.
    {{0xffff, 0xfffe}, {mojom::CustomizationRestriction::kAllowCustomizations}},
    // Fake data for testing.
    {{0xffff, 0xffff},
     {mojom::CustomizationRestriction::kDisallowCustomizations}},
    // Razer Naga Pro (USB Dongle)
    {{0x1532, 0x0090},
     {mojom::CustomizationRestriction::kAllowAlphabetOrNumberKeyEventRewrites}},
    // Razer Naga Pro (Bluetooth)
    {{0x1532, 0x0092},
     {mojom::CustomizationRestriction::kAllowAlphabetOrNumberKeyEventRewrites}},
    // HP 690/695 Mouse
    {{0x3f0, 0x804a},
     {mojom::CustomizationRestriction::kAllowAlphabetOrNumberKeyEventRewrites}},
});

static constexpr auto kKeyboardMetadata =
    base::MakeFixedFlatMap<VendorProductId, KeyboardMetadata>({
        {{0x03f0, 0x1f41}, {}},  // HP OMEN Sequencer
        {{0x045e, 0x082c}, {}},  // Microsoft Ergonomic Keyboard
        {{0x046d, 0x4088}, {}},  // Logitech ERGO K860 (Bluetooth)
        {{0x046d, 0x408a}, {}},  // Logitech MX Keys (Universal Receiver)
        {{0x046d, 0xb350}, {}},  // Logitech Craft Keyboard
        {{0x046d, 0xb359}, {}},  // Logitech ERGO K860
        {{0x046d, 0xb35b}, {}},  // Logitech MX Keys (Bluetooth)
        {{0x046d, 0xb35f}, {}},  // Logitech G915 TKL (Bluetooth)
        {{0x046d, 0xb361}, {}},  // Logitech MX Keys for Mac (Bluetooth)
        {{0x046d, 0xb364}, {}},  // Logitech ERGO 860B
        {{0x046d, 0xc336}, {}},  // Logitech G213
        {{0x046d, 0xc33f}, {}},  // Logitech G815 RGB
        {{0x046d, 0xc343}, {}},  // Logitech G915 TKL (USB)
        {{0x05ac, 0x024f}, {}},  // EGA MGK2 (Bluetooth) + Keychron K2
        {{0x05ac, 0x0256}, {}},  // EGA MGK2 (USB)
        {{0x0951, 0x16e5}, {}},  // HyperX Alloy Origins
        {{0x0951, 0x16e6}, {}},  // HyperX Alloy Origins Core
        {{0x1038, 0x1612}, {}},  // SteelSeries Apex 7
        {{0x1065, 0x0002}, {}},  // SteelSeries Apex 3 TKL
        {{0x1532, 0x022a}, {}},  // Razer Cynosa Chroma
        {{0x1532, 0x025d}, {}},  // Razer Ornata V2
        {{0x1532, 0x025e}, {}},  // Razer Cynosa V2
        {{0x1532, 0x026b}, {}},  // Razer Huntsman V2 Tenkeyless
        {{0x1535, 0x0046}, {}},  // Razer Huntsman Elite
        {{0x1b1c, 0x1b2d}, {}},  // Corsair Gaming K95 RGB Platinum
        {{0x28da, 0x1101}, {}},  // G.Skill KM780
        {{0x29ea, 0x0102}, {}},  // Kinesis Freestyle Edge RGB
        {{0x2f68, 0x0082}, {}},  // Durgod Taurus K320
        {{0x320f, 0x5044}, {}},  // Glorious GMMK Pro
        {{0x3297, 0x1969}, {}},  // ZSA Moonlander Mark I
        {{0x3297, 0x4974}, {}},  // ErgoDox EZ
        {{0x3297, 0x4976}, {}},  // ErgoDox EZ Glow
        {{0x3434, 0x0121}, {}},  // Keychron Q3
        {{0x3434, 0x0151}, {}},  // Keychron Q5
        {{0x3434, 0x0163}, {}},  // Keychron Q6
        {{0x3434, 0x01a1}, {}},  // Keychron Q10
        {{0x3434, 0x0311}, {}},  // Keychron V1
        {{0x3496, 0x0006}, {}},  // Keyboardio Model 100
        {{0x4c44, 0x0040}, {}},  // LazyDesigners Dimple
        {{0xfeed, 0x1307}, {}},  // ErgoDox EZ
    });

static constexpr auto kKeyboardMouseComboMetadata =
    base::MakeFixedFlatMap<VendorProductId, KeyboardMouseComboMetadata>({
        // Logitech K400
        {{0x046d, 0x4024},
         {mojom::CustomizationRestriction::kDisallowCustomizations}},
        // Logitech K400+
        {{0x046d, 0x404d},
         {mojom::CustomizationRestriction::kDisallowCustomizations}},
        // Logitech BOLT Receiver
        {{0x046d, 0xc548},
         {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
        // Lenovo TrackPoint Keyboard II
        {{0x17ef, 0x60e1},
         {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
        // Lenovo TrackPoint Keyboard II
        {{0x17ef, 0x60ee},
         {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
        // Lenovo ThinkPad Compact USB Keyboard with TrackPoint
        {{0x17ef, 0x6047},
         {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
        // Lenovo 100 USB-A Wireless Combo Keyboard and Mouse
        {{0x17ef, 0x609f},
         {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
    });

}  // namespace

bool MouseMetadata::operator==(const MouseMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

bool KeyboardMouseComboMetadata::operator==(
    const KeyboardMouseComboMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

const MouseMetadata* GetMouseMetadata(const ui::InputDevice& device) {
  const auto* iter = kMouseMetadata.find({device.vendor_id, device.product_id});
  if (iter != kMouseMetadata.end()) {
    return &(iter->second);
  }
  return nullptr;
}

const KeyboardMetadata* GetKeyboardMetadata(const ui::InputDevice& device) {
  const auto* iter =
      kKeyboardMetadata.find({device.vendor_id, device.product_id});
  if (iter != kKeyboardMetadata.end()) {
    return &(iter->second);
  }
  return nullptr;
}

const KeyboardMouseComboMetadata* GetKeyboardMouseComboMetadata(
    const ui::InputDevice& device) {
  const auto* iter =
      kKeyboardMouseComboMetadata.find({device.vendor_id, device.product_id});
  if (iter != kKeyboardMouseComboMetadata.end()) {
    return &(iter->second);
  }
  return nullptr;
}

DeviceType GetDeviceType(const ui::InputDevice& device) {
  const auto* keyboard_mouse_combo_metadata =
      GetKeyboardMouseComboMetadata(device);
  if (keyboard_mouse_combo_metadata) {
    return DeviceType::kKeyboardMouseCombo;
  }

  const auto* keyboard_metadata = GetKeyboardMetadata(device);
  if (keyboard_metadata) {
    return DeviceType::kKeyboard;
  }

  const auto* mouse_metadata = GetMouseMetadata(device);
  if (mouse_metadata) {
    return DeviceType::kMouse;
  }

  return DeviceType::kUnknown;
}

}  // namespace ash
