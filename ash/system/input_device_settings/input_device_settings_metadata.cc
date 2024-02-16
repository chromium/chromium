// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/no_destructor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace ash {

namespace {

std::vector<mojom::ButtonRemappingPtr> GetDefaultButtonRemappingList() {
  return {};
}

std::vector<mojom::ButtonRemappingPtr> GetFiveKeyButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_FORWARD_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_BACK_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide),
      /*remapping_action=*/nullptr));
  return array;
}

std::vector<mojom::ButtonRemappingPtr> GetLogitechSixKeyButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_FORWARD_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_BACK_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_SIDE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      /*remapping_action=*/nullptr));
  return array;
}

std::vector<mojom::ButtonRemappingPtr>
GetLogitechSixKeyWithTabButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_FORWARD_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_BACK_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_SIDE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewVkey(ui::VKEY_TAB),
      /*remapping_action=*/nullptr));
  return array;
}

std::vector<mojom::ButtonRemappingPtr>
GetWacomStandardPenButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_PEN_FRONT_BUTTON_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kRightClick)));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_PEN_REAR_BUTTON_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kRight),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kMiddleClick)));
  return array;
}

std::vector<mojom::ButtonRemappingPtr>
GetWacomStandardFourButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_TABLET_EXPRESS_KEY_1_NAME),
      /*button=*/
      mojom::Button::NewVkey(ui::VKEY_BUTTON_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_SHIFT, static_cast<int>(ui::DomCode::SHIFT_LEFT),
          static_cast<int>(ui::DomKey::SHIFT), ui::EF_SHIFT_DOWN,
          /*key_display=*/""))));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_TABLET_EXPRESS_KEY_2_NAME),
      /*button=*/
      mojom::Button::NewVkey(ui::VKEY_BUTTON_1),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_MENU, static_cast<int>(ui::DomCode::ALT_LEFT),
          static_cast<int>(ui::DomKey::ALT), ui::EF_ALT_DOWN,
          /*key_display=*/""))));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_TABLET_EXPRESS_KEY_3_NAME),
      /*button=*/
      mojom::Button::NewVkey(ui::VKEY_BUTTON_2),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_CONTROL, static_cast<int>(ui::DomCode::CONTROL_LEFT),
          static_cast<int>(ui::DomKey::CONTROL), ui::EF_CONTROL_DOWN,
          /*key_display=*/""))));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_TABLET_EXPRESS_KEY_4_NAME),
      /*button=*/
      mojom::Button::NewVkey(ui::VKEY_BUTTON_3),
      mojom::RemappingAction::NewAcceleratorAction(
          AcceleratorAction::kToggleOverview)));
  return array;
}

}  // namespace

