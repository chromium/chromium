// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_suggestions_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/quick_insert/mock_quick_insert_client.h"
#include "ash/quick_insert/model/quick_insert_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::VariantWith;
using ::testing::WithArg;

auto RunCallbackArgWith(auto result) {
  return [result](auto callback) { return std::move(callback).Run(result); };
}

using QuickInsertSuggestionsControllerTest = testing::Test;

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenUnfocusedReturnsNewWindowResults) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(VariantWith<QuickInsertNewWindowResult>(_))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenSelectedTextReturnsEditorRewriteResults) {
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetSuggestedEditorResults)
      .WillRepeatedly(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
          QuickInsertEditorResult(QuickInsertEditorResult::Mode::kRewrite, u"",
                                  {}, {}),
      }));
  PickerSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(AllOf(Not(IsEmpty()),
                        Each(VariantWith<QuickInsertEditorResult>(
                            Field(&QuickInsertEditorResult::mode,
                                  QuickInsertEditorResult::Mode::kRewrite))))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWithSelectionReturnsLobsterResult) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(IsSupersetOf({
                  QuickInsertLobsterResult(
                      QuickInsertLobsterResult::Mode::kWithSelection, u""),
              })))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenFocusedDoesNotReturnNewWindowResults) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback,
              Run(Contains(VariantWith<QuickInsertNewWindowResult>(_))))
      .Times(0);
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenCapsOffReturnsCapsOn) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(false);
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(Contains(QuickInsertCapsLockResult(
          /*enabled=*/true, QuickInsertCapsLockResult::Shortcut::kAltSearch))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenCapsOnReturnsCapsOff) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(true);
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(Contains(QuickInsertCapsLockResult(
          /*enabled=*/false, QuickInsertCapsLockResult::Shortcut::kAltSearch))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWithSelectionReturnsCaseTransforms) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(IsSupersetOf({
                  QuickInsertCaseTransformResult(
                      QuickInsertCaseTransformResult::Type::kUpperCase),
                  QuickInsertCaseTransformResult(
                      QuickInsertCaseTransformResult::Type::kLowerCase),
                  QuickInsertCaseTransformResult(
                      QuickInsertCaseTransformResult::Type::kTitleCase),
              })))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWithNoSelectionDoesNotReturnCaseTransforms) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback, Run(Contains(QuickInsertCaseTransformResult(
                            QuickInsertCaseTransformResult::Type::kUpperCase))))
      .Times(0);
  EXPECT_CALL(callback, Run(Contains(QuickInsertCaseTransformResult(
                            QuickInsertCaseTransformResult::Type::kLowerCase))))
      .Times(0);
  EXPECT_CALL(callback, Run(Contains(QuickInsertCaseTransformResult(
                            QuickInsertCaseTransformResult::Type::kTitleCase))))
      .Times(0);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsRequestsAndReturnsOneSuggestionPerCategory) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ash::features::kPickerGrid);
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetSuggestedLinkResults(_, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertBrowsingHistoryResult(GURL("a.com"), u"a",
                                               /*icon=*/{}),
              QuickInsertBrowsingHistoryResult(GURL("b.com"), u"b",
                                               /*icon=*/{}),
          })));
  EXPECT_CALL(client, GetRecentDriveFileResults(5, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertDriveFileResult(/*id=*/{}, u"a", GURL("a.com"),
                                         /*file_path=*/{}),
              QuickInsertDriveFileResult(/*id=*/{}, u"b", GURL("b.com"),
                                         /*file_path=*/{}),
          })));
  EXPECT_CALL(client, GetRecentLocalFileResults(1, _, _))
      .WillRepeatedly(
          WithArg<2>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertLocalFileResult(u"a", /*file_path=*/{}),
              QuickInsertLocalFileResult(u"b", /*file_path=*/{}),
          })));
  PickerSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(_))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertDriveFileResult>(_))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertLocalFileResult>(_))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsRequestsAndReturnsSuggestionsPerCategory) {
  base::test::ScopedFeatureList feature_list(ash::features::kPickerGrid);
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetSuggestedLinkResults(_, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertBrowsingHistoryResult(GURL("a.com"), u"a",
                                               /*icon=*/{}),
              QuickInsertBrowsingHistoryResult(GURL("b.com"), u"b",
                                               /*icon=*/{}),
          })));
  EXPECT_CALL(client, GetRecentDriveFileResults(5, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertDriveFileResult(/*id=*/{}, u"a", GURL("a.com"),
                                         /*file_path=*/{}),
              QuickInsertDriveFileResult(/*id=*/{}, u"b", GURL("b.com"),
                                         /*file_path=*/{}),
          })));
  EXPECT_CALL(client, GetRecentLocalFileResults(3, _, _))
      .WillRepeatedly(
          WithArg<2>(RunCallbackArgWith(std::vector<QuickInsertSearchResult>{
              QuickInsertLocalFileResult(u"a", /*file_path=*/{}),
              QuickInsertLocalFileResult(u"b", /*file_path=*/{}),
              QuickInsertLocalFileResult(u"c", /*file_path=*/{}),
              QuickInsertLocalFileResult(u"d", /*file_path=*/{}),
          })));
  PickerSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(_))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertDriveFileResult>(_))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertLocalFileResult>(_),
                              VariantWith<QuickInsertLocalFileResult>(_),
                              VariantWith<QuickInsertLocalFileResult>(_))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest, GetSuggestionsForLinkCategory) {
  const std::vector<QuickInsertSearchResult> suggested_links = {
      QuickInsertBrowsingHistoryResult(GURL("a.com"), u"a", /*icon=*/{}),
      QuickInsertBrowsingHistoryResult(GURL("b.com"), u"b", /*icon=*/{}),
  };
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetSuggestedLinkResults)
      .WillRepeatedly(WithArg<1>(RunCallbackArgWith(suggested_links)));
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kLinks,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_links);
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForDriveFileCategory) {
  const std::vector<QuickInsertSearchResult> suggested_files = {
      QuickInsertDriveFileResult(/*id=*/{}, u"a", GURL("a.com"),
                                 /*file_path=*/{}),
      QuickInsertDriveFileResult(/*id=*/{}, u"b", GURL("b.com"),
                                 /*file_path=*/{}),
  };
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetRecentDriveFileResults)
      .WillRepeatedly(WithArg<1>(RunCallbackArgWith(suggested_files)));
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kDriveFiles,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_files);
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForLocalFileCategory) {
  const std::vector<QuickInsertSearchResult> suggested_files = {
      QuickInsertLocalFileResult(u"a", /*file_path=*/{}),
      QuickInsertLocalFileResult(u"b", /*file_path=*/{}),
  };
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetRecentLocalFileResults)
      .WillRepeatedly(WithArg<2>(RunCallbackArgWith(suggested_files)));
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kLocalFiles,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_files);
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForDatesCategoryReturnsSomeResults) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kDatesTimes,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), Not(IsEmpty()));
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForMathsCategoryReturnsSomeResults) {
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kUnitsMaths,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), Not(IsEmpty()));
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForClipboardCategory) {
  ClipboardHistoryItem clipboard_item =
      ClipboardHistoryItemBuilder()
          .SetFormat(ui::ClipboardInternalFormat::kText)
          .SetText("abc")
          .Build();
  MockClipboardHistoryController mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(RunCallbackArgWith(
          std::vector<ClipboardHistoryItem>{clipboard_item}));
  NiceMock<MockQuickInsertClient> client;
  PickerSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kClipboard,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(VariantWith<QuickInsertClipboardResult>(
                  FieldsAre(_, QuickInsertClipboardResult::DisplayFormat::kText,
                            _, u"abc", _, _))));
}

}  // namespace
}  // namespace ash
