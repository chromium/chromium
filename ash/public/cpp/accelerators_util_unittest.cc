// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators_util.h"

#include <memory>
#include <string>

#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using KeyCodeLookupEntry = AcceleratorKeycodeLookupCache::KeyCodeLookupEntry;

class AcceleratorsUtilTest : public AshTestBase {
 public:
  AcceleratorsUtilTest() {
    layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    ui::KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine();
    ui::KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        layout_engine_.get());
  }

  ~AcceleratorsUtilTest() override = default;

 protected:
  std::unique_ptr<ui::StubKeyboardLayoutEngine> layout_engine_;
};

TEST_F(AcceleratorsUtilTest, BasicDomCode) {
  const std::u16string expected = u"a";

  std::optional<KeyCodeLookupEntry> found_entry =
      AcceleratorKeycodeLookupCache::Get()->Find(ui::KeyboardCode::VKEY_A,
                                                 /*remap_positional_key=*/true);
  EXPECT_FALSE(found_entry.has_value());
  EXPECT_EQ(expected, KeycodeToKeyString(ui::KeyboardCode::VKEY_A));
  // Expect the cache to be populated.
  found_entry = AcceleratorKeycodeLookupCache::Get()->Find(
      ui::KeyboardCode::VKEY_A, /*remap_positional_key=*/true);
  EXPECT_TRUE(found_entry.has_value());
  EXPECT_EQ(expected, found_entry->key_display);
}

TEST_F(AcceleratorsUtilTest, PositionalKeyCode) {
  // Provide a custom layout that mimics behavior of a de-DE keyboard.
  // In the German keyboard, VKEY_OEM_4 is located at DomCode position MINUS
  // with DomKey `ß`. With positional remapping, VKEY_OEM_4 is remapped to
  // search for DomCode BRACKET_LEFT, resulting in DomKey `ü`.
  const std::vector<ui::StubKeyboardLayoutEngine::CustomLookupEntry> table = {
      {ui::DomCode::MINUS, ui::DomKey::FromCharacter(u'ß'),
       ui::DomKey::FromCharacter(u'?'), ui::KeyboardCode::VKEY_OEM_4},
      {ui::DomCode::BRACKET_LEFT, ui::DomKey::FromCharacter(u'ü'),
       ui::DomKey::FromCharacter(u'Ü'), ui::KeyboardCode::VKEY_OEM_1}};

  layout_engine_->SetCustomLookupTableForTesting(table);

  // Without positional remapping, expect `ß` to be the returned string.
  const std::u16string expected = u"ß";
  EXPECT_EQ(expected, KeycodeToKeyString(ui::KeyboardCode::VKEY_OEM_4,
                                         /*remap_positional_key=*/false));

  // With positional remmaping, expect `ü` to be the returned string.
  const std::u16string expected2 = u"ü";
  EXPECT_EQ(expected2, KeycodeToKeyString(ui::KeyboardCode::VKEY_OEM_4,
                                          /*remap_positional_key=*/true));
}

TEST_F(AcceleratorsUtilTest, NonAlphanumericKey) {
  const std::u16string expected = u"Meta";
  std::optional<AcceleratorKeycodeLookupCache::KeyCodeLookupEntry> found_entry =
      AcceleratorKeycodeLookupCache::Get()->Find(ui::KeyboardCode::VKEY_COMMAND,
                                                 /*remap_positional_key=*/true);
  EXPECT_FALSE(found_entry.has_value());
  EXPECT_EQ(expected, KeycodeToKeyString(ui::KeyboardCode::VKEY_COMMAND));

  found_entry = AcceleratorKeycodeLookupCache::Get()->Find(
      ui::KeyboardCode::VKEY_COMMAND, /*remap_positional_key=*/true);
  EXPECT_TRUE(found_entry.has_value());
  EXPECT_EQ(expected, found_entry->key_display);
}

TEST_F(AcceleratorsUtilTest, UnidentifiedKey) {
  EXPECT_EQ(u"Key 10", GetKeyDisplay(ui::KeyboardCode::VKEY_BACKTAB));
  EXPECT_EQ(u"Key 231", GetKeyDisplay(ui::KeyboardCode::VKEY_PACKET));
  EXPECT_EQ(u"Key 240", GetKeyDisplay(ui::KeyboardCode::VKEY_OEM_ATTN));
  EXPECT_EQ(u"Key 241", GetKeyDisplay(ui::KeyboardCode::VKEY_OEM_FINISH));
}

}  // namespace ash