const base::flat_map<VendorProductId, MouseMetadata>& GetMouseMetadataList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, MouseMetadata>>
      mouse_metadata_list({
          // Fake data for testing.
          {{0xffff, 0xfffe},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::MouseButtonConfig::kLogitechSixKey}},
          // Fake data for testing.
          {{0xffff, 0xffff},
           {mojom::CustomizationRestriction::kDisallowCustomizations,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech M720 Triathlon (USB Dongle)
          {{0x046d, 0x405e},
           {mojom::CustomizationRestriction::kAllowTabEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX Master 2S (USB Dongle)
          {{0x046d, 0x4069},
           {mojom::CustomizationRestriction::kAllowTabEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKeyWithTab}},
          // Logitech MX Master 3 (USB Dongle)
          {{0x046d, 0x4082},
           {mojom::CustomizationRestriction::kAllowTabEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKeyWithTab}},
          // Logitech ERGO M575 (USB Dongle)
          {{0x046d, 0x4096},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech M510 (USB Dongle)
          {{0x046d, 0x4051},
           {mojom::CustomizationRestriction::
                kAllowHorizontalScrollWheelRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HP 690/695 Mouse
          {{0x03f0, 0x804a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech M650 L
          {{0x046d, 0xb02a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech MX Master 3S (Bluetooth)
          {{0x046d, 0xb034},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKey}},
          // Logitech MX Master 3S B (Bluetooth)
          {{0x046d, 0xb035},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKey}},
          // Logitech MX Anywhere 3S (Bluetooth)
          {{0x046d, 0xb037},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech M500 (USB)
          {{0x046d, 0xc069},
           {mojom::CustomizationRestriction::
                kAllowHorizontalScrollWheelRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // SteelSeries Aerox 9 WL (USB)
          {{0x1038, 0x185a},
           {mojom::CustomizationRestriction::
                kAllowAlphabetOrNumberKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga Pro (USB Dongle)
          {{0x1532, 0x0090},
           {mojom::CustomizationRestriction::
                kAllowAlphabetOrNumberKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
      });
  return *mouse_metadata_list;
}

const base::flat_map<VendorProductId, GraphicsTabletMetadata>&
GetGraphicsTabletMetadataList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, GraphicsTabletMetadata>>
      graphics_tablet_metadata_list({
          // Fake data for testing.
          {{0xeeee, 0xeeee},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kNoConfig}},
          // One by Wacom S
          {{0x056a, 0x037a},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // One by Wacom M
          {{0x056a, 0x0301},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // Wacom One Pen Tablet S
          {{0x056a, 0x0100},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // Wacom One pen tablet M
          {{0x056a, 0x0102},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // Wacom One Pen Display 11
          {{0x056a, 0x03Ce},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // Wacom One Pen Display 13 Touch
          {{0x056a, 0x03Cb},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly}},
          // Wacom Intuos S
          {{0x056a, 0x0374},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons}},
          // Wacom Intuos M
          {{0x056a, 0x0375},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons}},
      });
  return *graphics_tablet_metadata_list;
}

const base::flat_map<VendorProductId, KeyboardMetadata>&
GetKeyboardMetadataList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, KeyboardMetadata>>
      keyboard_metadata_list({
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
  return *keyboard_metadata_list;
}

const base::flat_map<VendorProductId, KeyboardMouseComboMetadata>&
GetKeyboardMouseComboMetadataList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, KeyboardMouseComboMetadata>>
      keyboard_mouse_combo_metadata_list({
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
  return *keyboard_mouse_combo_metadata_list;
}

const base::flat_map<VendorProductId, VendorProductId>& GetVidPidAliasList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, VendorProductId>>
      vid_pid_alias_list({
          // Logitech ERGO M575 (Bluetooth -> USB Dongle)
          {{0x46d, 0xb027}, {0x46d, 0x4096}},
          // Logitech MX Master 2S (Bluetooth -> USB Dongle)
          {{0x046d, 0xb019}, {0x046d, 0x4069}},
          // Logitech MX Master 3 (Bluetooth -> USB Dongle)
          {{0x046d, 0xb023}, {0x046d, 0x4082}},
          // Logitech M720 Triathlon (Bluetooth -> USB Dongle)
          {{0x046d, 0xb015}, {0x046d, 0x405e}},
          // Wacom Intuos S (Bluetooth -> USB)
          {{0x056a, 0x0376}, {0x056a, 0x0374}},
          // Wacom Intuos S (Bluetooth -> USB)
          {{0x056a, 0x03c5}, {0x056a, 0x0374}},
          // Wacom Intuos M (Bluetooth -> USB)
          {{0x056a, 0x0378}, {0x056a, 0x0375}},
          // Wacom Intuos M (Bluetooth -> USB)
          {{0x056a, 0x03c7}, {0x056a, 0x0375}},
          // SteelSeries Aerox 9 WL (USB Dongle -> USB)
          {{0x1038, 0x1858}, {0x1038, 0x185a}},
          // SteelSeries Aerox 9 WL (Bluetooth -> USB)
          {{0x0111, 0x185a}, {0x1038, 0x185a}},
          // Razer Naga Pro (Bluetooth -> USB Dongle)
          {{0x1532, 0x0092}, {0x1532, 0x0090}},
      });
  return *vid_pid_alias_list;
}

bool MouseMetadata::operator==(const MouseMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

bool GraphicsTabletMetadata::operator==(
    const GraphicsTabletMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

bool KeyboardMouseComboMetadata::operator==(
    const KeyboardMouseComboMetadata& other) const {
  return customization_restriction == other.customization_restriction;
}

const MouseMetadata* GetMouseMetadata(const ui::InputDevice& device) {
  VendorProductId vid_pid = {device.vendor_id, device.product_id};

  const auto alias_iter = GetVidPidAliasList().find(vid_pid);
  if (alias_iter != GetVidPidAliasList().end()) {
    vid_pid = alias_iter->second;
  }

  const auto iter = GetMouseMetadataList().find(vid_pid);
  if (iter != GetMouseMetadataList().end()) {
    return &(iter->second);
  }

  return nullptr;
}

const GraphicsTabletMetadata* GetGraphicsTabletMetadata(
    const ui::InputDevice& device) {
  VendorProductId vid_pid = {device.vendor_id, device.product_id};

  const auto alias_iter = GetVidPidAliasList().find(vid_pid);
  if (alias_iter != GetVidPidAliasList().end()) {
    vid_pid = alias_iter->second;
  }

  const auto iter = GetGraphicsTabletMetadataList().find(
      {vid_pid.vendor_id, vid_pid.product_id});
  if (iter != GetGraphicsTabletMetadataList().end()) {
    return &(iter->second);
  }

  return nullptr;
}

const KeyboardMetadata* GetKeyboardMetadata(const ui::InputDevice& device) {
  VendorProductId vid_pid = {device.vendor_id, device.product_id};

  const auto alias_iter = GetVidPidAliasList().find(vid_pid);
  if (alias_iter != GetVidPidAliasList().end()) {
    vid_pid = alias_iter->second;
  }

  const auto iter = GetKeyboardMetadataList().find(vid_pid);
  if (iter != GetKeyboardMetadataList().end()) {
    return &(iter->second);
  }

  return nullptr;
}

const KeyboardMouseComboMetadata* GetKeyboardMouseComboMetadata(
    const ui::InputDevice& device) {
  VendorProductId vid_pid = {device.vendor_id, device.product_id};

  const auto alias_iter = GetVidPidAliasList().find(vid_pid);
  if (alias_iter != GetVidPidAliasList().end()) {
    vid_pid = alias_iter->second;
  }

  const auto iter = GetKeyboardMouseComboMetadataList().find(vid_pid);
  if (iter != GetKeyboardMouseComboMetadataList().end()) {
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

std::vector<mojom::ButtonRemappingPtr> GetButtonRemappingListForConfig(
    mojom::MouseButtonConfig mouse_button_config) {
  switch (mouse_button_config) {
    case mojom::MouseButtonConfig::kNoConfig:
      return GetDefaultButtonRemappingList();
    case mojom::MouseButtonConfig::kFiveKey:
      return GetFiveKeyButtonRemappingList();
    case mojom::MouseButtonConfig::kLogitechSixKey:
      return GetLogitechSixKeyButtonRemappingList();
    case mojom::MouseButtonConfig::kLogitechSixKeyWithTab:
      return GetLogitechSixKeyWithTabButtonRemappingList();
  }
}

std::vector<mojom::ButtonRemappingPtr> GetPenButtonRemappingListForConfig(
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config) {
  switch (graphics_tablet_button_config) {
    case mojom::GraphicsTabletButtonConfig::kNoConfig:
      return GetDefaultButtonRemappingList();
    case mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly:
    case mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons:
      return GetWacomStandardPenButtonRemappingList();
  }
}

std::vector<mojom::ButtonRemappingPtr> GetTabletButtonRemappingListForConfig(
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config) {
  switch (graphics_tablet_button_config) {
    case mojom::GraphicsTabletButtonConfig::kNoConfig:
    case mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly:
      return GetDefaultButtonRemappingList();
    case mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons:
      return GetWacomStandardFourButtonRemappingList();
  }
}

}  // namespace ash
