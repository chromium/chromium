// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_keyboard.h"

#include <fcntl.h>
#include <linux/input.h>

#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/mojom/input.mojom-shared.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-shared.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
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

// Hard-coded top-row key mappings. These are intended to match the behaviour of
// EventRewriterAsh::RewriteFunctionKeys for historical keyboards. No
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

// Turkish F-Type xkb keyboard layout id which is used to differentiate between
// a device from 'tr' region with Q-Type vs F-Type.
constexpr std::string_view kTurkishFLayoutId = "xkb:tr:f:tur";

// |kTurkeyRegionCode| is the real turkey region code.
// |kTurkeyFLayoutRegionCode| is used purely in the diagnostics app to
// accurately display F-Type keyboard layouts.
constexpr std::string_view kTurkeyRegionCode = "tr";
constexpr std::string_view kTurkeyFLayoutRegionCode = "tr.f";

mojom::MechanicalLayout GetSystemMechanicalLayout() {
  system::StatisticsProvider* stats_provider =
      system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> layout_string =
      stats_provider->GetMachineStatistic(system::kKeyboardMechanicalLayoutKey);
  if (!layout_string) {
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
    LOG(ERROR) << "Unknown mechanical layout " << layout_string.value();
    return mojom::MechanicalLayout::kUnknown;
  }
}

std::optional<std::string> GetRegionCode() {
  system::StatisticsProvider* stats_provider =
      system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> layout_string =
      stats_provider->GetMachineStatistic(system::kRegionKey);
  if (!layout_string) {
    LOG(ERROR) << "Couldn't determine region";
    return std::nullopt;
  }

  // In Turkey, two different layouts are shipped (Q-Type and F-Type) under the
  // same region code |kTurkeyRegionCode|. To do a best effort differentiation
  // between the two, query the current IME. If it is |kTurkishFLayoutId|,
  // return our made up |kTurnishFLayoutRegionCode|.
  if (layout_string.value() == kTurkeyRegionCode) {
    ImeControllerImpl* controller = Shell::Get()->ime_controller();
    DCHECK(controller);
    if (base::EndsWith(controller->current_ime().id, kTurkishFLayoutId)) {
      return std::string(kTurkeyFLayoutRegionCode);
    }
  }

  return std::string(layout_string.value());
}

constexpr mojom::TopRowKey ConvertTopRowActionKeyToDiagnosticsTopRowKey(
    ui::TopRowActionKey action_key) {
  switch (action_key) {
    case ui::TopRowActionKey::kBack:
      return mojom::TopRowKey::kBack;
    case ui::TopRowActionKey::kForward:
      return mojom::TopRowKey::kForward;
    case ui::TopRowActionKey::kRefresh:
      return mojom::TopRowKey::kRefresh;
    case ui::TopRowActionKey::kFullscreen:
      return mojom::TopRowKey::kFullscreen;
    case ui::TopRowActionKey::kOverview:
      return mojom::TopRowKey::kOverview;
    case ui::TopRowActionKey::kScreenshot:
      return mojom::TopRowKey::kScreenshot;
    case ui::TopRowActionKey::kScreenBrightnessDown:
      return mojom::TopRowKey::kScreenBrightnessDown;
    case ui::TopRowActionKey::kScreenBrightnessUp:
      return mojom::TopRowKey::kScreenBrightnessUp;
    case ui::TopRowActionKey::kMicrophoneMute:
      return mojom::TopRowKey::kMicrophoneMute;
    case ui::TopRowActionKey::kVolumeMute:
      return mojom::TopRowKey::kVolumeMute;
    case ui::TopRowActionKey::kVolumeDown:
      return mojom::TopRowKey::kVolumeDown;
    case ui::TopRowActionKey::kVolumeUp:
      return mojom::TopRowKey::kVolumeUp;
    case ui::TopRowActionKey::kKeyboardBacklightToggle:
      return mojom::TopRowKey::kKeyboardBacklightToggle;
    case ui::TopRowActionKey::kKeyboardBacklightDown:
      return mojom::TopRowKey::kKeyboardBacklightDown;
    case ui::TopRowActionKey::kKeyboardBacklightUp:
      return mojom::TopRowKey::kKeyboardBacklightUp;
    case ui::TopRowActionKey::kNextTrack:
      return mojom::TopRowKey::kNextTrack;
    case ui::TopRowActionKey::kPreviousTrack:
      return mojom::TopRowKey::kPreviousTrack;
    case ui::TopRowActionKey::kPlayPause:
      return mojom::TopRowKey::kPlayPause;
    case ui::TopRowActionKey::kPrivacyScreenToggle:
      return mojom::TopRowKey::kPrivacyScreenToggle;
    case ui::TopRowActionKey::kAllApplications:
    case ui::TopRowActionKey::kEmojiPicker:
    case ui::TopRowActionKey::kDictation:
    case ui::TopRowActionKey::kAccessibility:
    case ui::TopRowActionKey::kUnknown:
      return mojom::TopRowKey::kUnknown;
    case ui::TopRowActionKey::kNone:
      return mojom::TopRowKey::kNone;
  }
}

}  // namespace

