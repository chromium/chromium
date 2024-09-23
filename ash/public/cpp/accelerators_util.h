// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_

#include <string>

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Represents the type of key pressed from a key event.
// Do not reorder or insert values in the middle as this is used by metrics.
enum class AcceleratorKeyInputType : uint8_t {
  kMetaLeft,
  kMetaRight,
  kControlLeft,
  kControlRight,
  kAltLeft,
  kAltRight,
  kShiftLeft,
  kShiftRight,
  kAltGr,
  kAlpha,
  kDigit,
  kTopRow,
  kSixPack,
  kNumberPad,
  kLeftArrow,
  kRightArrow,
  kUpArrow,
  kDownArrow,
  kEscape,
  kTab,
  kCapsLock,
  kSpace,
  kEnter,
  kBackspace,
  // Misc buckets every other key on the keyboard which mostly consists of
  // non-standard keys.
  kMisc,
  kFunction,
  kAssistant,
  kRightAlt,
  kMaxValue = kRightAlt,
};

// Returns the string of a DomKey for a given KeyboardCode. A keyboard code
// needs to be mapped to a physical key, DomCode, and then the DomCode needs
// to be mapped to a meaning or character of a DomKey based on the
// corresponding keyboard layout.
// `remap_postional_key` is an optional param, by default will attempt to
// convert any positional keys to the corresponding Domkey string.
// This function does take into account of keyboard locale.
ASH_PUBLIC_EXPORT std::u16string KeycodeToKeyString(
    ui::KeyboardCode key_code,
    bool remap_positional_key = true);

// Returns the string to display in the UI for the given key.
ASH_PUBLIC_EXPORT std::u16string GetKeyDisplay(
    ui::KeyboardCode key_code,
    bool remap_positional_key = true);

// Returns the `DomCode`, `DomKey`, and a string to display for the given
// `key_code`. The lookup of the keycode entry is remapped based on the US
// layout keycodes based on `remap_positional_key`. Returns nullptr if no valid
// `KeyCodeLookupEntry` can be produced from the given `key_code`.
ASH_PUBLIC_EXPORT
std::optional<AcceleratorKeycodeLookupCache::KeyCodeLookupEntry>
FindKeyCodeEntry(ui::KeyboardCode key_code,
                 ui::DomCode dom_code = ui::DomCode::NONE,
                 bool remap_positional_key = true);

// Returns the `AcceleratorKeyInputType` that matches for the given key_event.
ASH_PUBLIC_EXPORT
AcceleratorKeyInputType GetKeyInputTypeFromKeyEvent(
    const ui::KeyEvent& key_event);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_
