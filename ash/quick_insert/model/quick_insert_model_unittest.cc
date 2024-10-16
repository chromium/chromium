// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_insert/model/quick_insert_mode_type.h"
#include "ash/quick_insert/quick_insert_category.h"
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

TEST(QuickInsertModelTest, AvailableCategoriesWithNoFocusHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              ElementsAre(PickerCategory::kLinks, PickerCategory::kDriveFiles,
                          PickerCategory::kLocalFiles));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesWithNoSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEditorWrite, PickerCategory::kLobster,
                  PickerCategory::kLinks, PickerCategory::kEmojis,
                  PickerCategory::kClipboard, PickerCategory::kDriveFiles,
                  PickerCategory::kLocalFiles, PickerCategory::kDatesTimes,
                  PickerCategory::kUnitsMaths));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesWithSelectedTextHasCorrectOrdering) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(
      model.GetAvailableCategories(),
      ElementsAre(PickerCategory::kEditorRewrite, PickerCategory::kLobster));
}

TEST(QuickInsertModelTest, AvailableCategoriesContainsEditorWriteWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEditorWrite));
}

TEST(QuickInsertModelTest, AvailableCategoriesOmitsEditorWriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kDisabled,
                         QuickInsertModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorWrite)));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesContainsEditorRewriteWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEditorRewrite));
}

TEST(QuickInsertModelTest, AvailableCategoriesOmitsEditorRewriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"a", gfx::Range(0, 1));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kDisabled,
                         QuickInsertModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEditorRewrite)));
}

TEST(QuickInsertModelTest, AvailableCategoriesContainsLobsterWhenEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kDisabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kLobster));
}

TEST(QuickInsertModelTest, AvailableCategoriesOmitsLobsterWriteWhenDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kDisabled,
                         QuickInsertModel::LobsterStatus::kDisabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kLobster)));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesContainsEmojisAndGifsWhenGifsEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/&prefs, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEmojisGifs));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojis)));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesContainsOnlyEmojisWhenGifsDisables) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        false);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/&prefs, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Contains(PickerCategory::kEmojis));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojisGifs)));
}

TEST(QuickInsertModelTest,
     AvailableCategoriesDoesNotContainExpressionsForUrlFields) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_URL});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojis)));
  EXPECT_THAT(model.GetAvailableCategories(),
              Not(Contains(PickerCategory::kEmojisGifs)));
}

TEST(QuickInsertModelTest, GetsEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 1));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_EQ(model.selected_text(), u"");
}

TEST(QuickInsertModelTest, GetsNonEmptySelectedText) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 3));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);
  EXPECT_EQ(model.selected_text(), u"bc");
}

TEST(QuickInsertModelTest, GetModeForUnfocusedState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kUnfocused);
}

TEST(QuickInsertModelTest, GetModeForInputTypeNone) {
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_NONE});
  input_method::FakeImeKeyboard fake_ime_keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kUnfocused);
}

TEST(QuickInsertModelTest, GetModeForNoSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kNoSelection);
}

TEST(QuickInsertModelTest, GetModeForSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"abcd efgh", gfx::Range(1, 5));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kHasSelection);
}

TEST(QuickInsertModelTest, GifsDisabledWhenPrefDoesNotExist) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(&prefs, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.IsGifsEnabled());
}

TEST(QuickInsertModelTest, GifsEnabledWhenPrefIsTrue) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  prefs.SetBoolean(prefs::kEmojiPickerGifSupportEnabled, true);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(&prefs, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.IsGifsEnabled());
}

TEST(QuickInsertModelTest, GifsDisabledWhenPrefIsFalse) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kEmojiPickerGifSupportEnabled,
                                        true);
  prefs.SetBoolean(prefs::kEmojiPickerGifSupportEnabled, false);
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});

  QuickInsertModel model(&prefs, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.IsGifsEnabled());
}

TEST(QuickInsertModelTest, GetModeForBlankStringsSelectionState) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client({.type = ui::TEXT_INPUT_TYPE_TEXT});
  client.SetTextAndSelection(u"  \n \t\ra", gfx::Range(0, 5));

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_EQ(model.GetMode(), PickerModeType::kNoSelection);
}

TEST(QuickInsertModelTest, UnfocusedShouldLearn) {
  input_method::FakeImeKeyboard fake_ime_keyboard;

  QuickInsertModel model(/*prefs=*/nullptr, nullptr, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.should_do_learning());
}

TEST(QuickInsertModelTest, FocusedShouldLearnIfLearningEnabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = true});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_TRUE(model.should_do_learning());
}

TEST(QuickInsertModelTest, FocusedShouldLearnIfLearningDisabled) {
  input_method::FakeImeKeyboard fake_ime_keyboard;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .should_do_learning = false});

  QuickInsertModel model(/*prefs=*/nullptr, &client, &fake_ime_keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  EXPECT_FALSE(model.should_do_learning());
}

}  // namespace
}  // namespace ash
