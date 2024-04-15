// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;

TEST(PickerModel, AvailableCategoriesWithNoFocusHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  PickerModel model(/*focused_client=*/nullptr, &fake_ime_keyboard);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kCapsOn, PickerCategory::kLinks,
                  PickerCategory::kExpressions, PickerCategory::kClipboard,
                  PickerCategory::kDriveFiles, PickerCategory::kLocalFiles,
                  PickerCategory::kDatesTimes, PickerCategory::kUnitsMaths));
}

TEST(PickerModel, AvailableCategoriesWithNoSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0));

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kCapsOn, PickerCategory::kLinks,
                  PickerCategory::kExpressions, PickerCategory::kClipboard,
                  PickerCategory::kDriveFiles, PickerCategory::kLocalFiles,
                  PickerCategory::kDatesTimes, PickerCategory::kUnitsMaths));
}

TEST(PickerModel, AvailableCategoriesWithSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kUpperCase, PickerCategory::kLowerCase,
                  PickerCategory::kSentenceCase, PickerCategory::kTitleCase));
}

TEST(PickerModel, AvailableCategoriesShowsCapsOffWhenCapsIsOn) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  fake_ime_keyboard.SetCapsLockEnabled(true);
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kCapsOff));
}

TEST(PickerModel, AvailableCategoriesShowsCapsOnWhenCapsIsOff) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  fake_ime_keyboard.SetCapsLockEnabled(false);
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kCapsOn));
}

TEST(PickerModel, GetsEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 1));

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_FALSE(model.HasSelectedText());
  EXPECT_EQ(model.selected_text(), u"");
}

TEST(PickerModel, GetsNonEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 3));

  PickerModel model(&client, &fake_ime_keyboard);
  EXPECT_TRUE(model.HasSelectedText());
  EXPECT_EQ(model.selected_text(), u"bc");
}

}  // namespace
}  // namespace ash
