// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"

#include <string>

#include "base/no_destructor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

AcceleratorKeycodeLookupCache* g_instance = nullptr;

}  // namespace

AcceleratorKeycodeLookupCache* AcceleratorKeycodeLookupCache::Get() {
  return g_instance;
}

AcceleratorKeycodeLookupCache::AcceleratorKeycodeLookupCache() {
  CHECK(!g_instance);
  g_instance = this;

  DCHECK(input_method::InputMethodManager::Get());
  input_method::InputMethodManager::Get()->AddObserver(this);
}

AcceleratorKeycodeLookupCache::~AcceleratorKeycodeLookupCache() {
  CHECK(input_method::InputMethodManager::Get());
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void AcceleratorKeycodeLookupCache::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  key_code_to_string16_cache_.clear();
}

std::optional<std::u16string> AcceleratorKeycodeLookupCache::Find(
    ui::KeyboardCode key_code) {
  const auto& found_iter = key_code_to_string16_cache_.find(key_code);
  if (found_iter == key_code_to_string16_cache_.end()) {
    return std::nullopt;
  }
  return found_iter->second;
}

void AcceleratorKeycodeLookupCache::InsertOrAssign(ui::KeyboardCode key_code,
                                                   std::u16string description) {
  key_code_to_string16_cache_.insert_or_assign(key_code, description);
}

void AcceleratorKeycodeLookupCache::Clear() {
  key_code_to_string16_cache_.clear();
}

}  // namespace ash
