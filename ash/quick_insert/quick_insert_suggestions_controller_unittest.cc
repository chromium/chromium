// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_suggestions_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/quick_insert/mock_quick_insert_client.h"
#include "ash/quick_insert/model/quick_insert_model.h"
#include "ash/test/ash_test_base.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

using ::base::test::RunOnceCallback;
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

class QuickInsertSuggestionsControllerTest : public testing::Test {
 public:
  QuickInsertSuggestionsControllerTest() {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
  }

  history::HistoryService* GetHistoryService() {
    return history_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
};

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenUnfocusedReturnsNewWindowResults) {
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
      .WillOnce(RunOnceCallback<0>(std::vector<QuickInsertSearchResult>{
          QuickInsertEditorResult(QuickInsertEditorResult::Mode::kRewrite, u"",
                                  {}, {}),
      }));
  QuickInsertSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
  QuickInsertSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/&input_field,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
  EXPECT_CALL(callback,
              Run(Contains(VariantWith<QuickInsertNewWindowResult>(_))))
      .Times(0);
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());

  controller.GetSuggestions(client, model, callback.Get());
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsWhenCapsOffReturnsCapsOn) {
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(false);
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(true);
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(GetHistoryService()));
  QuickInsertSuggestionsController controller;
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, &input_field, &keyboard,
                         QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
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
       GetSuggestionsRequestsAndReturnsSuggestionsPerCategory) {
  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();
  history_service->AddPageWithDetails(
      GURL("https://a.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("https://b.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(history_service));
  EXPECT_CALL(client, GetRecentDriveFileResults(5, _))
      .WillOnce(RunOnceCallback<1>(std::vector<QuickInsertSearchResult>{
          QuickInsertDriveFileResult(/*id=*/{}, u"a", GURL("a.com"),
                                     /*file_path=*/{}),
          QuickInsertDriveFileResult(/*id=*/{}, u"b", GURL("b.com"),
                                     /*file_path=*/{}),
      }));
  EXPECT_CALL(client, GetRecentLocalFileResults(3, _, _))
      .WillOnce(RunOnceCallback<2>(std::vector<QuickInsertSearchResult>{
          QuickInsertLocalFileResult(u"a", /*file_path=*/{}),
          QuickInsertLocalFileResult(u"b", /*file_path=*/{}),
          QuickInsertLocalFileResult(u"c", /*file_path=*/{}),
          QuickInsertLocalFileResult(u"d", /*file_path=*/{}),
      }));
  QuickInsertSuggestionsController controller;
  input_method::FakeImeKeyboard keyboard;
  QuickInsertModel model(/*prefs=*/nullptr, /*focused_client=*/nullptr,
                         &keyboard, QuickInsertModel::EditorStatus::kEnabled,
                         QuickInsertModel::LobsterStatus::kEnabled);

  base::test::TestFuture<void> history_future;
  base::MockCallback<QuickInsertSuggestionsController::SuggestionsCallback>
      callback;
  EXPECT_CALL(callback, Run).Times(AnyNumber());
  EXPECT_CALL(
      callback,
      Run(ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(_))))
      .WillOnce([&]() { history_future.SetValue(); });
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertDriveFileResult>(_))))
      .Times(1);
  EXPECT_CALL(callback,
              Run(ElementsAre(VariantWith<QuickInsertLocalFileResult>(_),
                              VariantWith<QuickInsertLocalFileResult>(_),
                              VariantWith<QuickInsertLocalFileResult>(_))))
      .Times(1);

  controller.GetSuggestions(client, model, callback.Get());
  ASSERT_TRUE(history_future.Wait());
}

TEST_F(QuickInsertSuggestionsControllerTest, GetSuggestionsForLinkCategory) {
  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();
  history_service->AddPageWithDetails(
      GURL("https://a.com"), /*title=*/u"a", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("https://b.com"), /*title=*/u"b", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  NiceMock<MockQuickInsertClient> client;
  EXPECT_CALL(client, GetHistoryService)
      .WillRepeatedly(Return(history_service));
  QuickInsertSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kLinks,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(
                              FieldsAre(GURL("https://a.com"), u"a", _, _)),
                          VariantWith<QuickInsertBrowsingHistoryResult>(
                              FieldsAre(GURL("https://b.com"), u"b", _, _))));
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
      .WillOnce(RunOnceCallback<1>(suggested_files));
  QuickInsertSuggestionsController controller;

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
      .WillOnce(RunOnceCallback<2>(suggested_files));
  QuickInsertSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kLocalFiles,
                                       future.GetRepeatingCallback());

  EXPECT_EQ(future.Take(), suggested_files);
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForDatesCategoryReturnsSomeResults) {
  NiceMock<MockQuickInsertClient> client;
  QuickInsertSuggestionsController controller;

  base::test::TestFuture<std::vector<QuickInsertSearchResult>> future;
  controller.GetSuggestionsForCategory(client, QuickInsertCategory::kDatesTimes,
                                       future.GetRepeatingCallback());

  EXPECT_THAT(future.Take(), Not(IsEmpty()));
}

TEST_F(QuickInsertSuggestionsControllerTest,
       GetSuggestionsForMathsCategoryReturnsSomeResults) {
  NiceMock<MockQuickInsertClient> client;
  QuickInsertSuggestionsController controller;

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
      .WillOnce(RunOnceCallback<0>(
          std::vector<ClipboardHistoryItem>{clipboard_item}));
  NiceMock<MockQuickInsertClient> client;
  QuickInsertSuggestionsController controller;

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
