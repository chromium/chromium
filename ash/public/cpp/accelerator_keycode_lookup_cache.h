// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_

#include <map>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Thin class that observe IME change events and updates the static cache for
// more efficient lookups for `KeycodeToKeyString()`.
class ASH_PUBLIC_EXPORT AcceleratorKeycodeLookupCache
    : public input_method::InputMethodManager::Observer {
 public:
  struct KeyCodeLookupEntry {
    // Resulting `DomCode` from the reverse KeyboardCode -> DomCode lookup.
    ui::DomCode dom_code;
    // Resulting `DomKey` from looking up the `dom_code` based on the current
    // keyboard layout.
    ui::DomKey dom_key;
    // Resulting `KeyboardCode` when the input `KeyboardCode` is positionally
    // remapped and looked up in the current layout.
    ui::KeyboardCode resulting_key_code;
    // The string to display in UI for the input `KeyboardCode`.
    std::u16string key_display;
  };

  // Get the singleton instance.
  static AcceleratorKeycodeLookupCache* Get();

  AcceleratorKeycodeLookupCache();
  AcceleratorKeycodeLookupCache(const AcceleratorKeycodeLookupCache&) = delete;
  AcceleratorKeycodeLookupCache& operator=(
      const AcceleratorKeycodeLookupCache&) = delete;
  ~AcceleratorKeycodeLookupCache() override;

  // InputMethodManager::Observer
  // The cache is invalid when the IME changes, so this observer notifies us
  // when to clear the cache.
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  std::optional<KeyCodeLookupEntry> Find(ui::KeyboardCode key_code,
                                         bool remap_positional_key);

  void InsertOrAssign(ui::KeyboardCode key_code,
                      bool remap_positional_key,
                      ui::DomCode dom_code,
                      ui::DomKey dom_key,
                      ui::KeyboardCode resulting_key_code,
                      std::u16string description);

  void Clear();

 private:
  friend class AcceleratorKeycodeLookupCacheTest;

  // A cache of Keycode to string16 so that we don't have to perform a
  // reverse lookup for the DomKey for every accelerator.
  // On IME changes, this cache resets.
  std::map<std::pair<ui::KeyboardCode, bool>, KeyCodeLookupEntry>
      key_code_to_cache_entry_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_
