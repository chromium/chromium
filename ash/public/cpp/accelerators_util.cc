// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators_util.h"

#include <iterator>
#include <string>

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

using KeyCodeLookupEntry = AcceleratorKeycodeLookupCache::KeyCodeLookupEntry;

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
          {ui::KeyboardCode::VKEY_CAPITAL, u"caps lock"},
          {ui::KeyboardCode::VKEY_ACCESSIBILITY, u"Accessibility"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          {ui::KeyboardCode::VKEY_RIGHT_ALT, u"RightAlt"},
#else
          {ui::KeyboardCode::VKEY_RIGHT_ALT, u"right alt"},
#endif
      }));
  return *key_display_map;
}

bool IsValidDomCode(ui::DomCode dom_code) {
  return ui::KeycodeConverter::InvalidNativeKeycode() !=
         ui::KeycodeConverter::UsbKeycodeToNativeKeycode(
             static_cast<int32_t>(dom_code));
}

bool IsAlphaOrPunctuationKey(ui::KeyboardCode key_code) {
  if (key_code >= ui::VKEY_A && key_code <= ui::VKEY_Z) {
    return true;
  }

  static constexpr auto kPunctuationKeys =
      base::MakeFixedFlatSet<ui::KeyboardCode>({
          ui::VKEY_OEM_1,
          ui::VKEY_OEM_PLUS,
          ui::VKEY_OEM_COMMA,
          ui::VKEY_OEM_MINUS,
          ui::VKEY_OEM_PERIOD,
          ui::VKEY_OEM_2,
          ui::VKEY_OEM_3,
          ui::VKEY_OEM_4,
          ui::VKEY_OEM_5,
          ui::VKEY_OEM_6,
          ui::VKEY_OEM_7,
          ui::VKEY_OEM_8,
          ui::VKEY_OEM_102,
          ui::VKEY_OEM_103,
          ui::VKEY_OEM_104,
      });
  return kPunctuationKeys.contains(key_code);
}

bool IsDigitKey(ui::KeyboardCode key_code) {
  return key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9;
}

bool IsSixPackKey(ui::KeyboardCode key_code) {
  static constexpr auto kSixPackKeys = base::MakeFixedFlatSet<ui::KeyboardCode>(
      {ui::VKEY_PRIOR, ui::VKEY_NEXT, ui::VKEY_END, ui::VKEY_HOME,
       ui::VKEY_INSERT, ui::VKEY_DELETE});
  return kSixPackKeys.contains(key_code);
}

bool IsNumpadKey(ui::KeyboardCode key_code) {
  // Numpad keys are all in consecutive order.
  return key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_DIVIDE;
}

