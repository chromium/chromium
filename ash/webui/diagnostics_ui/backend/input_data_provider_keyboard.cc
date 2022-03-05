// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider_keyboard.h"

#include <fcntl.h>
#include <linux/input.h>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/shell.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chromeos/system/statistics_provider.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

namespace {

enum {
  kFKey1 = 0,
  kFKey2,
  kFKey3,
  kFKey4,
  kFKey5,
  kFKey6,
  kFKey7,
  kFKey8,
  kFKey9,
  kFKey10,
  kFKey11,
  kFKey12,
  kFKey13,
  kFKey14,
  kFKey15
};

// Numeric values of evdev KEY_F# are non-contiguous, making this mapping
// non-trivial.
constexpr auto kFKeyOrder =
    base::MakeFixedFlatMap<uint32_t, unsigned int>({{KEY_F1, kFKey1},
                                                    {KEY_F2, kFKey2},
                                                    {KEY_F3, kFKey3},
                                                    {KEY_F4, kFKey4},
                                                    {KEY_F5, kFKey5},
                                                    {KEY_F6, kFKey6},
                                                    {KEY_F7, kFKey7},
                                                    {KEY_F8, kFKey8},
                                                    {KEY_F9, kFKey9},
                                                    {KEY_F10, kFKey10},
                                                    {KEY_F11, kFKey11},
                                                    {KEY_F12, kFKey12},
                                                    {KEY_F13, kFKey13},
                                                    {KEY_F14, kFKey14},
                                                    {KEY_F15, kFKey15}});

// Mapping from keyboard scancodes to TopRowKeys (must be in scancode-sorted
// order) for keyboards with custom top row layouts (vivaldi). This replicates
// and should be identical to the mapping behaviour of ChromeOS: changes will
// be needed if new AT scancodes or HID mappings are used in a top-row key,
// likely added in ui/events/keycodes/dom/dom_code_data.inc.
//
// Note that there are currently no dedicated scancodes for kScreenMirror.
constexpr auto kCustomScancodeMapping =
    base::MakeFixedFlatMap<uint32_t, mojom::TopRowKey>({
        // Vivaldi-specific extended Set-1 AT-style scancodes.
        {0x90, mojom::TopRowKey::kPreviousTrack},
        {0x91, mojom::TopRowKey::kFullscreen},
        {0x92, mojom::TopRowKey::kOverview},
        {0x93, mojom::TopRowKey::kScreenshot},
        {0x94, mojom::TopRowKey::kScreenBrightnessDown},
        {0x95, mojom::TopRowKey::kScreenBrightnessUp},
        {0x96, mojom::TopRowKey::kPrivacyScreenToggle},
        {0x97, mojom::TopRowKey::kKeyboardBacklightDown},
        {0x98, mojom::TopRowKey::kKeyboardBacklightUp},
        {0x99, mojom::TopRowKey::kNextTrack},
        {0x9A, mojom::TopRowKey::kPlayPause},
        {0xA0, mojom::TopRowKey::kVolumeMute},
        {0xAE, mojom::TopRowKey::kVolumeDown},
        {0xB0, mojom::TopRowKey::kVolumeUp},
        {0xE9, mojom::TopRowKey::kForward},
        {0xEA, mojom::TopRowKey::kBack},
        {0xE7, mojom::TopRowKey::kRefresh},

        // HID 32-bit usage codes
        {0x070046, mojom::TopRowKey::kScreenshot},
        {0x0C00E2, mojom::TopRowKey::kVolumeMute},
        {0x0C00E9, mojom::TopRowKey::kVolumeUp},
        {0x0C00EA, mojom::TopRowKey::kVolumeDown},
        {0x0C006F, mojom::TopRowKey::kScreenBrightnessUp},
        {0x0C0070, mojom::TopRowKey::kScreenBrightnessDown},
        {0x0C0079, mojom::TopRowKey::kKeyboardBacklightUp},
        {0x0C007A, mojom::TopRowKey::kKeyboardBacklightDown},
        {0x0C00B5, mojom::TopRowKey::kNextTrack},
        {0x0C00B6, mojom::TopRowKey::kPreviousTrack},
        {0x0C00CD, mojom::TopRowKey::kPlayPause},
        {0x0C0224, mojom::TopRowKey::kBack},
        {0x0C0225, mojom::TopRowKey::kForward},
        {0x0C0227, mojom::TopRowKey::kRefresh},
        {0x0C0232, mojom::TopRowKey::kFullscreen},
        {0x0C029F, mojom::TopRowKey::kOverview},
        {0x0C02D0, mojom::TopRowKey::kPrivacyScreenToggle},
    });

// Hard-coded top-row key mappings. These are intended to match the behaviour of
// EventRewriterChromeOS::RewriteFunctionKeys for historical keyboards. No
// updates should be needed, as all new keyboards are expected to be using
// customizable top row keys (vivaldi).

constexpr mojom::TopRowKey kSystemKeys1[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kForward,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

constexpr mojom::TopRowKey kSystemKeys2[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kPlayPause,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

constexpr mojom::TopRowKey kSystemKeysWilco[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp,
    mojom::TopRowKey::kNone,          // F10
    mojom::TopRowKey::kNone,          // F11
    mojom::TopRowKey::kScreenMirror,  // F12
    mojom::TopRowKey::kDelete  // Just a normal Delete key, but in the top row.
};

constexpr mojom::TopRowKey kSystemKeysDrallion[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp,
    mojom::TopRowKey::kNone,  // F10
    mojom::TopRowKey::kNone,  // F11
    mojom::TopRowKey::kNone,  // F12 - May be Privacy Screen on some models.
    mojom::TopRowKey::kScreenMirror,
    mojom::TopRowKey::kDelete  // Just a normal Delete key, but in the top row.
};

// Wilco and Drallion have unique 'action' scancodes for their top rows,
// that are different from the vivaldi mappings. These scancodes are generated
// when a top-tow key is pressed without the /Fn/ modifier.
constexpr uint32_t kScancodesWilco[] = {
    0xEA, 0xE7, 0xD5, 0xD6, 0x95, 0x91, 0xA0,
    0xAE, 0xB0, 0x44, 0x57, 0x8B, 0xD3,
};

constexpr uint32_t kScancodesDrallion[] = {
    0xEA, 0xE7, 0xD5, 0xD6, 0x95, 0x91, 0xA0,
    0xAE, 0xB0, 0x44, 0x57, 0xd7, 0x8B, 0xD3,
};

mojom::MechanicalLayout GetSystemMechanicalLayout() {
  chromeos::system::StatisticsProvider* stats_provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string layout_string;
  if (!stats_provider->GetMachineStatistic(
          chromeos::system::kKeyboardMechanicalLayoutKey, &layout_string)) {
    LOG(ERROR) << "Couldn't determine mechanical layout";
    return mojom::MechanicalLayout::kUnknown;
  }
  if (layout_string == "ANSI") {
    return mojom::MechanicalLayout::kAnsi;
  } else if (layout_string == "ISO") {
    return mojom::MechanicalLayout::kIso;
  } else if (layout_string == "JIS") {
    return mojom::MechanicalLayout::kJis;
  } else {
    LOG(ERROR) << "Unknown mechanical layout " << layout_string;
    return mojom::MechanicalLayout::kUnknown;
  }
}

absl::optional<std::string> GetRegionCode() {
  chromeos::system::StatisticsProvider* stats_provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string layout_string;
  if (!stats_provider->GetMachineStatistic(chromeos::system::kRegionKey,
                                           &layout_string)) {
    LOG(ERROR) << "Couldn't determine region";
    return absl::nullopt;
  }

  return layout_string;
}

}  // namespace

InputDataProviderKeyboard::InputDataProviderKeyboard() {}
InputDataProviderKeyboard::~InputDataProviderKeyboard() {}

InputDataProviderKeyboard::AuxData::AuxData() = default;
InputDataProviderKeyboard::AuxData::~AuxData() = default;

void InputDataProviderKeyboard::ProcessKeyboardTopRowLayout(
    const InputDeviceInformation* device_info,
    ui::EventRewriterChromeOS::KeyboardTopRowLayout top_row_layout,
    const base::flat_map<uint32_t, ui::EventRewriterChromeOS::MutableKeyState>&
        scan_code_map,
    std::vector<mojom::TopRowKey>* out_top_row_keys,
    AuxData* out_aux_data) {
  ui::InputDevice input_device = device_info->input_device;

  // Simple array in physical order from left to right
  std::vector<mojom::TopRowKey> top_row_keys = {};

  // Map of scan-code -> index within tow_row_keys: 0 is first key to the
  // right of Escape, 1 is next key to the right of it, etc.
  base::flat_map<uint32_t, uint32_t> top_row_key_scancode_indexes;

  switch (top_row_layout) {
    case ui::EventRewriterChromeOS::kKbdTopRowLayoutWilco:
      top_row_keys.assign(std::begin(kSystemKeysWilco),
                          std::end(kSystemKeysWilco));

      for (size_t i = 0; i < top_row_keys.size(); i++)
        top_row_key_scancode_indexes[kScancodesWilco[i]] = i;
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayoutDrallion:
      top_row_keys.assign(std::begin(kSystemKeysDrallion),
                          std::end(kSystemKeysDrallion));

      for (size_t i = 0; i < top_row_keys.size(); i++)
        top_row_key_scancode_indexes[kScancodesDrallion[i]] = i;

      // On some Drallion devices, the F12 key is used for the Privacy Screen.

      // The scancode for F12 does not need to be modified, it is the same on
      // all Drallion devices, only the interpretation of the key is different.

      // This should be the same logic as in
      // EventRewriterControllerImpl::Initialize. This is a historic device, and
      // this logic should not need to be updated, as newer devices will use
      // custom top row layouts (vivaldi).
      if (Shell::Get()->privacy_screen_controller() &&
          Shell::Get()->privacy_screen_controller()->IsSupported()) {
        top_row_keys[kFKey12] = mojom::TopRowKey::kPrivacyScreenToggle;
      }

      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayoutCustom:

      // Process scan-code map generated from custom top-row key layout: it maps
      // from physical scan codes to several things, including VKEY key-codes,
      // which we will use to derive a linear index.

      for (auto iter = scan_code_map.begin(); iter != scan_code_map.end();
           iter++) {
        size_t fn_key_number = iter->second.key_code - ui::VKEY_F1;
        uint32_t scancode = iter->first;

        if (top_row_keys.size() < fn_key_number + 1)
          top_row_keys.resize(fn_key_number + 1, mojom::TopRowKey::kNone);

        if (kCustomScancodeMapping.contains(scancode))
          top_row_keys[fn_key_number] = kCustomScancodeMapping.at(scancode);
        else
          top_row_keys[fn_key_number] = mojom::TopRowKey::kUnknown;

        top_row_key_scancode_indexes[scancode] = fn_key_number;
      }
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayout2:
      top_row_keys.assign(std::begin(kSystemKeys2), std::end(kSystemKeys2));
      // No specific top_row_key_scancode_indexes are needed
      // for classic ChromeOS keyboards, as they do not have an /Fn/ key and
      // only emit /F[0-9]+/ keys.
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayout1:
    default:
      top_row_keys.assign(std::begin(kSystemKeys1), std::end(kSystemKeys1));
      // No specific top_row_key_scancode_indexes are needed for classic
      // ChromeOS keyboards, as they do not have an /Fn/ key and only emit
      // /F[0-9]+/ keys.
      //
      // If this is an unknown keyboard and we are just using Layout1 as
      // the default, we also do not want to assign any scancode or keycode
      // indexes, as we do not know whether the keyboard can generate special
      // keys, or their location relative to the top row.
  }

  *out_top_row_keys = std::move(top_row_keys);
  out_aux_data->top_row_key_scancode_indexes =
      std::move(top_row_key_scancode_indexes);
}

mojom::KeyboardInfoPtr InputDataProviderKeyboard::ConstructKeyboard(
    const InputDeviceInformation* device_info,
    AuxData* out_aux_data) {
  mojom::KeyboardInfoPtr result = mojom::KeyboardInfo::New();

  result->id = device_info->evdev_id;
  result->connection_type = device_info->connection_type;
  result->name = device_info->event_device_info.name();

  // TODO(crbug.com/1207678): review support for WWCB keyboards, Chromebase
  // keyboards, and Dell KM713 Chrome keyboard.

  ProcessKeyboardTopRowLayout(device_info, device_info->keyboard_top_row_layout,
                              device_info->keyboard_scan_code_map,
                              &result->top_row_keys, out_aux_data);

  // Work out the physical layout.
  if (device_info->keyboard_type ==
      ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard) {
    // TODO(crbug.com/1207678): set internal keyboard as unknown on ChromeOS
    // Flex (board names chromeover64 or reven).
    if (device_info->keyboard_top_row_layout ==
        ui::EventRewriterChromeOS::KeyboardTopRowLayout::
            kKbdTopRowLayoutWilco) {
      result->physical_layout =
          mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco;
    } else if (device_info->keyboard_top_row_layout ==
               ui::EventRewriterChromeOS::KeyboardTopRowLayout::
                   kKbdTopRowLayoutDrallion) {
      result->physical_layout =
          mojom::PhysicalLayout::kChromeOSDellEnterpriseDrallion;
    } else {
      result->physical_layout = mojom::PhysicalLayout::kChromeOS;
    }
  } else {
    result->physical_layout = mojom::PhysicalLayout::kUnknown;
  }

  // Get the mechanical and visual layouts, if possible.
  if (device_info->keyboard_type ==
      ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard) {
    result->mechanical_layout = GetSystemMechanicalLayout();
    result->region_code = GetRegionCode();
  } else {
    result->mechanical_layout = mojom::MechanicalLayout::kUnknown;
    result->region_code = absl::nullopt;
  }

  // Determine number pad presence.
  if (device_info->keyboard_type ==
      ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard) {
    result->number_pad_present =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kHasNumberPad)
            ? mojom::NumberPadPresence::kPresent
            : mojom::NumberPadPresence::kNotPresent;

    // Log if there is contradictory information.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kHasNumberPad) &&
        !device_info->event_device_info.HasNumberpad())
      LOG(ERROR) << "OS believes internal numberpad is implemented, but "
                    "evdev disagrees.";
  } else if (device_info->keyboard_top_row_layout ==
             ui::EventRewriterChromeOS::KeyboardTopRowLayout::
                 kKbdTopRowLayoutCustom) {
    // If keyboard has WWCB top row custom layout (vivaldi) then we can trust
    // the HID descriptor to be accurate about presence of keys.
    result->number_pad_present = device_info->event_device_info.HasNumberpad()
                                     ? mojom::NumberPadPresence::kPresent
                                     : mojom::NumberPadPresence::kNotPresent;
  } else {
    // Without WWCB information, absence of KP keycodes means it definitely
    // doesn't have a numberpad, but the presence isn't a reliable indicator.
    result->number_pad_present = device_info->event_device_info.HasNumberpad()
                                     ? mojom::NumberPadPresence::kUnknown
                                     : mojom::NumberPadPresence::kNotPresent;
  }

