// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_suggestions_controller.h"

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
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

using PickerSuggestionsControllerTest = AshTestBase;

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenUnfocusedReturnsNewWindowResults) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*focused_client=*/nullptr, &keyboard,
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
      .WillRepeatedly(
          [](PickerClient::SuggestedEditorResultsCallback callback) {
            std::move(callback).Run({
                PickerSearchResult::Editor(
                    PickerSearchResult::EditorData::Mode::kRewrite, u"", {},
                    {}),
            });
          });
  PickerSuggestionsController controller(&client);
  ui::FakeTextInputClient input_field({.type = ui::TEXT_INPUT_TYPE_TEXT});
  input_field.SetTextAndSelection(u"a", gfx::Range(0, 1));
  input_method::FakeImeKeyboard keyboard;
  PickerModel model(/*focused_client=*/&input_field, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

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
  PickerModel model(/*focused_client=*/&input_field, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

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
  PickerModel model(/*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback, Run(Contains(PickerSearchResult::CapsLock(true))))
      .Times(1);

  controller.GetSuggestions(model, callback.Get());
}

TEST_F(PickerSuggestionsControllerTest,
       GetSuggestionsWhenCapsOnReturnsCapsOff) {
  NiceMock<MockPickerClient> client;
  PickerSuggestionsController controller(&client);
  input_method::FakeImeKeyboard keyboard;
  keyboard.SetCapsLockEnabled(true);
  PickerModel model(/*focused_client=*/nullptr, &keyboard,
                    PickerModel::EditorStatus::kEnabled);

  base::MockCallback<PickerSuggestionsController::SuggestionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(AnyNumber());
  EXPECT_CALL(callback, Run(Contains(PickerSearchResult::CapsLock(false))))
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
  PickerModel model(&input_field, &keyboard,
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
          PickerSearchResult::CaseTransform(
              PickerSearchResult::CaseTransformData::Type::kSentenceCase),
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
  PickerModel model(&input_field, &keyboard,
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
  EXPECT_CALL(callback,
              Run(Contains(PickerSearchResult::CaseTransform(
                  PickerSearchResult::CaseTransformData::Type::kSentenceCase))))
      .Times(0);

  controller.GetSuggestions(model, callback.Get());
}

}  // namespace
}  // namespace ash
