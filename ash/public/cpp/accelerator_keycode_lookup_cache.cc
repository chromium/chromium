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

using KeyCodeLookupEntry = AcceleratorKeycodeLookupCache::KeyCodeLookupEntry;

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
  key_code_to_cache_entry_.clear();
}

std::optional<KeyCodeLookupEntry> AcceleratorKeycodeLookupCache::Find(
    ui::KeyboardCode key_code,
    bool remap_positional_key) {
  const auto& found_iter =
      key_code_to_cache_entry_.find({key_code, remap_positional_key});
  if (found_iter == key_code_to_cache_entry_.end()) {
    return std::nullopt;
  }
  return found_iter->second;
}

void AcceleratorKeycodeLookupCache::InsertOrAssign(
    ui::KeyboardCode key_code,
    bool remap_positional_key,
    ui::DomCode dom_code,
    ui::DomKey dom_key,
    ui::KeyboardCode resulting_key_code,
    std::u16string description) {
  key_code_to_cache_entry_.insert_or_assign(
      {key_code, remap_positional_key},
      KeyCodeLookupEntry{dom_code, dom_key, resulting_key_code, description});
}

void AcceleratorKeycodeLookupCache::Clear() {
  key_code_to_cache_entry_.clear();
}

}  // namespace ash