// This includes only the top row keys we know about, it is possible there are
// other on external keyboards. They would instead be considered misc keys.
bool IsTopRowKey(ui::KeyboardCode key_code, ui::DomCode dom_code) {
  static constexpr auto kTopRowKeys = base::MakeFixedFlatSet<ui::KeyboardCode>({
      ui::VKEY_F1,
      ui::VKEY_F2,
      ui::VKEY_F3,
      ui::VKEY_F4,
      ui::VKEY_F5,
      ui::VKEY_F6,
      ui::VKEY_F7,
      ui::VKEY_F8,
      ui::VKEY_F9,
      ui::VKEY_F10,
      ui::VKEY_F11,
      ui::VKEY_F12,
      ui::VKEY_F13,
      ui::VKEY_F14,
      ui::VKEY_F15,
      ui::VKEY_F16,
      ui::VKEY_F17,
      ui::VKEY_F18,
      ui::VKEY_F19,
      ui::VKEY_F20,
      ui::VKEY_F21,
      ui::VKEY_F22,
      ui::VKEY_F23,
      ui::VKEY_F24,
      ui::VKEY_BROWSER_BACK,
      ui::VKEY_BROWSER_FORWARD,
      ui::VKEY_BROWSER_REFRESH,
      ui::VKEY_BROWSER_STOP,
      ui::VKEY_BROWSER_SEARCH,
      ui::VKEY_BROWSER_FAVORITES,
      ui::VKEY_BROWSER_HOME,
      ui::VKEY_VOLUME_MUTE,
      ui::VKEY_VOLUME_DOWN,
      ui::VKEY_VOLUME_UP,
      ui::VKEY_MEDIA_NEXT_TRACK,
      ui::VKEY_MEDIA_PREV_TRACK,
      ui::VKEY_MEDIA_STOP,
      ui::VKEY_MEDIA_PLAY_PAUSE,
      ui::VKEY_MEDIA_LAUNCH_MAIL,
      ui::VKEY_MEDIA_LAUNCH_MEDIA_SELECT,
      ui::VKEY_MEDIA_LAUNCH_APP1,
      ui::VKEY_MEDIA_LAUNCH_APP2,
      ui::VKEY_PLAY,
      ui::VKEY_ZOOM,
      ui::VKEY_SNAPSHOT,
      ui::VKEY_PRIVACY_SCREEN_TOGGLE,
      ui::VKEY_MICROPHONE_MUTE_TOGGLE,
      ui::VKEY_BRIGHTNESS_DOWN,
      ui::VKEY_BRIGHTNESS_UP,
      ui::VKEY_KBD_BACKLIGHT_TOGGLE,
      ui::VKEY_KBD_BRIGHTNESS_DOWN,
      ui::VKEY_KBD_BRIGHTNESS_UP,
      ui::VKEY_SLEEP,
  });

  if (dom_code == ui::DomCode::SHOW_ALL_WINDOWS) {
    return true;
  }

  return kTopRowKeys.contains(key_code);
}

}  // namespace

std::optional<ash::KeyCodeLookupEntry> FindKeyCodeEntry(
    ui::KeyboardCode key_code,
    ui::DomCode original_dom_code,
    bool remap_positional_key) {
  std::optional<ash::KeyCodeLookupEntry> cached_key_data =
      ash::AcceleratorKeycodeLookupCache::Get()->Find(key_code,
                                                      remap_positional_key);
  // Cache hit, return immediately.
  if (cached_key_data) {
    return cached_key_data;
  }

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
        ui::KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(
            key_code, original_dom_code);
    if (dom_code != ui::DomCode::NONE) {
      std::u16string result;
      if (IsValidDomCode(dom_code) &&
          layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                                &key_code_to_compare)) {
        if (!dom_key.IsValid()) {
          return std::nullopt;
        }
        if (dom_key.IsDeadKey()) {
          result = GetStringForDeadKey(dom_key);
        } else {
          result = base::UTF8ToUTF16(
              ui::KeycodeConverter::DomKeyToKeyString(dom_key));
        }
      }
      if (dom_key != ui::DomKey::UNIDENTIFIED) {
        ash::AcceleratorKeycodeLookupCache::Get()->InsertOrAssign(
            key_code, /*remap_positional_key=*/remap_positional_key, dom_code,
            dom_key, key_code_to_compare, result);
      }
      return KeyCodeLookupEntry{dom_code, dom_key, key_code_to_compare, result};
    }
  }

  // Cache miss, get the key string and store it.
  for (const auto& dom_code : ui::kDomCodesArray) {
    if (IsValidDomCode(dom_code) &&
        !layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                               &key_code_to_compare)) {
      continue;
    }

    if (!dom_key.IsValid() || dom_key == ui::DomKey::UNIDENTIFIED) {
      continue;
    }

    if (key_code_to_compare != key_code) {
      continue;
    }

    const std::u16string key_string =
        base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
    if (dom_key != ui::DomKey::UNIDENTIFIED) {
      AcceleratorKeycodeLookupCache::Get()->InsertOrAssign(
          key_code,
          /*remap_positional_key=*/remap_positional_key, dom_code, dom_key,
          key_code_to_compare, key_string);
    }

    return ash::KeyCodeLookupEntry{dom_code, dom_key, key_code_to_compare,
                                   key_string};
  }
  return std::nullopt;
}

std::u16string KeycodeToKeyString(ui::KeyboardCode key_code,
                                  bool remap_positional_key) {
  auto entry =
      FindKeyCodeEntry(key_code, ui::DomCode::NONE, remap_positional_key);
  return entry ? std::move(entry->key_display) : std::u16string();
}

