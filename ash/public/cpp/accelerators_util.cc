// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators_util.h"

#include <iterator>
#include <string>

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace {

constexpr char kUnidentifiedKeyString[] = "Unidentified";

// Dead keys work by combining two consecutive keystrokes together. The first
// keystroke does not produce an output character, it acts as a one-shot
// modifier for a subsequent keystroke. So for example on a German keyboard,
// pressing the acute ´ dead key, then pressing the letter e will produce é.
// The first character is called the combining character and does not produce
// an output glyph. This table maps the combining character to a string
// containing the non-combining equivalent that can be displayed.
std::u16string GetStringForDeadKey(ui::DomKey dom_key) {
  DCHECK(dom_key.IsDeadKey());
  int32_t ch = dom_key.ToDeadKeyCombiningCharacter();
  switch (ch) {
    // Combining grave.
    case 0x300:
      return u"`";
    // Combining acute.
    case 0x301:
      return u"´";
    // Combining circumflex.
    case 0x302:
      return u"^";
    // Combining tilde.
    case 0x303:
      return u"~";
    // Combining diaeresis.
    case 0x308:
      return u"¨";
    default:
      break;
  }

  LOG(WARNING) << "No mapping for dead key: " << ch;
  return base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
}

// This map is for KeyboardCodes that don't return a key_display from
// `KeycodeToKeyString`. The string values here were arbitrarily chosen
// based on the VKEY enum name.
const base::flat_map<ui::KeyboardCode, std::u16string>& GetKeyDisplayMap() {
  static auto key_display_map =
      base::NoDestructor(base::flat_map<ui::KeyboardCode, std::u16string>({
          {ui::KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE,
           u"MicrophoneMuteToggle"},
          {ui::KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE,
           u"KeyboardBacklightToggle"},
          {ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_UP, u"KeyboardBrightnessUp"},
          {ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN,
           u"KeyboardBrightnessDown"},
          {ui::KeyboardCode::VKEY_SLEEP, u"Sleep"},
          {ui::KeyboardCode::VKEY_NEW, u"NewTab"},
          {ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE,
           u"PrivacyScreenToggle"},
          {ui::KeyboardCode::VKEY_ALL_APPLICATIONS, u"ViewAllApps"},
          {ui::KeyboardCode::VKEY_DICTATE, u"EnableOrToggleDictation"},
          {ui::KeyboardCode::VKEY_WLAN, u"ToggleWifi"},
          {ui::KeyboardCode::VKEY_EMOJI_PICKER, u"EmojiPicker"},
          {ui::KeyboardCode::VKEY_MENU, u"alt"},
          {ui::KeyboardCode::VKEY_HOME, u"home"},
          {ui::KeyboardCode::VKEY_END, u"end"},
          {ui::KeyboardCode::VKEY_DELETE, u"delete"},
          {ui::KeyboardCode::VKEY_INSERT, u"insert"},
          {ui::KeyboardCode::VKEY_PRIOR, u"page up"},
          {ui::KeyboardCode::VKEY_NEXT, u"page down"},
          {ui::KeyboardCode::VKEY_SPACE, u"space"},
          {ui::KeyboardCode::VKEY_TAB, u"tab"},
          {ui::KeyboardCode::VKEY_ESCAPE, u"esc"},
          {ui::KeyboardCode::VKEY_RETURN, u"enter"},
          {ui::KeyboardCode::VKEY_BACK, u"backspace"},
          {ui::KeyboardCode::VKEY_MEDIA_PLAY, u"MediaPlay"},
          {ui::KeyboardCode::VKEY_NUMPAD0, u"numpad 0"},
          {ui::KeyboardCode::VKEY_NUMPAD1, u"numpad 1"},
          {ui::KeyboardCode::VKEY_NUMPAD2, u"numpad 2"},
          {ui::KeyboardCode::VKEY_NUMPAD3, u"numpad 3"},
          {ui::KeyboardCode::VKEY_NUMPAD4, u"numpad 4"},
          {ui::KeyboardCode::VKEY_NUMPAD5, u"numpad 5"},
          {ui::KeyboardCode::VKEY_NUMPAD6, u"numpad 6"},
          {ui::KeyboardCode::VKEY_NUMPAD7, u"numpad 7"},
          {ui::KeyboardCode::VKEY_NUMPAD8, u"numpad 8"},
          {ui::KeyboardCode::VKEY_NUMPAD9, u"numpad 9"},
          {ui::KeyboardCode::VKEY_ADD, u"numpad +"},
          {ui::KeyboardCode::VKEY_DECIMAL, u"numpad ."},
          {ui::KeyboardCode::VKEY_DIVIDE, u"numpad /"},
          {ui::KeyboardCode::VKEY_MULTIPLY, u"numpad *"},
          {ui::KeyboardCode::VKEY_SUBTRACT, u"numpad -"},
      }));
  return *key_display_map;
}

bool IsValidDomCode(ui::DomCode dom_code) {
  return ui::KeycodeConverter::InvalidNativeKeycode() !=
         ui::KeycodeConverter::UsbKeycodeToNativeKeycode(
             static_cast<int32_t>(dom_code));
}

}  // namespace

