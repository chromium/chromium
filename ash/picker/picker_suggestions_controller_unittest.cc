// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_suggestions_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/model/picker_model.h"
#include "ash/public/cpp/picker/mock_picker_client.h"
#include "ash/test/ash_test_base.h"
#include "base/test/mock_callback.h"
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

using PickerSuggestionsControllerTest = testing::Test;

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenUnfocusedReturnsNewWindowResults) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(
                  Property(&PickerSearchResult::data,
                           VariantWith<PickerSearchResult::NewWindowData>(_)))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenSelectedTextReturnsEditorRewriteResults) {
  NiceMock<MockPickerClient> client;
  EXPECT_CALL(client, GetSuggestedEditorResults)
      .WillRepeatedly(RunCallbackArgWith(std::vector<PickerSearchResult>{
          PickerSearchResult::Editor(
              PickerSearchResult::EditorData::Mode::kRewrite, u"", {}, {}),
      }));
  PickerSuggestionsController controller(&client);
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                    &keyboard, PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(AllOf(Not(IsEmpty()),
                Each(Property(
                    "data", &PickerSearchResult::data,
                    VariantWith<PickerSearchResult::EditorData>(Field(
                        &PickerSearchResult::EditorData::mode,
                        PickerSearchResult::EditorData::Mode::kRewrite)))))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenFocusedDoesNotReturnNewWindowResults) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                    &keyboard, PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback,
              Run(Contains(
                  Property(&PickerSearchResult::data,
                           VariantWith<PickerSearchResult::NewWindowData>(_)))))
      .Times(0);
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenCapsOffReturnsCapsOn) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(false);
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CapsLock(
                  /*enabled=*/true,
                  PickerSearchResult::CapsLockData::Shortcut::kAltSearch))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenCapsOnReturnsCapsOff) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(true);
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CapsLock(
                  /*enabled=*/false,
                  PickerSearchResult::CapsLockData::Shortcut::kAltSearch))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWithSelectionReturnsCaseTransforms) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(IsSupersetOf({
          PickerSearchResult::CaseTransform(
              PickerSearchResult::CaseTransformData::Type::kUpperCase),
          PickerSearchResult::CaseTransform(
              PickerSearchResult::CaseTransformData::Type::kLowerCase),
          PickerSearchResult::CaseTransform(
              PickerSearchResult::CaseTransformData::Type::kTitleCase),
      })))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWithNoSelectionDoesNotReturnCaseTransforms) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CaseTransform(
                  PickerSearchResult::CaseTransformData::Type::kUpperCase))))
      .Times(0);
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CaseTransform(
                  PickerSearchResult::CaseTransformData::Type::kLowerCase))))
      .Times(0);
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CaseTransform(
                  PickerSearchResult::CaseTransformData::Type::kTitleCase))))
      .Times(0);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsRequestsAndReturnsOneSuggestionPerCategory) {
  NiceMock<MockPickerClient> client;
  EXPECT_CALL(client, GetSuggestedLinkResults(1, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<PickerSearchResult>{
              PickerSearchResult::BrowsingHistory(GURL("a.com"), u"a",
                                                  /*icon=*/{}),
              PickerSearchResult::BrowsingHistory(GURL("b.com"), u"b",
                                                  /*icon=*/{}),
          })));
  EXPECT_CALL(client, GetRecentDriveFileResults(5, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<PickerSearchResult>{
              PickerSearchResult::DriveFile(/*id=*/{}, u"a", GURL("a.com"),
                                            /*file_path=*/{}),
              PickerSearchResult::DriveFile(/*id=*/{}, u"b", GURL("b.com"),
                                            /*file_path=*/{}),
          })));
  EXPECT_CALL(client, GetRecentLocalFileResults(1, _))
      .WillRepeatedly(
          WithArg<1>(RunCallbackArgWith(std::vector<PickerSearchResult>{
              PickerSearchResult::LocalFile(u"a", /*file_path=*/{}),
              PickerSearchResult::LocalFile(u"b", /*file_path=*/{}),
          })));
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(AnyNumber());
  EXPECT_CALL(callback,
              Run(ElementsAre(Property(
                  &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::BrowsingHistoryData>(_)))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Property(&PickerSearchResult::data,
                           VariantWith<PickerSearchResult::DriveFileData>(_)))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Property(&PickerSearchResult::data,
                           VariantWith<PickerSearchResult::LocalFileData>(_)))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest, GetSuggestionsForLinkCategory) {
  const std::vector<PickerSearchResult> suggested_links = {
      PickerSearchResult::BrowsingHistory(GURL("a.com"), u"a", /*icon=*/{}),
      PickerSearchResult::BrowsingHistory(GURL("b.com"), u"b", /*icon=*/{}),
  };
  NiceMock<MockPickerClient> client;
  EXPECT_CALL(client, GetSuggestedLinkResults)
      .WillRepeatedly(WithArg<1>(RunCallbackArgWith(suggested_links)));
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kLinks,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_links);
}

TEST_F(PickerSuggestionsControllerTest, GetSuggestionsForDriveFileCategory) {
  const std::vector<PickerSearchResult> suggested_files = {
      PickerSearchResult::DriveFile(/*id=*/{}, u"a", GURL("a.com"),
                                    /*file_path=*/{}),
      PickerSearchResult::DriveFile(/*id=*/{}, u"b", GURL("b.com"),
                                    /*file_path=*/{}),
  };
  NiceMock<MockPickerClient> client;
  EXPECT_CALL(client, GetRecentDriveFileResults)
      .WillRepeatedly(WithArg<1>(RunCallbackArgWith(suggested_files)));
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kDriveFiles,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_files);
}

TEST_F(PickerSuggestionsControllerTest, GetSuggestionsForLocalFileCategory) {
  const std::vector<PickerSearchResult> suggested_files = {
      PickerSearchResult::LocalFile(u"a", /*file_path=*/{}),
      PickerSearchResult::LocalFile(u"b", /*file_path=*/{}),
  };
  NiceMock<MockPickerClient> client;
  EXPECT_CALL(client, GetRecentLocalFileResults)
      .WillRepeatedly(WithArg<1>(RunCallbackArgWith(suggested_files)));
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kLocalFiles,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_files);
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsForDatesCategoryReturnsSomeResults) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kDatesTimes,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), Not(IsEmpty()));
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsForMathsCategoryReturnsSomeResults) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kUnitsMaths,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), Not(IsEmpty()));
}

TEST_F(PickerSuggestionsControllerTest, GetSuggestionsForClipboardCategory) {
  ClipboardHistoryItem clipboard_item =
      ClipboardHistoryItemBuilder()
          .SetFormat(ui::ClipboardInternalFormat::kText)
          .SetText("abc")
          .Build();
  MockClipboardHistoryController mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(RunCallbackArgWith(
          std::vector<ClipboardHistoryItem>{clipboard_item}));
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  controller.GetSuggestionsForCategory(PickerCategory::kClipboard,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(
      future.Take(),
      ElementsAre(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
              _, PickerSearchResult::ClipboardData::DisplayFormat::kText, _,
              u"abc", _, _)))));
}

}  // namespace
}  // namespace ash
