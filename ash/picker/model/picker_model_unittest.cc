// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/picker_category.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Not;

TEST(PickerModel, AvailableCategoriesWithNoFocusHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                    &fake_ime_keyboard, PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              ElementsAre(PickerCategory::kLinks, PickerCategory::kDriveFiles,
                          PickerCategory::kLocalFiles));
}

TEST(PickerModel, AvailableCategoriesWithNoSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEditorWrite, PickerCategory::kLobster,
                  PickerCategory::kLinks, PickerCategory::kEmojis,
                  PickerCategory::kClipboard, PickerCategory::kDriveFiles,
                  PickerCategory::kLocalFiles, PickerCategory::kDatesTimes,
                  PickerCategory::kUnitsMaths));
}

TEST(PickerModel, AvailableCategoriesWithSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEditorRewrite, PickerCategory::kLobster));
}

TEST(PickerModel, AvailableCategoriesContainsEditorWriteWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEditorWrite));
}

TEST(PickerModel, AvailableCategoriesOmitsEditorWriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kDisabled,
                    PickerModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorWrite)));
}

TEST(PickerModel, AvailableCategoriesContainsEditorRewriteWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEditorRewrite));
}

TEST(PickerModel, AvailableCategoriesOmitsEditorRewriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kDisabled,
                    PickerModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorRewrite)));
}

TEST(PickerModel, AvailableCategoriesContainsLobsterWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kDisabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kLobster));
}

TEST(PickerModel, AvailableCategoriesOmitsLobsterWriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kDisabled,
                    PickerModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kLobster)));
}

TEST(PickerModel, AvailableCategoriesContainsEmojisAndGifsWhenGifsEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/&prefs, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEmojisGifs));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojis)));
}

TEST(PickerModel, AvailableCategoriesContainsOnlyEmojisWhenGifsDisables) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        false);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/&prefs, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEmojis));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojisGifs)));
}

TEST(PickerModel, AvailableCategoriesDoesNotContainExpressionsForUrlFields) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_URL});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojis)));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojisGifs)));
}

TEST(PickerModel, GetsEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 1));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_EQ(model.selected_text(), u"");
}

TEST(PickerModel, GetsNonEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 3));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);
  EXPECT_EQ(model.selected_text(), u"bc");
}

TEST(PickerModel, GetModeForUnfocusedState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                    &fake_ime_keyboard, PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kUnfocused);
}

TEST(PickerModel, GetModeForInputTypeNone) {
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_NONE});
  input_method::FakeImeKeyboard fake_ime_keyboard;
  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kUnfocused);
}

TEST(PickerModel, GetModeForNoSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kNoSelection);
}

TEST(PickerModel, GetModeForSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd efgh", gfx::Range(1, 5));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kHasSelection);
}

TEST(PickerModel, GifsDisabledWhenPrefDoesNotExist) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(&prefs, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.IsGifsEnabled());
}

TEST(PickerModel, GifsEnabledWhenPrefIsTrue) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  prefs.SetBoolean(prefs::kEmojiPickerGifSupportEnabled, true);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(&prefs, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.IsGifsEnabled());
}

TEST(PickerModel, GifsDisabledWhenPrefIsFalse) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  prefs.SetBoolean(prefs::kEmojiPickerGifSupportEnabled, false);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  PickerModel model(&prefs, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.IsGifsEnabled());
}

TEST(PickerModel, GetModeForBlankStringsSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"  \n \t\ra", gfx::Range(0, 5));

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kNoSelection);
}

TEST(PickerModel, UnfocusedShouldLearn) {
  input_method::FakeImeKeyboard fake_ime_keyboard;

  PickerModel model(/*prefs=*/nullptr, nullptr, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.should_do_learning());
}

TEST(PickerModel, FocusedShouldLearnIfLearningEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = true});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.should_do_learning());
}

TEST(PickerModel, FocusedShouldLearnIfLearningDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = false});

  PickerModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                    PickerModel::EditorStatus::kEnabled,
                    PickerModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.should_do_learning());
}

}  // namespace
}  // namespace ash
