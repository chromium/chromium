// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata.h"

#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"

namespace ash {

namespace {

std::vector<mojom::ButtonRemappingPtr> GetDefaultButtonRemappingList() {
  return {};
}

std::vector<mojom::ButtonRemappingPtr> GetThreeKeyButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      /*remapping_action=*/nullptr));
  return array;
}

std::vector<mojom::ButtonRemappingPtr>
GetFourKeyWithTopButtonButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_MIDDLE_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      /*remapping_action=*/nullptr));
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_TOP_BUTTON_DEFAULT_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      /*remapping_action=*/nullptr));
  return array;
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
GetWacomStandardPenOneButtonRemappingList() {
  std::vector<mojom::ButtonRemappingPtr> array;
  array.push_back(mojom::ButtonRemapping::New(
      /*name=*/l10n_util::GetStringUTF8(
          IDS_SETTINGS_CUSTOMIZATION_PEN_FRONT_BUTTON_NAME),
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kRight),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kRightClick)));
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

MouseMetadata::MouseMetadata() = default;
MouseMetadata::~MouseMetadata() = default;
MouseMetadata::MouseMetadata(
    mojom::CustomizationRestriction customization_restriction,
    mojom::MouseButtonConfig config,
    std::optional<std::string> name)
    : customization_restriction(customization_restriction),
      mouse_button_config(config),
      name(name) {}
MouseMetadata::MouseMetadata(const MouseMetadata& other) = default;

GraphicsTabletMetadata::GraphicsTabletMetadata() = default;
GraphicsTabletMetadata::~GraphicsTabletMetadata() = default;
GraphicsTabletMetadata::GraphicsTabletMetadata(
    const GraphicsTabletMetadata& other) = default;