namespace ash {

std::u16string KeycodeToKeyString(ui::KeyboardCode key_code,
                                  bool remap_positional_key) {
  ui::DomKey dom_key;
  ui::KeyboardCode key_code_to_compare = ui::VKEY_UNKNOWN;
  const ui::KeyboardLayoutEngine* layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();

  // The input |key_code| is the |KeyboardCode| aka VKEY of the shortcut in
  // the US layout which is registered from the shortcut table. |key_code|
  // is first mapped to the |DomCode| this key is on in the US layout. If
  // the key is not positional, this processing is skipped and it is handled
  // normally in the loop below. For the positional keys, the |DomCode| is
  // then mapped to the |DomKey| in the current layout which represents the
  // glyph/character that appears on the key (and usually when typed).

  // Positional keys are direct lookups, no need to store in the cache.
  if (remap_positional_key &&
      ::features::IsImprovedKeyboardShortcutsEnabled()) {
    ui::DomCode dom_code =
        ui::KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(key_code);
    if (dom_code != ui::DomCode::NONE) {
      if (IsValidDomCode(dom_code) &&
          layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                                &key_code_to_compare)) {
        if (dom_key.IsDeadKey()) {
          return GetStringForDeadKey(dom_key);
        }
        if (!dom_key.IsValid()) {
          return std::u16string();
        }
        return base::UTF8ToUTF16(
            ui::KeycodeConverter::DomKeyToKeyString(dom_key));
      }
      return std::u16string();
    }
  }

  const std::optional<std::u16string> cached_key_string =
      AcceleratorKeycodeLookupCache::Get()->Find(key_code);
  // Cache hit, return immediately.
  if (cached_key_string.has_value()) {
    return std::move(cached_key_string).value();
  }

  // Cache miss, get the key string and store it.
  for (const auto& dom_code : ui::kDomCodesArray) {
    if (IsValidDomCode(dom_code) &&
        !layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                               &key_code_to_compare)) {
      continue;
    }

    if (!dom_key.IsValid() || dom_key.IsDeadKey()) {
      continue;
    }

    if (key_code_to_compare != key_code) {
      continue;
    }

    const std::u16string key_string =
        base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
    if (dom_key != ui::DomKey::UNIDENTIFIED) {
      AcceleratorKeycodeLookupCache::Get()->InsertOrAssign(key_code,
                                                           key_string);
    }

    return key_string;
  }
  return std::u16string();
}

std::u16string GetKeyDisplay(ui::KeyboardCode key_code) {
  // If there's an entry for this key_code in our
  // map, return that entry's value.
  auto it = GetKeyDisplayMap().find(key_code);
  if (it != GetKeyDisplayMap().end()) {
    return it->second;
  } else {
    const std::string converted_string =
        base::UTF16ToUTF8(KeycodeToKeyString(key_code));
    // If `KeycodeToKeyString` fails to get a proper string, fallback to
    // the domcode string.
    if (converted_string == kUnidentifiedKeyString || converted_string == "") {
      ui::DomCode converted_domcode =
          ui::UsLayoutKeyboardCodeToDomCode(key_code);
      if (converted_domcode != ui::DomCode::NONE) {
        return base::UTF8ToUTF16(
            ui::KeycodeConverter::DomCodeToCodeString(converted_domcode));
      }

      // If no DomCode can be mapped, attempt reverse DomKey mappings.
      for (const auto& domkey_it : ui::kDomKeyToKeyboardCodeMap) {
        if (domkey_it.key_code == key_code) {
          return base::UTF8ToUTF16(
              ui::KeycodeConverter::DomKeyToKeyString(domkey_it.dom_key));
        }
      }
      // Else, return "Unidentified {digit}" for Unidentified key.
      return base::UTF8ToUTF16(base::StringPrintf(
          "Unidentified %u", static_cast<unsigned int>(key_code)));
    }
    // Otherwise, get the key_display from a util function.
    return KeycodeToKeyString(key_code);
  }
}

}  // namespace ash
