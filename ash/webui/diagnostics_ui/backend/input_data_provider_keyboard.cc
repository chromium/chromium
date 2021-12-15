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
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/system/statistics_provider.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
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

// Mapping from keyboard scancodes to TopRowKeys (must be in scancode-sorted
// order). This replicates and should be identical to the mapping behaviour
// of ChromeOS: changes will be needed if new AT scancodes or HID mappings
// are used in a top-row key, likely added in
// ui/events/keycodes/dom/dom_code_data.inc
//
// Note that there are no dedicated scancodes for kScreenMirror.
constexpr auto kScancodeMapping =
    base::MakeFixedFlatMap<uint32_t, mojom::TopRowKey>({
        // Vivaldi extended Set-1 AT-style scancodes
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
        {0xD3, mojom::TopRowKey::kDelete},  // Only relevant for Drallion.
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

// Convert an XKB layout string as stored in VPD (e.g. "xkb:us::eng" or
// "xkb:cz:qwerty:cze") into the form used by XkbKeyboardLayoutEngine (e.g. "us"
// or "cz(qwerty)").
std::string ConvertXkbLayoutString(const std::string& input) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      input, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  const base::StringPiece& id = parts[1];
  const base::StringPiece& variant = parts[2];
  return variant.empty() ? std::string(id)
                         : base::StrCat({id, "(", variant, ")"});
}

}  // namespace

InputDataProviderKeyboard::InputDataProviderKeyboard()
    : xkb_layout_engine_(xkb_evdev_codes_) {}
InputDataProviderKeyboard::~InputDataProviderKeyboard() {}

