// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators_util.h"

#include <iterator>
#include <string>

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace {

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
      if (layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
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

  const absl::optional<std::u16string> cached_key_string =
      AcceleratorKeycodeLookupCache::Get()->Find(key_code);
  // Cache hit, return immediately.
  if (cached_key_string.has_value()) {
    return std::move(cached_key_string).value();
  }

  // Cache miss, get the key string and store it.
  for (const auto& dom_code : ui::kDomCodesArray) {
    if (!layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                               &key_code_to_compare)) {
      continue;
    }

    // Even though this isn't what we're looking for, we should still populate
    // the cache as we're iterating through the DomCode array.
    // Do not store "Unidentified".
    if (key_code_to_compare != key_code) {
      if (dom_key != ui::DomKey::UNIDENTIFIED) {
        AcceleratorKeycodeLookupCache::Get()->InsertOrAssign(
            key_code_to_compare,
            base::UTF8ToUTF16(
                ui::KeycodeConverter::DomKeyToKeyString(dom_key)));
      }
      continue;
    }

    if (!dom_key.IsValid() || dom_key.IsDeadKey()) {
      continue;
    }

    // Found the correct lookup, cache and return the string.
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

}  // namespace ash