InputDataProviderKeyboard::InputDataProviderKeyboard() {}
InputDataProviderKeyboard::~InputDataProviderKeyboard() {}

InputDataProviderKeyboard::AuxData::AuxData() = default;
InputDataProviderKeyboard::AuxData::~AuxData() = default;

void InputDataProviderKeyboard::ProcessKeyboardTopRowLayout(
    const InputDeviceInformation* device_info,
    ui::KeyboardCapability::KeyboardTopRowLayout top_row_layout,
    const std::vector<uint32_t>& top_row_scan_codes,
    std::vector<mojom::TopRowKey>* out_top_row_keys,
    AuxData* out_aux_data) {
  // Simple array in physical order from left to right
  std::vector<mojom::TopRowKey> top_row_keys;

  // Map of scan-code -> index within tow_row_keys: 0 is first key to the
  // right of Escape, 1 is next key to the right of it, etc.
  base::flat_map<uint32_t, uint32_t> top_row_key_scancode_indexes;

  switch (top_row_layout) {
    case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      top_row_keys.assign(std::begin(kSystemKeysWilco),
                          std::end(kSystemKeysWilco));

      for (size_t i = 0; i < top_row_keys.size(); i++)
        top_row_key_scancode_indexes[kScancodesWilco[i]] = i;
      break;

    case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
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

    case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom: {
      top_row_keys.reserve(top_row_scan_codes.size());

      // If action keys cannot be found or if it has a different size than the
      // top row scan codes, do not fill out the arrays.
      // This will only happen if there is an error in `KeyboardCapability`.
      const auto* action_keys =
          Shell::Get()->keyboard_capability()->GetTopRowActionKeys(
              ui::KeyboardDevice(device_info->input_device));
      if (!action_keys || action_keys->size() != top_row_scan_codes.size()) {
        break;
      }

      // Exclude all top row keys which are considered `kNone`.
      size_t index = 0;
      for (size_t i = 0; i < top_row_scan_codes.size(); i++) {
        auto top_row_key =
            ConvertTopRowActionKeyToDiagnosticsTopRowKey((*action_keys)[i]);
        if (top_row_key == mojom::TopRowKey::kNone) {
          continue;
        }
        top_row_keys.push_back(top_row_key);
        top_row_key_scancode_indexes[top_row_scan_codes[i]] = index++;
      }
      break;
    }

    case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
      top_row_keys.assign(std::begin(kSystemKeys2), std::end(kSystemKeys2));
      // No specific top_row_key_scancode_indexes are needed
      // for classic ChromeOS keyboards, as they do not have an /Fn/ key and
      // only emit /F[0-9]+/ keys.
      break;

    case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
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
                              device_info->keyboard_scan_codes,
                              &result->top_row_keys, out_aux_data);

  // Work out the physical layout.
  if (device_info->keyboard_type ==
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    // Reven boards have unknown keyboard layouts and should not be considered
    // internal keyboards for the purposes of diagnostics.
    if (switches::IsRevenBranding()) {
      result->physical_layout = mojom::PhysicalLayout::kUnknown;
      result->connection_type = mojom::ConnectionType::kUnknown;
    } else if (device_info->keyboard_top_row_layout ==
               ui::KeyboardCapability::KeyboardTopRowLayout::
                   kKbdTopRowLayoutWilco) {
      result->physical_layout =
          mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco;
    } else if (device_info->keyboard_top_row_layout ==
               ui::KeyboardCapability::KeyboardTopRowLayout::
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
  if (result->physical_layout != mojom::PhysicalLayout::kUnknown) {
    result->mechanical_layout = GetSystemMechanicalLayout();
    result->region_code = GetRegionCode();
  } else {
    result->mechanical_layout = mojom::MechanicalLayout::kUnknown;
    result->region_code = std::nullopt;
  }

  // Determine number pad presence.
  if (result->physical_layout != mojom::PhysicalLayout::kUnknown) {
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
             ui::KeyboardCapability::KeyboardTopRowLayout::
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

  // Logic in InputDataProvider will change kUnknown to the most likely one in
  // cases where we can't be sure.
  result->top_right_key = mojom::TopRightKey::kUnknown;
  if (result->physical_layout != mojom::PhysicalLayout::kUnknown) {
    if (result->physical_layout ==
        mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco) {
      // The first generation of Wilco devices both have lock in the top-right
      // (and a separate power key).
      result->top_right_key = mojom::TopRightKey::kLock;
    } else if (device_info->event_device_info.bustype() == BUS_USB) {
      // It's a detachable keyboard (counted as internal USB), so it definitely
      // has Lock in the top-right.
      result->top_right_key = mojom::TopRightKey::kLock;
    } else if (device_info->event_device_info.HasKeyEvent(KEY_CONTROLPANEL)) {
      // All actual internal keyboards (not detachable) with KEY_CONTROLPANEL
      // (i.e. Eve) have the Control Panel key in the top right.
      result->top_right_key = mojom::TopRightKey::kControlPanel;
    }
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
  const auto jter = kFKeyOrder.find(event->key_code);
  if (event->top_row_position == -1 && jter != kFKeyOrder.end()) {
    event->top_row_position = jter->second;
  }

  return event;
}

}  // namespace diagnostics
}  // namespace ash