void InputDataProviderKeyboard::GetKeyboardVisualLayout(
    mojom::KeyboardInfoPtr keyboard,
    mojom::InputDataProvider::GetKeyboardVisualLayoutCallback callback) {
  std::string layout_name;
  if (keyboard->connection_type == mojom::ConnectionType::kInternal) {
    chromeos::system::StatisticsProvider* stats_provider =
        chromeos::system::StatisticsProvider::GetInstance();
    if (!stats_provider->GetMachineStatistic(
            chromeos::system::kKeyboardLayoutKey, &layout_name) ||
        layout_name.empty()) {
      LOG(ERROR) << "Couldn't determine visual layout for keyboard with ID "
                 << keyboard->id;
      return;
    }
    // In some regions, the keyboard layout string from the region database will
    // contain multiple comma-separated parts, where the first is the XKB layout
    // name. (For example, in region "gcc" (Gulf Cooperation Council), the
    // string is "xkb:us::eng,m17n:ar,t13n:ar".) We just want the first part.
    layout_name = base::SplitString(layout_name, ",", base::KEEP_WHITESPACE,
                                    base::SPLIT_WANT_ALL)[0];
    layout_name = ConvertXkbLayoutString(layout_name);
  } else {
    // TODO(crbug.com/1207678): Ensure layout is updated when IME changes
    // External keyboards generally don't tell us what layout they have, so
    // assume the layout the user has currently selected.
    layout_name = input_method::InputMethodManager::Get()
                      ->GetActiveIMEState()
                      ->GetCurrentInputMethod()
                      .keyboard_layout();
  }

  xkb_layout_engine_.SetCurrentLayoutByNameWithCallback(
      layout_name,
      base::BindOnce(&InputDataProviderKeyboard::ProcessXkbLayout,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InputDataProviderKeyboard::ProcessXkbLayout(
    mojom::InputDataProvider::GetKeyboardVisualLayoutCallback callback) {
  base::flat_map<uint32_t, mojom::KeyGlyphSetPtr> layout;

  // Add the glyphs for each range of evdev keycodes we're concerned with.
  // (The keycode ranges generally correspond to rows on a QWERTY keyboard, see
  // Linux's input-event-codes.h.)
  for (int evdev_code = KEY_1; evdev_code <= KEY_EQUAL; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_Q; evdev_code <= KEY_RIGHTBRACE; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_A; evdev_code <= KEY_GRAVE; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  for (int evdev_code = KEY_BACKSLASH; evdev_code <= KEY_SLASH; evdev_code++) {
    layout[evdev_code] = LookupGlyphSet(evdev_code);
  }
  layout[KEY_102ND] = LookupGlyphSet(KEY_102ND);

  std::move(callback).Run(std::move(layout));
}

mojom::KeyGlyphSetPtr InputDataProviderKeyboard::LookupGlyphSet(
    uint32_t evdev_code) {
  ui::DomCode dom_code = ui::KeycodeConverter::EvdevCodeToDomCode(evdev_code);
  ui::DomKey dom_key;
  ui::KeyboardCode key_code;
  if (!xkb_layout_engine_.Lookup(dom_code, ui::EF_NONE, &dom_key, &key_code)) {
    LOG(ERROR) << "Couldn't look up glyph for evdev code " << evdev_code;
    return nullptr;
  }
  mojom::KeyGlyphSetPtr glyph_set = mojom::KeyGlyphSet::New();
  glyph_set->main_glyph = ui::KeycodeConverter::DomKeyToKeyString(dom_key);

  if (!xkb_layout_engine_.Lookup(dom_code, ui::EF_SHIFT_DOWN, &dom_key,
                                 &key_code)) {
    LOG(WARNING) << "Couldn't look up shift glyph for evdev code "
                 << evdev_code;
  } else {
    const std::string shift_glyph =
        ui::KeycodeConverter::DomKeyToKeyString(dom_key);
    if (shift_glyph != base::ToUpperASCII(glyph_set->main_glyph)) {
      glyph_set->shift_glyph = shift_glyph;
    }
  }
  return glyph_set;
}

void InputDataProviderKeyboard::ProcessKeyboardTopRowLayout(
    const InputDeviceInformation* device_info,
    ui::EventRewriterChromeOS::KeyboardTopRowLayout* out_top_row_layout,
    std::vector<mojom::TopRowKey>* out_top_row_keys) {
  ui::InputDevice input_device = device_info->input_device;
  ui::EventRewriterChromeOS::DeviceType device_type;
  ui::EventRewriterChromeOS::KeyboardTopRowLayout top_row_layout;
  base::flat_map<uint32_t, ui::EventRewriterChromeOS::MutableKeyState>
      scan_code_map;
  ui::EventRewriterChromeOS::IdentifyKeyboard(input_device, &device_type,
                                              &top_row_layout, &scan_code_map);

  // Simple array in physical order from left to right
  std::vector<mojom::TopRowKey> top_row_keys = {};

  switch (top_row_layout) {
    case ui::EventRewriterChromeOS::kKbdTopRowLayoutWilco:
      top_row_keys.assign(std::begin(kSystemKeysWilco),
                          std::end(kSystemKeysWilco));
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayoutDrallion:
      top_row_keys.assign(std::begin(kSystemKeysDrallion),
                          std::end(kSystemKeysDrallion));

      // On some Drallion devices, the F12 key is used for the Privacy Screen.

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
      // which we will use to produce indexes.

      for (auto iter = scan_code_map.begin(); iter != scan_code_map.end();
           iter++) {
        size_t fn_key_number = iter->second.key_code - ui::VKEY_F1;
        uint32_t scancode = iter->first;

        if (top_row_keys.size() < fn_key_number + 1)
          top_row_keys.resize(fn_key_number + 1, mojom::TopRowKey::kNone);

        if (kScancodeMapping.contains(scancode))
          top_row_keys[fn_key_number] = kScancodeMapping.at(scancode);
        else
          top_row_keys[fn_key_number] = mojom::TopRowKey::kUnknown;
      }
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayout2:
      top_row_keys.assign(std::begin(kSystemKeys2), std::end(kSystemKeys2));
      break;

    case ui::EventRewriterChromeOS::kKbdTopRowLayout1:
    default:
      top_row_keys.assign(std::begin(kSystemKeys1), std::end(kSystemKeys1));
  }

  *out_top_row_layout = std::move(top_row_layout);
  *out_top_row_keys = std::move(top_row_keys);
}

mojom::KeyboardInfoPtr InputDataProviderKeyboard::ConstructKeyboard(
    const InputDeviceInformation* device_info) {
  mojom::KeyboardInfoPtr result = mojom::KeyboardInfo::New();

  result->id = device_info->evdev_id;
  result->connection_type = device_info->connection_type;
  result->name = device_info->event_device_info.name();

  // TODO(crbug.com/1207678): review support for WWCB keyboards, Chromebase
  // keyboards, and Dell KM713 Chrome keyboard.

  ui::EventRewriterChromeOS::KeyboardTopRowLayout top_row_layout_type;

  ProcessKeyboardTopRowLayout(device_info, &top_row_layout_type,
                              &result->top_row_keys);

  if (result->connection_type == mojom::ConnectionType::kInternal) {
    if (device_info->event_device_info.HasKeyEvent(KEY_KBD_LAYOUT_NEXT)) {
      // Only Dell Enterprise devices have this key, marked by a globe icon.
      result->physical_layout = mojom::PhysicalLayout::kChromeOSDellEnterprise;
    } else {
      result->physical_layout = mojom::PhysicalLayout::kChromeOS;
    }
    // TODO(crbug.com/1207678): set internal keyboard as unknown on CloudReady
    // (board names chromeover64 or reven).
    result->mechanical_layout = GetSystemMechanicalLayout();

    result->number_pad_present =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kHasNumberPad)
            ? mojom::NumberPadPresence::kPresent
            : mojom::NumberPadPresence::kNotPresent;

    // Log if there is contradictory information.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kHasNumberPad) &&
        !device_info->event_device_info.HasNumberpad())
      LOG(ERROR) << "OS believes internal numberpad is implemented, but "
                    "evdev disagrees.";
  } else {
    result->physical_layout = mojom::PhysicalLayout::kUnknown;

    if (top_row_layout_type == ui::EventRewriterChromeOS::KeyboardTopRowLayout::
                                   kKbdTopRowLayoutCustom) {
      // If keyboard has WWCB top row custom layout (vivaldi) then we can trust
      // the HID descriptor to be accurate about presence of keys.
      result->number_pad_present =
          !device_info->event_device_info.HasNumberpad()
              ? mojom::NumberPadPresence::kNotPresent
              : mojom::NumberPadPresence::kPresent;
    } else {
      // Without WWCB information, absence of KP keycodes means it definitely
      // doesn't have a numberpad, but the presence isn't a reliable indicator.
      result->number_pad_present =
          !device_info->event_device_info.HasNumberpad()
              ? mojom::NumberPadPresence::kNotPresent
              : mojom::NumberPadPresence::kUnknown;
    }
  }

  result->has_assistant_key =
      device_info->event_device_info.HasKeyEvent(KEY_ASSISTANT);

  return result;
}

}  // namespace diagnostics
}  // namespace ash
