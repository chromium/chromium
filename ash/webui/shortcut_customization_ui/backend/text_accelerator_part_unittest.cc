// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/text_accelerator_part.h"

#include <string>

#include "ash/public/mojom/accelerator_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {
namespace {

using Type = mojom::TextAcceleratorPartType;

// -----------------------------------------------------------------------------
// Modifier constructor.
// -----------------------------------------------------------------------------

// Parameterized coverage of every supported modifier flag, exercising the
// mapping in a single table-driven place.
struct ModifierCase {
  ui::EventFlags modifier;
  std::u16string expected_text;
};

class TextAcceleratorPartModifierTest
    : public testing::TestWithParam<ModifierCase> {};

TEST_P(TextAcceleratorPartModifierTest, ProducesExpectedModifierText) {
  const ModifierCase& test_case = GetParam();
  TextAcceleratorPart part(test_case.modifier);
  EXPECT_EQ(part.text, test_case.expected_text);
  EXPECT_EQ(part.type, Type::kModifier);
  EXPECT_FALSE(part.keycode.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TextAcceleratorPartModifierTest,
    testing::Values(ModifierCase{ui::EF_SHIFT_DOWN, u"shift"},
                    ModifierCase{ui::EF_CONTROL_DOWN, u"ctrl"},
                    ModifierCase{ui::EF_ALT_DOWN, u"alt"},
                    ModifierCase{ui::EF_COMMAND_DOWN, u"meta"}));

// -----------------------------------------------------------------------------
// Keycode constructor.
// -----------------------------------------------------------------------------

TEST(TextAcceleratorPartTest, KeyCodeStoresKeycodeAndLeavesTextEmpty) {
  TextAcceleratorPart part(ui::VKEY_A);
  EXPECT_EQ(part.type, Type::kKey);
  ASSERT_TRUE(part.keycode.has_value());
  EXPECT_EQ(part.keycode.value(), ui::VKEY_A);
  // The localized key string is resolved later, so no text is stored here.
  EXPECT_TRUE(part.text.empty());
}

class TextAcceleratorPartKeyCodeTest
    : public testing::TestWithParam<ui::KeyboardCode> {};

TEST_P(TextAcceleratorPartKeyCodeTest, StoresKeycodeForAllKinds) {
  const ui::KeyboardCode key_code = GetParam();
  TextAcceleratorPart part(key_code);
  EXPECT_EQ(part.type, Type::kKey);
  ASSERT_TRUE(part.keycode.has_value());
  EXPECT_EQ(part.keycode.value(), key_code);
  EXPECT_TRUE(part.text.empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TextAcceleratorPartKeyCodeTest,
                         testing::Values(ui::VKEY_A,
                                         ui::VKEY_Z,
                                         ui::VKEY_0,
                                         ui::VKEY_9,
                                         ui::VKEY_SPACE,
                                         ui::VKEY_RETURN,
                                         ui::VKEY_TAB,
                                         ui::VKEY_ESCAPE,
                                         ui::VKEY_LEFT,
                                         ui::VKEY_RIGHT,
                                         ui::VKEY_F1,
                                         ui::VKEY_UNKNOWN));

// -----------------------------------------------------------------------------
// Plain-text constructor.
// -----------------------------------------------------------------------------

TEST(TextAcceleratorPartTest, PlainText) {
  const std::u16string kPlainText = u"and";
  TextAcceleratorPart part(kPlainText);
  EXPECT_EQ(part.text, kPlainText);
  EXPECT_EQ(part.type, Type::kPlainText);
  EXPECT_FALSE(part.keycode.has_value());
}

TEST(TextAcceleratorPartTest, PlainTextEmpty) {
  const std::u16string kEmpty;
  TextAcceleratorPart part(kEmpty);
  EXPECT_TRUE(part.text.empty());
  EXPECT_EQ(part.type, Type::kPlainText);
  EXPECT_FALSE(part.keycode.has_value());
}

TEST(TextAcceleratorPartTest, PlainTextUnicode) {
  const std::u16string kUnicode = u"press 日本語 ✧";
  TextAcceleratorPart part(kUnicode);
  EXPECT_EQ(part.text, kUnicode);
  EXPECT_EQ(part.type, Type::kPlainText);
  EXPECT_FALSE(part.keycode.has_value());
}

TEST(TextAcceleratorPartTest, PlainTextWhitespaceIsPreserved) {
  const std::u16string kWhitespace = u"  then  ";
  TextAcceleratorPart part(kWhitespace);
  EXPECT_EQ(part.text, kWhitespace);
  EXPECT_EQ(part.type, Type::kPlainText);
  EXPECT_FALSE(part.keycode.has_value());
}

// -----------------------------------------------------------------------------
// Delimiter constructor.
// -----------------------------------------------------------------------------

TEST(TextAcceleratorPartTest, DelimiterPlusSign) {
  TextAcceleratorPart part(TextAcceleratorDelimiter::kPlusSign);
  EXPECT_EQ(part.text, u"+");
  EXPECT_EQ(part.type, Type::kDelimiter);
  EXPECT_FALSE(part.keycode.has_value());
}

// -----------------------------------------------------------------------------
// Copy constructor / copy assignment.
// -----------------------------------------------------------------------------

TEST(TextAcceleratorPartTest, CopyConstructorKeyPart) {
  TextAcceleratorPart original(ui::VKEY_B);
  TextAcceleratorPart copy(original);
  EXPECT_EQ(copy.type, original.type);
  EXPECT_EQ(copy.text, original.text);
  ASSERT_TRUE(copy.keycode.has_value());
  EXPECT_EQ(copy.keycode.value(), ui::VKEY_B);
}

TEST(TextAcceleratorPartTest, CopyConstructorModifierPart) {
  TextAcceleratorPart original(ui::EF_CONTROL_DOWN);
  TextAcceleratorPart copy(original);
  EXPECT_EQ(copy.type, Type::kModifier);
  EXPECT_EQ(copy.text, u"ctrl");
  EXPECT_FALSE(copy.keycode.has_value());
}

TEST(TextAcceleratorPartTest, CopyConstructorPlainTextPart) {
  TextAcceleratorPart original(u"or");
  TextAcceleratorPart copy(original);
  EXPECT_EQ(copy.type, Type::kPlainText);
  EXPECT_EQ(copy.text, u"or");
  EXPECT_FALSE(copy.keycode.has_value());
}

TEST(TextAcceleratorPartTest, CopyConstructorDelimiterPart) {
  TextAcceleratorPart original(TextAcceleratorDelimiter::kPlusSign);
  TextAcceleratorPart copy(original);
  EXPECT_EQ(copy.type, Type::kDelimiter);
  EXPECT_EQ(copy.text, u"+");
  EXPECT_FALSE(copy.keycode.has_value());
}

TEST(TextAcceleratorPartTest, CopyAssignmentOverwritesModifierWithPlainText) {
  TextAcceleratorPart original(u"then");
  TextAcceleratorPart assigned(ui::EF_SHIFT_DOWN);
  assigned = original;
  EXPECT_EQ(assigned.type, Type::kPlainText);
  EXPECT_EQ(assigned.text, u"then");
  EXPECT_FALSE(assigned.keycode.has_value());
}

TEST(TextAcceleratorPartTest, CopyAssignmentClearsKeycodeWhenSourceHasNone) {
  TextAcceleratorPart original(ui::EF_ALT_DOWN);
  TextAcceleratorPart assigned(ui::VKEY_C);
  ASSERT_TRUE(assigned.keycode.has_value());
  assigned = original;
  EXPECT_EQ(assigned.type, Type::kModifier);
  EXPECT_EQ(assigned.text, u"alt");
  EXPECT_FALSE(assigned.keycode.has_value());
}

TEST(TextAcceleratorPartTest, CopyAssignmentCopiesKeycode) {
  TextAcceleratorPart original(ui::VKEY_LEFT);
  TextAcceleratorPart assigned(u"plain");
  assigned = original;
  EXPECT_EQ(assigned.type, Type::kKey);
  ASSERT_TRUE(assigned.keycode.has_value());
  EXPECT_EQ(assigned.keycode.value(), ui::VKEY_LEFT);
  EXPECT_TRUE(assigned.text.empty());
}

TEST(TextAcceleratorPartTest, SelfAssignmentIsSafe) {
  TextAcceleratorPart part(ui::VKEY_A);
  const TextAcceleratorPart& alias = part;
  part = alias;
  EXPECT_EQ(part.type, Type::kKey);
  ASSERT_TRUE(part.keycode.has_value());
  EXPECT_EQ(part.keycode.value(), ui::VKEY_A);
  EXPECT_TRUE(part.text.empty());
}

}  // namespace
}  // namespace ash
