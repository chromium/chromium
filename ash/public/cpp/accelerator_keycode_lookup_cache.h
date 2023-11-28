// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_
#define ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_

#include <map>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Thin class that observe IME change events and updates the static cache for
// more efficient lookups for `KeycodeToKeyString()`.
class ASH_PUBLIC_EXPORT AcceleratorKeycodeLookupCache
    : public input_method::InputMethodManager::Observer {
 public:
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

  std::optional<std::u16string> Find(ui::KeyboardCode key_code);

  void InsertOrAssign(ui::KeyboardCode key_code, std::u16string description);

  void Clear();

 private:
  friend class AcceleratorKeycodeLookupCacheTest;

  // A cache of Keycode to string16 so that we don't have to perform a
  // reverse lookup for the DomKey for every accelerator.
  // On IME changes, this cache resets.
  std::map<ui::KeyboardCode, std::u16string> key_code_to_string16_cache_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATOR_KEYCODE_LOOKUP_CACHE_H_