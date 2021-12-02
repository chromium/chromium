// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider_keyboard.h"

#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/system/statistics_provider.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ash {
namespace diagnostics {

namespace {

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

mojom::KeyboardInfoPtr InputDataProviderKeyboard::ConstructKeyboard(
    int id,
    const ui::EventDeviceInfo* device_info,
    mojom::ConnectionType connection_type) {
  mojom::KeyboardInfoPtr result = mojom::KeyboardInfo::New();
  result->id = id;
  result->connection_type = connection_type;
  result->name = device_info->name();

  if (result->connection_type == mojom::ConnectionType::kInternal) {
    if (device_info->HasKeyEvent(KEY_KBD_LAYOUT_NEXT)) {
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
  } else {
    result->physical_layout = mojom::PhysicalLayout::kUnknown;
    result->number_pad_present = mojom::NumberPadPresence::kUnknown;
    // TODO(crbug.com/1207678): support WWCB keyboards, Chromebase keyboards,
    // and Dell KM713 Chrome keyboard.
  }

  result->has_assistant_key = device_info->HasKeyEvent(KEY_ASSISTANT);

  return result;
}

}  // namespace diagnostics
}  // namespace ash