  result->has_assistant_key =
      device_info->event_device_info.HasKeyEvent(KEY_ASSISTANT);

  return result;
}

mojom::KeyEventPtr InputDataProviderKeyboard::ConstructInputKeyEvent(
    const mojom::KeyboardInfoPtr& keyboard,
    const AuxData* aux_data,
    uint32_t key_code,
    uint32_t scan_code,
    bool down) {
  mojom::KeyEventPtr event = mojom::KeyEvent::New();
  event->id = keyboard->id;
  event->type =
      down ? mojom::KeyEventType::kPress : mojom::KeyEventType::kRelease;
  event->key_code = key_code;    // evdev code
  event->scan_code = scan_code;  // scan code
  event->top_row_position = -1;

  // If a top row action key was pressed, note its physical index in the row.
  const auto iter =
      aux_data->top_row_key_scancode_indexes.find(event->scan_code);
  if (iter != aux_data->top_row_key_scancode_indexes.end()) {
    event->top_row_position = iter->second;
  }

  // Do the same if F1-F15 was pressed.
  const auto* jter = kFKeyOrder.find(event->key_code);
  if (event->top_row_position == -1 && jter != kFKeyOrder.end()) {
    event->top_row_position = jter->second;
  }

  return event;
}

}  // namespace diagnostics
}  // namespace ash
