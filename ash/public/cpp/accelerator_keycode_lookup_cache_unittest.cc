// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"

#include <memory>
#include <string>

#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using KeyCodeLookupEntry = AcceleratorKeycodeLookupCache::KeyCodeLookupEntry;

class AcceleratorKeycodeLookupCacheTest : public testing::Test {
 public:
  class TestInputMethodManager : public input_method::MockInputMethodManager {
   public:
    void AddObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.AddObserver(observer);
    }

    void RemoveObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.RemoveObserver(observer);
    }

    // Calls all observers with Observer::InputMethodChanged
    void NotifyInputMethodChanged() {
      for (auto& observer : observers_) {
        observer.InputMethodChanged(
            /*manager=*/this, /*profile=*/nullptr, /*show_message=*/false);
      }
    }

    base::ObserverList<InputMethodManager::Observer>::Unchecked observers_;
  };

  AcceleratorKeycodeLookupCacheTest() {
    input_method_manager_ = new TestInputMethodManager();
    input_method::InputMethodManager::Initialize(input_method_manager_);

    layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    ui::KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine();
    ui::KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        layout_engine_.get());

    lookup_cache_ = std::make_unique<AcceleratorKeycodeLookupCache>();
  }

  ~AcceleratorKeycodeLookupCacheTest() override = default;

 protected:
  std::map<std::pair<ui::KeyboardCode, bool>, KeyCodeLookupEntry> cache() {
    return lookup_cache_->key_code_to_cache_entry_;
  }

  std::unique_ptr<AcceleratorKeycodeLookupCache> lookup_cache_;
  std::unique_ptr<ui::StubKeyboardLayoutEngine> layout_engine_;
  // Test global singleton. Delete is handled by InputMethodManager::Shutdown().
  raw_ptr<TestInputMethodManager> input_method_manager_;
};

TEST_F(AcceleratorKeycodeLookupCacheTest, ImeChanged) {
  const std::u16string expected = u"a";

  EXPECT_TRUE(cache().empty());
  lookup_cache_->InsertOrAssign(ui::KeyboardCode::VKEY_A,
                                /*remap_positional_key=*/false,
                                ui::DomCode::NONE, ui::DomKey::NONE,
                                ui::KeyboardCode::VKEY_A, expected);
  // Expect the cache to be populated.
  std::optional<KeyCodeLookupEntry> found_entry = lookup_cache_->Find(
      ui::KeyboardCode::VKEY_A, /*remap_positional_key=*/false);
  EXPECT_TRUE(found_entry.has_value());
  EXPECT_EQ(expected, found_entry->key_display);

  // Trigger IME change event, expect the cache to be cleared.
  input_method_manager_->NotifyInputMethodChanged();
  EXPECT_TRUE(cache().empty());
  found_entry = lookup_cache_->Find(ui::KeyboardCode::VKEY_A,
                                    /*remap_positional_key=*/false);
  EXPECT_FALSE(found_entry.has_value());
}

}  // namespace ash