GraphicsTabletMetadata::GraphicsTabletMetadata(
    mojom::CustomizationRestriction restriction,
    mojom::GraphicsTabletButtonConfig config,
    std::optional<std::string> name)
    : customization_restriction(restriction),
      graphics_tablet_button_config(config),
      name(name) {}

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
            mojom::MouseButtonConfig::kNoConfig, "M720 Triathlon"}},
          // Logitech MX Anywhere 2S (USB Dongle)
          {{0x046d, 0x406a},
           {mojom::CustomizationRestriction::
                kAllowHorizontalScrollWheelRewrites,
            mojom::MouseButtonConfig::kNoConfig, "MX Anywhere 2S"}},
          // Logitech MX Ergo Trackball (USB Dongle)
          {{0x046d, 0x406f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "Logitech MX Ergo Trackball"}},
          // Logitech MX Master 2S (USB Dongle)
          {{0x046d, 0x4069},
           {mojom::CustomizationRestriction::kAllowTabEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKeyWithTab, "MX Master 2S"}},
          // Logitech Pebble M350 (USB Dongle)
          {{0x046d, 0x4080},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kThreeKey, "Pebble M350"}},
          // Logitech MX Master 3 (USB Dongle)
          {{0x046d, 0x4082},
           {mojom::CustomizationRestriction::kAllowTabEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKeyWithTab, "MX Master 3"}},
          // Logitech MX Anywhere 3 (USB Dongle)
          {{0x046d, 0x4090},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "MX Anywhere 3"}},
          // Logitech ERGO M575 (USB Dongle)
          {{0x046d, 0x4096},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "ERGO M575"}},
          // Logitech M510 (USB Dongle)
          {{0x046d, 0x4051},
           {mojom::CustomizationRestriction::
                kAllowHorizontalScrollWheelRewrites,
            mojom::MouseButtonConfig::kNoConfig, "M510"}},
          // HP 690/695 Mouse
          {{0x03f0, 0x804a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech M650 L
          {{0x046d, 0xb02a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "M650 L"}},
          // Logitech M550
          {{0x046d, 0xb02b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kThreeKey, "M550"}},
          // Logitech Pop Mouse
          {{0x046d, 0xb030},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFourKeyWithTopButton, "Pop Mouse"}},
          // Logitech Lift
          {{0x046d, 0xb031},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "Lift"}},
          // Logitech M650 For Business
          {{0x046d, 0xb032},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "M650 For Business"}},
          // Logitech MX Master 3S (Bluetooth)
          {{0x046d, 0xb034},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKey, "MX Master 3S"}},
          // Logitech MX Master 3S For Business (Bluetooth)
          {{0x046d, 0xb035},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kLogitechSixKey,
            "MX Master 3S For Business"}},
          // Logitech Pebble 2 M350S
          {{0x046d, 0xb036},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kThreeKey, "Pebble 2 M350S"}},
          // Logitech MX Anywhere 3S (Bluetooth)
          {{0x046d, 0xb037},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "MX Anywhere 3S"}},
          // Logitech M240 Silent
          {{0x046d, 0xb03a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kThreeKey, "M240 Silent"}},
          // Logitech MX Ergo S Trackball
          {{0x046d, 0xb03e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "MX Ergo S Trackball"}},
          // Logitech Signature AI Edition M750
          {{0x046d, 0xb040},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "Signature AI Edition M750"}},
          // Logitech M650 For Business
          {{0x046d, 0xb032},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey, "M650 For Business"}},
          // Logitech M500 (USB)
          {{0x046d, 0xc069},
           {mojom::CustomizationRestriction::
                kAllowHorizontalScrollWheelRewrites,
            mojom::MouseButtonConfig::kNoConfig, "M500"}},
          // Redragon M811 Aatrox MMO
          {{0x04d9, 0xfc6d},
           {mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Rival 3
          {{0x1038, 0x1824},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Rival 3"}},
          // Rival 3 WL
          {{0x1038, 0x1830},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Rival 3 WL"}},
          // SteelSeries Aerox 3
          {{0x1038, 0x1836},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Aerox 3"}},
          // SteelSeries Aerox 3 WL
          {{0x1038, 0x1838},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Aerox 3 WL"}},
          // SteelSeries Aerox 5
          {{0x1038, 0x1850},
           {mojom::CustomizationRestriction::kAllowFKeyRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Aerox 5"}},
          // SteelSeries Aerox 5 WL (USB)
          {{0x1038, 0x1852},
           {mojom::CustomizationRestriction::kAllowFKeyRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Aerox 5 WL"}},
          // SteelSeries Aerox 9 WL (USB)
          {{0x1038, 0x185a},
           {mojom::CustomizationRestriction::
                kAllowAlphabetOrNumberKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig, "Aerox 9 WL"}},
          // Razer Naga Pro (USB Dongle)
          {{0x1532, 0x0090},
           {mojom::CustomizationRestriction::
                kAllowAlphabetOrNumberKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          /////////////////////////////////
          // Below is data for imposter devices, and is not official metadata.
          /////////////////////////////////
          // HP HyperX Pulsefire Haste Wireless
          {{0x03f0, 0x028e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HP HyperX Pulsefire Core
          {{0x03f0, 0x0d8f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HP HyperX Pulsefire Haste
          {{0x03f0, 0x0f8f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HP HyperX Pulsefire Haste 2 Wireless
          {{0x03f0, 0x0f98},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Genius Wireless Mouse
          {{0x0458, 0x0189},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Arc Touch Mouse SE
          {{0x045e, 0x07f3},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Microsoft USB Precision Mouse
          {{0x045e, 0x0822},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Acer 2.4G Device
          {{0x0461, 0x4e9a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HP 910 Mouse
          {{0x0461, 0x4eef},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Cube
          {{0x046d, 0x4010},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech M545
          {{0x046d, 0x4028},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G602
          {{0x046d, 0x402c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Dell WM324
          {{0x046d, 0x4030},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX Master
          {{0x046d, 0x4041},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G603
          {{0x046d, 0x406c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G703
          {{0x046d, 0x4070},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G304
          {{0x046d, 0x4074},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G Pro
          {{0x046d, 0x4079},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX Vertical
          {{0x046d, 0x407b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kFiveKey}},
          // Logitech G604
          {{0x046d, 0x4085},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G703 LS
          {{0x046d, 0x4086},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G903 LS
          {{0x046d, 0x4087},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech M557
          {{0x046d, 0xb010},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX Anywhere 2
          {{0x046d, 0xb018},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G9 Laser Mouse
          {{0x046d, 0xc048},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G500
          {{0x046d, 0xc068},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Gaming Mouse G502
          {{0x046d, 0xc07d},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Gaming Mouse G402
          {{0x046d, 0xc07e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Gaming Mouse G302
          {{0x046d, 0xc07f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Gaming Mouse G303
          {{0x046d, 0xc080},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G403 Prodigy Wired/Wireless Gaming Mouse
          {{0x046d, 0xc082},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G102 Prodigy Gaming Mouse
          {{0x046d, 0xc084},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G Pro Gaming Mouse
          {{0x046d, 0xc085},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX Vertical Advanced Ergonomic Mouse
          {{0x046d, 0xc08a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech MX518 Gaming Mouse
          {{0x046d, 0xc08e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G403 HERO Gaming Mouse
          {{0x046d, 0xc08f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G703 LIGHTSPEED Wireless Gaming Mouse
          {{0x046d, 0xc090},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G502 X
          {{0x046d, 0xc099},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G300s Optical Gaming Mouse
          {{0x046d, 0xc246},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech Gaming Mouse G600
          {{0x046d, 0xc24a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech G500s Laser Gaming Mouse
          {{0x046d, 0xc24e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Chicony NEC Laser Mouse
          {{0x04f2, 0x1218},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // NEC Bluetooth Mouse
          {{0x04f2, 0x13ee},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ELECOM MMO Mouse
          {{0x056e, 0x00e7},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ELECOM DEFT Pro TrackBall
          {{0x056e, 0x0131},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MosArt 2.4G Wireless Mouse
          {{0x062a, 0x4108},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MosArt 2.4G Full-Speed Mouse
          {{0x062a, 0x41cf},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MosArt 2.4G Mouse
          {{0x062a, 0x636a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder V2 X
          {{0x068e, 0x009d},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk V3 Pro
          {{0x068e, 0x00ac},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // PixArt Gaming Mouse
          {{0x093a, 0x2532},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // J.Burrows JBBTMSLIM
          {{0x093a, 0x2801},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // PixArt 2.4G Wireless Mouse
          {{0x093a, 0x3701},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Glorious Model O 2 Wireless
          {{0x093a, 0x822d},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HyperX Pulsefire FPS Pro
          {{0x0951, 0x16d7},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // HyperX Pulsefire Raid
          {{0x0951, 0x16e4},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ASUS WT300
          {{0x0b05, 0x185f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ASUS TUF Gaming M5
          {{0x0b05, 0x1898},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ASUS TUF Gaming M4 Wireless
          {{0x0b05, 0x19f4},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Contour Design RollerMouse Free 3
          {{0x0b33, 0x0404},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Contour Design UNIMOUSE
          {{0x0b33, 0x1055},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MSI Clutch GM40 GAMING Mouse
          {{0x0d22, 0x0d40},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MSI Clutch GM08 OpticalMouse
          {{0x0db0, 0x0d08},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MSI Clutch GM11 Gaming Mouse
          {{0x0db0, 0x0d11},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MSI Clutch GM41 Light Weight Wireless Gaming Mouse
          {{0x0db0, 0x0d4b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Turtle Beach Grip 500 Mouse
          {{0x10f5, 0x0600},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Logitech M650 Mouse
          {{0x1235, 0xaa22},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // SOAI USB Gaming Mouse
          {{0x12c9, 0x1018},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Newmen USB Gaming Mouse
          {{0x12c9, 0x1027},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // COOLER MASTER CM STORM INFERNO GAMING MOUSE
          {{0x12cf, 0x0186},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Mad Catz RAT 8+ gaming mouse
          {{0x12cf, 0x0c05},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MosArt 2.4G Speed Mouse
          {{0x145f, 0x01c1},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // YVI Wireless Mouse
          {{0x145f, 0x0252},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // GXT 133 Gaming Mouse
          {{0x145f, 0x026e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // SSIF GXT 960 Gaming Mouse
          {{0x145f, 0x02b6},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // JLab GO Mouse
          {{0x145f, 0x02fc},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga
          {{0x1532, 0x0015},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Mamba
          {{0x1532, 0x0024},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Taipan
          {{0x1532, 0x0034},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder 2013
          {{0x1532, 0x0037},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder 1800
          {{0x1532, 0x0038},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga 2014
          {{0x1532, 0x0040},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder Chroma
          {{0x1532, 0x0043},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Mamba Tournament Edition
          {{0x1532, 0x0046},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Diamondback
          {{0x1532, 0x004c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Lancehead Tournament Edition
          {{0x1532, 0x0060},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk
          {{0x1532, 0x0064},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk Essential
          {{0x1532, 0x0065},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga Trinity
          {{0x1532, 0x0067},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Mamba Elite
          {{0x1532, 0x006c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder Essential
          {{0x1532, 0x006e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Pro Click
          {{0x1532, 0x0076},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder V2 Pro
          {{0x1532, 0x007c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk X HyperSpeed
          {{0x1532, 0x0082},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder V2
          {{0x1532, 0x0084},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder V2 Mini
          {{0x1532, 0x008c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Viper
          {{0x1532, 0x0091},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Orochi V2
          {{0x1532, 0x0095},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga X
          {{0x1532, 0x0096},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk V3
          {{0x1532, 0x0099},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer DeathAdder V2 X HyperSpeed
          {{0x1532, 0x009c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Cobra
          {{0x1532, 0x00a3},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Viper V2 Pro
          {{0x1532, 0x00a6},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga V2 Pro
          {{0x1532, 0x00a8},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Naga V2 HyperSpeed
          {{0x1532, 0x00b4},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Razer Basilisk V3 X HyperSpeed
          {{0x1532, 0x00b9},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Lenovo Multi-function Mouse M300
          {{0x17ef, 0x6054},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x18f8, 0x0f97},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x18f8, 0x0f99},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Gaming Mouse (Unknown Brand)
          {{0x18f8, 0x0fc0},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x18f8, 0x1286},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Gaming Mouse (Unknown Brand)
          {{0x18f8, 0x1686},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // EVGA X20 Gaming Mouse
          {{0x1915, 0xeeee},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair M65 Gaming Mouse
          {{0x1b1c, 0x1b05},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair Gaming SCIMITAR PRO RGB Mouse
          {{0x1b1c, 0x1b3e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair M65 RGB ELITE Gaming Mouse
          {{0x1b1c, 0x1b5a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair M55 RGB PRO Gaming Mouse
          {{0x1b1c, 0x1b70},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair SABRE RGB PRO Gaming Mouse
          {{0x1b1c, 0x1b79},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair SCIMITAR RGB ELITE Gaming Mouse
          {{0x1b1c, 0x1b8b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Corsair Dark Core RGB Pro SE
          {{0x1b7e, 0x1b1c},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x1bcf, 0x0053},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // SOAI Gaming Mouse
          {{0x1bcf, 0x08b9},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // 2.4G Wireless Optical Mouse  (Unknown Brand)
          {{0x1d57, 0x130f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // JLab Epic Keys
          {{0x1d57, 0xfa60},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ROCCAT Ryos MK Pro
          {{0x1e7d, 0x3232},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Cooler Master Storm Mizar Mouse
          {{0x2516, 0x001f},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Cooler Master MM711 Gaming Mouse
          {{0x2516, 0x0101},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Glorious Model O Wireless
          {{0x258a, 0x2011},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Compx 2.4G Dual Mode Mouse
          {{0x25a7, 0xfa08},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // CX Gaming Mouse
          {{0x25a7, 0xfa68},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // PERIMICE-520
          {{0x260d, 0x1019},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Thermaltake Level 20 Mouse - Hatsune Miku Edition
          {{0x264a, 0x1024},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // MosArt Mi Wireless Mouse Lite
          {{0x2717, 0x5016},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // UGREEN Mouse
          {{0x2b89, 0x6209},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // UGREEN BLE Mouse
          {{0x2b89, 0x6621},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Xtrfy M42
          {{0x2ea8, 0x2203},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x30fa, 0x1040},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x30fa, 0x1140},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x30fa, 0x1340},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // ECURS GM003
          {{0x30fa, 0x1440},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x30fa, 0x1540},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Instant USB Gaming Mouse (Unknown Brand)
          {{0x30fa, 0x1701},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Evision Gaming
          {{0x320f, 0x507a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Evision RGB
          {{0x320f, 0x5080},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // EVISION USB-STDHID
          {{0x320f, 0x50ed},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // BT5.0 Mouse (Unknown Brand)
          {{0x32c2, 0x0001},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // BT5.1 Mouse (Unknown Brand)
          {{0x32c2, 0x6621},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // EVGA X12 Gaming Mouse
          {{0x3842, 0x2422},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // Blackweb Gaming Mouse
          {{0x3938, 0x1093},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // onn. OpticalMouse
          {{0x3938, 0x1193},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // onn. Gaming Optical Mouse
          {{0x3938, 0x1210},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // onn. Mouse
          {{0x3938, 0x1215},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // onn. Wireless Gaming Mouse
          {{0x3938, 0x1254},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // onn. Gaming Mouse
          {{0x3938, 0x1313},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x4423, 0x0001},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x4e53, 0x5406},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::MouseButtonConfig::kNoConfig}},
          // USB Optical Mouse (Unknown Brand)
          {{0x4e53, 0x5407},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
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
          // Wacom One Pen Tablet S
          {{0x0531, 0x0100},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnlyOneButton,
            "Wacom One Pen Tablet S"}},
          // Wacom One Pen tablet M
          {{0x0531, 0x0102},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnlyOneButton,
            "Wacom One Pen tablet M"}},
          // One by Wacom S
          {{0x056a, 0x037a},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly,
            "One by Wacom S"}},
          // One by Wacom M
          {{0x056a, 0x0301},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly,
            "One by Wacom M"}},
          // Wacom One Pen Display 11
          {{0x056a, 0x03Ce},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly,
            "Wacom One Pen Display 11"}},
          // Wacom One Pen Display 13 Touch
          {{0x056a, 0x03Cb},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly,
            "Wacom One Pen Display 13 Touch"}},
          // Wacom Intuos S
          {{0x056a, 0x0374},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons,
            "Wacom Intuos S"}},
          // Wacom Intuos M
          {{0x056a, 0x0375},
           {mojom::CustomizationRestriction::kAllowCustomizations,
            mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons,
            "Wacom Intuos M"}},
          /////////////////////////////////
          // Below is data for imposter devices, and is not official metadata.
          /////////////////////////////////
          // HUION Inspiroy H420 Tablet
          {{0x256c, 0x006e},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::GraphicsTabletButtonConfig::kNoConfig}},
          // UGEE S640W Tablet
          {{0x28bd, 0x0913},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::GraphicsTabletButtonConfig::kNoConfig}},
          // UGEE M708 Graphics Tablet
          {{0x28bd, 0x0924},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::GraphicsTabletButtonConfig::kNoConfig}},
          // UGEE S640 Tablet
          {{0x28bd, 0x0937},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites,
            mojom::GraphicsTabletButtonConfig::kNoConfig}},
      });
  return *graphics_tablet_metadata_list;
}

const base::flat_map<VendorProductId, KeyboardMetadata>&
GetKeyboardMetadataList() {
  const static base::NoDestructor<
      base::flat_map<VendorProductId, KeyboardMetadata>>
      keyboard_metadata_list({
          // HP OMEN Sequencer
          {{0x03f0, 0x1f41}, {}},
          // Microsoft Ergonomic Keyboard
          {{0x045e, 0x082c}, {}},
          // Logitech ERGO K860 (Bluetooth)
          {{0x046d, 0x4088}, {}},
          // Logitech MX Keys (Universal Receiver)
          {{0x046d, 0x408a}, {}},
          // Logitech Craft Keyboard
          {{0x046d, 0xb350}, {}},
          // Logitech ERGO K860
          {{0x046d, 0xb359}, {}},
          // Logitech MX Keys (Bluetooth)
          {{0x046d, 0xb35b}, {}},
          // Logitech G915 TKL (Bluetooth)
          {{0x046d, 0xb35f}, {}},
          // Logitech MX Keys for Mac (Bluetooth)
          {{0x046d, 0xb361}, {}},
          // Logitech ERGO 860B
          {{0x046d, 0xb364}, {}},
          // Logitech G213
          {{0x046d, 0xc336}, {}},
          // Logitech G815 RGB
          {{0x046d, 0xc33f}, {}},
          // Logitech G915 TKL (USB)
          {{0x046d, 0xc343}, {}},
          // EGA MGK2 (Bluetooth) + Keychron K2
          {{0x05ac, 0x024f}, {}},
          // EGA MGK2 (USB)
          {{0x05ac, 0x0256}, {}},
          // HyperX Alloy Origins
          {{0x0951, 0x16e5}, {}},
          // HyperX Alloy Origins Core
          {{0x0951, 0x16e6}, {}},
          // SteelSeries Apex 7
          {{0x1038, 0x1612}, {}},
          // SteelSeries Apex 3 TKL
          {{0x1065, 0x0002}, {}},
          // Razer Cynosa Chroma
          {{0x1532, 0x022a}, {}},
          // Razer Cynosa V2
          {{0x1532, 0x025e}, {}},
          // Razer Huntsman V2 Tenkeyless
          {{0x1532, 0x026b}, {}},
          // Razer Huntsman Elite
          {{0x1535, 0x0046}, {}},
          // Google Meet Series One Remote GPJ50L
          {{0x18d1, 0x8003}, {}},
          // Corsair Gaming K95 RGB Platinum
          {{0x1b1c, 0x1b2d}, {}},
          // G.Skill KM780
          {{0x28da, 0x1101}, {}},
          // Kinesis Freestyle Edge RGB
          {{0x29ea, 0x0102}, {}},
          // Durgod Taurus K320
          {{0x2f68, 0x0082}, {}},
          // Glorious GMMK Pro
          {{0x320f, 0x5044}, {}},
          // ZSA Moonlander Mark I
          {{0x3297, 0x1969}, {}},
          // ErgoDox EZ
          {{0x3297, 0x4974}, {}},
          // ErgoDox EZ Glow
          {{0x3297, 0x4976}, {}},
          // Keychron Q3
          {{0x3434, 0x0121}, {}},
          // Keychron Q5
          {{0x3434, 0x0151}, {}},
          // Keychron Q6
          {{0x3434, 0x0163}, {}},
          // Keychron Q10
          {{0x3434, 0x01a1}, {}},
          // Keychron V1
          {{0x3434, 0x0311}, {}},
          // Keyboardio Model 100
          {{0x3496, 0x0006}, {}},
          // LazyDesigners Dimple
          {{0x4c44, 0x0040}, {}},
          // ErgoDox EZ
          {{0xfeed, 0x1307}, {}},
          /////////////////////////////////
          // Below is data for imposter devices, and is not official metadata.
          /////////////////////////////////
          // HyperX Alloy Origins 65
          {{0x03f0, 0x038f}, {}},
          // HyperX Alloy Origins Core
          {{0x03f0, 0x098f}, {}},
          // HyperX Alloy Origins 60
          {{0x03f0, 0x0c8e}, {}},
          // Generic USB Keyboard
          {{0x040b, 0x2000}, {}},
          // Ducky One2 SF RGB
          {{0x0416, 0x0123}, {}},
          // Microsoft Surface Keyboard
          {{0x045e, 0x0922}, {}},
          // Logitech G915 Keyboard
          {{0x046d, 0xb354}, {}},
          // RK Royal Kludge RKM75 Keyboard
          {{0x046d, 0xb35a}, {}},
          // Logitech MX Keys Business Keyboard
          {{0x046d, 0xb363}, {}},
          // Logitech G915 Wireless
          {{0x046d, 0xc33e}, {}},
          // Lily58 Split Keyboard
          {{0x04d8, 0xeb2d}, {}},
          // Ducky Keyboard (Unknown Model)
          {{0x04d9, 0x0356}, {}},
          // Blackview K1 Keyboard
          {{0x04e8, 0x7021}, {}},
          // ELECOM TK-FDP098
          {{0x056e, 0x1064}, {}},
          // ELECOM TK-FDP099
          {{0x056e, 0x1065}, {}},
          // ELECOM TK-FDM063
          {{0x056e, 0x1084}, {}},
          // Keychron K8
          {{0x05ac, 0x0250}, {}},
          // RISE KBBTC01-WH
          {{0x05ac, 0x0257}, {}},
          // P. I. Engineering XK-80 HID
          {{0x05f3, 0x0441}, {}},
          // HyperX Alloy FPS Mechanical Gaming Keyboard
          {{0x0951, 0x16b7}, {}},
          // HyperX Alloy Elite RGB
          {{0x0951, 0x16be}, {}},
          // HyperX Alloy Core RGB
          {{0x0951, 0x16dd}, {}},
          // HyperX Alloy Elite 2
          {{0x0951, 0x1711}, {}},
          // ASUS ROG Falchion
          {{0x0b05, 0x193e}, {}},
          // Asus ROG Strix Scope RX TKL Wireless
          {{0x0b05, 0x1a07}, {}},
          // SteelSeries Apex 3
          {{0x1038, 0x161a}, {}},
          // SteelSeries Apex 5
          {{0x1038, 0x161c}, {}},
          // SteelSeries Apex 3 TKL
          {{0x1038, 0x1622}, {}},
          // Dygma Raise Keyboard
          {{0x1209, 0x2201}, {}},
          // Keyboardio Atreus
          {{0x1209, 0x2303}, {}},
          // GXT 865 Gaming keyboard
          {{0x145f, 0x0250}, {}},
          // Razer BlackWidow Ultimate 2016
          {{0x1532, 0x0214}, {}},
          // Razer Ornata Chroma
          {{0x1532, 0x021e}, {}},
          // Razer Huntsman Elite
          {{0x1532, 0x0226}, {}},
          // Razer BlackWidow Elite
          {{0x1532, 0x0228}, {}},
          // Razer BlackWidow Lite
          {{0x1532, 0x0235}, {}},
          // Razer BlackWidow Keyboard
          {{0x1532, 0x0241}, {}},
          // Razer Pro Type Keyboard
          {{0x1532, 0x0249}, {}},
          // Razer BlackWidow V3 Keyboard
          {{0x1532, 0x024e}, {}},
          // Razer Huntsman Mini
          {{0x1532, 0x0257}, {}},
          // Razer Ornata V2
          {{0x1532, 0x025d}, {}},
          // Razer BlackWidow V3 Mini Keyboard
          {{0x1532, 0x0271}, {}},
          // Razer BlackWidow V4 Keyboard
          {{0x1532, 0x0287}, {}},
          // Razer Ornata V3 Keyboard
          {{0x1532, 0x028f}, {}},
          // Razer Ornata V3 X
          {{0x1532, 0x02a2}, {}},
          // Razer Ornata V3 Tenkeyless
          {{0x1532, 0x02a3}, {}},
          // MoErgo Glove80 Left Keyboard
          {{0x16c0, 0x27db}, {}},
          // Lenovo ThinkPad 10 Ultrabook Keyboard
          {{0x17ef, 0x6062}, {}},
          // Lenovo Legion K300 RGB Gaming Keyboard
          {{0x17ef, 0x60f0}, {}},
          // NuPhy Air75 V2 Keyboard
          {{0x19f5, 0x3245}, {}},
          // Corsair K70 RGB MK.2 Mechanical Gaming Keyboard
          {{0x1b1c, 0x1b49}, {}},
          // Corsair K65 RGB MINI 60% Mechanical Gaming Keyboard
          {{0x1b1c, 0x1bbd}, {}},
          // Corsair K60 PRO TKL RGB Optical-Mechanical Gaming Keyboard
          {{0x1b1c, 0x1bc7}, {}},
          // Satechi wired keyboard
          {{0x1c4f, 0x0063}, {}},
          // ROCCAT Kova+
          {{0x1e7d, 0x2d50}, {}},
          // ROCCAT Vulcan AIMO
          {{0x1e7d, 0x3098}, {}},
          // Macally ACEKEY USB KEYBOARD
          {{0x2222, 0x0032}, {}},
          // MosArt RF Wireless PC Keyboard
          {{0x2222, 0x0068}, {}},
          // Cooler Master Gaming MECH KB Keyboard
          {{0x2516, 0x007f}, {}},
          // Cooler Master MK730 Keyboard
          {{0x2516, 0x008f}, {}},
          // Sino Wealth Gaming KB
          {{0x258a, 0x002a}, {}},
          // Sino Wealth 61 Keyboard
          {{0x258a, 0x013b}, {}},
          // Kinesis Kinesis Adv360
          {{0x29ea, 0x0360}, {}},
          // Kinesis Kinesis Freestyle2 MAC - KB800 Keyboard
          {{0x29ea, 0x800b}, {}},
          // Input Club Keyboard Kira PixelMap USB
          {{0x308f, 0x0013}, {}},
          // Input Club Keyboard Infinity Ergodox PixelMap USB
          {{0x308f, 0x0025}, {}},
          // TIETI B-2 Spirit Mini Wired
          {{0x3151, 0x4015}, {}},
          // Wooting 60HE (ARM)
          {{0x31e3, 0x1310}, {}},
          // SONiX MONTECH MK108
          {{0x320f, 0x50b7}, {}},
          // Ducky One 3 SF RGB
          {{0x3233, 0x5311}, {}},
          // Ducky One2 Mini RGB
          {{0x3233, 0x6301}, {}},
          // Ducky One 3 Mini RGB
          {{0x3233, 0x6311}, {}},
          // ZSA Voyager Keyboard
          {{0x3297, 0x1977}, {}},
          // ZSA ErgoDox EZ Shine
          {{0x3297, 0x4975}, {}},
          // Keychron Q11 Keyboard
          {{0x3434, 0x01e0}, {}},
          // Keychron K2 Pro Keyboard
          {{0x3434, 0x0223}, {}},
          // Keychron K5 Pro Keyboard
          {{0x3434, 0x0253}, {}},
          // Keychron K7 Pro Keyboard
          {{0x3434, 0x0271}, {}},
          // Keychron K10 Pro Keyboard
          {{0x3434, 0x02a0}, {}},
          // Keychron V3 Keyboard
          {{0x3434, 0x0330}, {}},
          // Keychron C3 Pro Keyboard
          {{0x3434, 0x0430}, {}},
          // Keychron Q10 Pro Keyboard
          {{0x3434, 0x06a0}, {}},
          // Geeky GK65 gaming Keyboard
          {{0x3532, 0xc0c2}, {}},
          // EVGA Z15 RGB Gaming Keyboard
          {{0x3842, 0x2608}, {}},
          // Blackweb Gaming Keyboard
          {{0x3938, 0x1095}, {}},
          // onn. Mechanical Gaming Keyboard
          {{0x3938, 0x1205}, {}},
          // Dell Wired Multimedia Keyboard
          {{0x413c, 0x2110}, {}},
          // DZ65 RGB V3 Hot-Swap RGB
          {{0x445a, 0x1424}, {}},
          // Tofu Jr
          {{0x445a, 0x1426}, {}},
          // foostan Corne Keyboard
          {{0x4653, 0x0001}, {}},
          // YMDK YMD09
          {{0x594d, 0x4409}, {}},
          // Yowkees Keyball61
          {{0x5957, 0x0100}, {}},
          // SayoDevice M3K RGB Keyboard
          {{0x8089, 0x0003}, {}},
          // TheVan Keyboard MiniVan
          {{0xfeae, 0x8847}, {}},
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
          /////////////////////////////////
          // Below is data from indirectly identified combo devices.
          /////////////////////////////////
          // HP Wireless Keyboard Combo 200
          {{0x03f0, 0x1941},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP Wireless Slim Keyboard - Skylab
          {{0x03f0, 0x194a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP Wireless Slim Keyboard - Skylab EU
          {{0x03f0, 0x1a4a},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP Wireless Keyboard and Mouse
          {{0x03f0, 0x2641},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP 510 Wireless KBMS Combo
          {{0x03f0, 0x4efd},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP 330 Wireless Mouse and Keyboard Combo
          {{0x03f0, 0x6341},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Microsoft Wireless Entertainment Keyboard 7000
          {{0x045e, 0x0705},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Microsoft Surface Cover
          {{0x045e, 0x07dc},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // HP 710 Wireless KB MS Combo
          {{0x0461, 0x4ef1},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Logitech K700 Wireless Keyboard Controller
          {{0x046d, 0x2012},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Logitech K400 keyboard
          {{0x046d, 0x404b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Logitech K400 Pro keyboard
          {{0x046d, 0x4068},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Logitech K600 Wireless Keyboard Controller
          {{0x046d, 0x4078},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Chicony Fujitsu Slim Keyboard with Touchpad
          {{0x04f2, 0x1322},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // ELECOM TK-TB01DMBK
          {{0x056e, 0x1077},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // MosArt Wireless Keyboard and Mouse
          {{0x062a, 0x0102},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x261b},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Newmen 2.4G Keyboard Mouse
          {{0x062a, 0x4101},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Lenovo Essential Wireless Keyboard and Mouse Combo
          {{0x17ef, 0x60a9},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Lenovo EOS Wireless Keyboard and Mouse
          {{0x1a81, 0x1021},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Brydge 12.3 Pro+
          {{0x3175, 0xa001},
           {mojom::CustomizationRestriction::kDisableKeyEventRewrites}},
          // Dell KM632 Wireless Keyboard and Mouse
          {{0x413c, 0x2501},
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
          // Logitech MX Anywhere 2S (Bluetooth -> USB Dongle)
          {{0x046d, 0xb01a}, {0x046d, 0x406a}},
          // Logitech MX Ergo Trackball (Bluetooth -> USB Dongle)
          {{0x046d, 0xb01d}, {0x046d, 0x406f}},
          // Logitech MX Vertical (Bluetooth -> USB Dongle)
          {{0x046d, 0xb020}, {0x046d, 0x407b}},
          // Logitech Pebble M350 (Bluetooth -> USB Dongle)
          {{0x046d, 0xb021}, {0x046d, 0x4080}},
          // Logitech MX Master 3 (Bluetooth -> USB Dongle)
          {{0x046d, 0xb023}, {0x046d, 0x4082}},
          // Logitech MX Anywhere 3 (Bluetooth -> USB Dongle)
          {{0x046d, 0xb025}, {0x046d, 0x4090}},
          // Logitech MX Anywhere 3 For Business (Bluetooth -> USB Dongle)
          {{0x046d, 0xb02d}, {0x046d, 0x4090}},
          // Logitech Lift For Business (Bluetooth -> Bluetooth)
          {{0x046d, 0xb033}, {0x046d, 0xb031}},
          // Logitech MX Anywhere 3S For Business (Bluetooth -> Bluetooth)
          {{0x046d, 0xb038}, {0x046d, 0xb037}},
          // Logitech M240 Silent For Business (Bluetooth -> Bluetooth)
          {{0x046d, 0xb03b}, {0x046d, 0xb03a}},
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
          /////////////////////////////////
          // Below is data for imposter devices, and is not official metadata.
          /////////////////////////////////
          // Logitech MX Master
          {{0x046d, 0x4060}, {0x046d, 0x4041}},
          // Logitech MX Master
          {{0x046d, 0x4071}, {0x046d, 0x4041}},
          // Logitech Gaming Mouse G502
          {{0x046d, 0xc332}, {0x046d, 0xc07d}},
          // NEC Bluetooth Mouse
          {{0x04f2, 0x2022}, {0x04f2, 0x13ee}},
          // MosArt 2.4G Mouse
          {{0x062a, 0x650a}, {0x062a, 0x636a}},
          // Razer Basilisk V3 Pro
          {{0x1532, 0x00ab}, {0x068e, 0x00ac}},
          // HyperX Pulsefire FPS Pro
          {{0x0951, 0x1729}, {0x0951, 0x16d7}},
          // Razer Naga
          {{0x1532, 0x002e}, {0x1532, 0x0015}},
          // Razer DeathAdder V2 Pro
          {{0x1532, 0x008e}, {0x1532, 0x007c}},
          // Corsair Dark Core RGB Pro SE
          {{0x1b80, 0x1b1c}, {0x1b7e, 0x1b1c}},
          // Compx 2.4G Dual Mode Mouse
          {{0x25a7, 0xfa7f}, {0x25a7, 0xfa08}},
          // UGREEN Mouse
          {{0x2b89, 0x6210}, {0x2b89, 0x6209}},
          // Blackweb Gaming Mouse
          {{0x3938, 0x1240}, {0x3938, 0x1093}},
          // Microsoft Surface Keyboard
          {{0x045e, 0x09b5}, {0x045e, 0x0922}},
          // Sino Wealth Gaming KB
          {{0x258a, 0x0049}, {0x258a, 0x002a}},
          // GMMK Pro Keyboard
          {{0x320f, 0x5046}, {0x320f, 0x5044}},
          // GMMK Pro Keyboard
          {{0x320f, 0x5092}, {0x320f, 0x5044}},
          // Keychron V3 Keyboard
          {{0x3434, 0x0331}, {0x3434, 0x0330}},
          // onn. Mechanical Gaming Keyboard
          {{0x3938, 0x1269}, {0x3938, 0x1205}},
          // Logitech Craft keyboard
          {{0x046d, 0x4066}, {0x046d, 0xb350}},
          // Microsoft Surface Cover
          {{0x045e, 0x07e2}, {0x045e, 0x07dc}},
          // Microsoft Surface Cover
          {{0x045e, 0x07e8}, {0x045e, 0x07dc}},
          // Microsoft Surface Cover
          {{0x045e, 0x07e9}, {0x045e, 0x07dc}},
          // Microsoft Surface Cover
          {{0x045e, 0x096f}, {0x045e, 0x07dc}},
          // Microsoft Surface Cover
          {{0x045e, 0x09c0}, {0x045e, 0x07dc}},
          // Microsoft Surface Cover
          {{0x045e, 0x09c2}, {0x045e, 0x07dc}},
          // HP 710 Wireless KB MS Combo
          {{0x04ca, 0x00bb}, {0x0461, 0x4ef1}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x3286}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x410a}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x4182}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x4189}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x4c01}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x5918}, {0x062a, 0x261b}},
          // MosArt 2.4G Keyboard Mouse
          {{0x062a, 0x9006}, {0x062a, 0x261b}},
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
  // Tracks whether the device was already logged to guarantee the device is
  // only tracked once.
  static base::NoDestructor<base::flat_set<VendorProductId>> logged_devices;
  auto [_, should_record_metric] =
      logged_devices->insert({device.vendor_id, device.product_id});

  const auto* keyboard_mouse_combo_metadata =
      GetKeyboardMouseComboMetadata(device);
  if (keyboard_mouse_combo_metadata) {
    if (should_record_metric) {
      base::UmaHistogramEnumeration(
          "ChromeOS.Inputs.ComboDeviceClassification",
          ui::ComboDeviceClassification::kKnownComboDevice);
    }
    return DeviceType::kKeyboardMouseCombo;
  }

  const auto* keyboard_metadata = GetKeyboardMetadata(device);
  if (keyboard_metadata) {
    if (should_record_metric) {
      base::UmaHistogramEnumeration(
          "ChromeOS.Inputs.ComboDeviceClassification",
          ui::ComboDeviceClassification::kKnownMouseImposter);
    }
    return DeviceType::kKeyboard;
  }

  const auto* mouse_metadata = GetMouseMetadata(device);
  if (mouse_metadata) {
    if (should_record_metric) {
      base::UmaHistogramEnumeration(
          "ChromeOS.Inputs.ComboDeviceClassification",
          ui::ComboDeviceClassification::kKnownKeyboardImposter);
    }
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
    case mojom::MouseButtonConfig::kThreeKey:
      return GetThreeKeyButtonRemappingList();
    case mojom::MouseButtonConfig::kFourKeyWithTopButton:
      return GetFourKeyWithTopButtonButtonRemappingList();
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
    case mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnlyOneButton:
      return GetWacomStandardPenOneButtonRemappingList();
  }
}

std::vector<mojom::ButtonRemappingPtr> GetTabletButtonRemappingListForConfig(
    mojom::GraphicsTabletButtonConfig graphics_tablet_button_config) {
  switch (graphics_tablet_button_config) {
    case mojom::GraphicsTabletButtonConfig::kNoConfig:
    case mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnly:
    case mojom::GraphicsTabletButtonConfig::kWacomStandardPenOnlyOneButton:
      return GetDefaultButtonRemappingList();
    case mojom::GraphicsTabletButtonConfig::kWacomStandardFourButtons:
      return GetWacomStandardFourButtonRemappingList();
  }
}

}  // namespace ash