std::u16string GetKeyDisplay(ui::KeyboardCode key_code,
                             bool remap_positional_key) {
  // If there's an entry for this key_code in our
  // map, return that entry's value.
  auto it = GetKeyDisplayMap().find(key_code);
  if (it != GetKeyDisplayMap().end()) {
    return it->second;
  } else {
    const std::u16string unconverted_string =
        KeycodeToKeyString(key_code, remap_positional_key);
    const std::string converted_string = base::UTF16ToUTF8(unconverted_string);
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
      // Else, return "Key {digit}" for Unidentified key.
      return base::UTF8ToUTF16(
          base::StringPrintf("Key %u", static_cast<unsigned int>(key_code)));
    }
    return unconverted_string;
  }
}

AcceleratorKeyInputType GetKeyInputTypeFromKeyEvent(
    const ui::KeyEvent& key_event) {
  const ui::KeyboardCode key_code = key_event.key_code();
  if (IsAlphaOrPunctuationKey(key_code)) {
    return AcceleratorKeyInputType::kAlpha;
  }

  if (IsDigitKey(key_code)) {
    return AcceleratorKeyInputType::kDigit;
  }

  if (IsTopRowKey(key_code, key_event.code())) {
    return AcceleratorKeyInputType::kTopRow;
  }

  if (IsSixPackKey(key_code)) {
    return AcceleratorKeyInputType::kSixPack;
  }

  if (IsNumpadKey(key_code)) {
    return AcceleratorKeyInputType::kNumberPad;
  }

  if (HasRightAltProperty(key_event)) {
    return AcceleratorKeyInputType::kRightAlt;
  }

  switch (key_event.code()) {
    case ui::DomCode::META_LEFT:
      return AcceleratorKeyInputType::kMetaLeft;
    case ui::DomCode::META_RIGHT:
      return AcceleratorKeyInputType::kMetaRight;
    case ui::DomCode::CONTROL_LEFT:
      return AcceleratorKeyInputType::kControlLeft;
    case ui::DomCode::CONTROL_RIGHT:
      return AcceleratorKeyInputType::kControlRight;
    case ui::DomCode::ALT_LEFT:
      return AcceleratorKeyInputType::kAltLeft;
    case ui::DomCode::ALT_RIGHT:
      if (key_event.key_code() == ui::VKEY_ALTGR) {
        return AcceleratorKeyInputType::kAltGr;
      }
      return AcceleratorKeyInputType::kAltRight;
    case ui::DomCode::SHIFT_LEFT:
      return AcceleratorKeyInputType::kShiftLeft;
    case ui::DomCode::SHIFT_RIGHT:
      return AcceleratorKeyInputType::kShiftRight;
    case ui::DomCode::FN:
      return AcceleratorKeyInputType::kFunction;
    default:
      break;
  }

  switch (key_code) {
    case ui::VKEY_ESCAPE:
      return AcceleratorKeyInputType::kEscape;
    case ui::VKEY_TAB:
      return AcceleratorKeyInputType::kTab;
    case ui::VKEY_CAPITAL:
      return AcceleratorKeyInputType::kCapsLock;
    case ui::VKEY_SPACE:
      return AcceleratorKeyInputType::kSpace;
    case ui::VKEY_RETURN:
      return AcceleratorKeyInputType::kEnter;
    case ui::VKEY_BACK:
      return AcceleratorKeyInputType::kBackspace;
    case ui::VKEY_UP:
      return AcceleratorKeyInputType::kUpArrow;
    case ui::VKEY_DOWN:
      return AcceleratorKeyInputType::kDownArrow;
    case ui::VKEY_RIGHT:
      return AcceleratorKeyInputType::kRightArrow;
    case ui::VKEY_LEFT:
      return AcceleratorKeyInputType::kLeftArrow;
    case ui::VKEY_ASSISTANT:
      return AcceleratorKeyInputType::kAssistant;
    default:
      break;
  }

  return AcceleratorKeyInputType::kMisc;
}

}  // namespace ash
