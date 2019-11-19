// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_keyboard_accessory_adapter.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace autofill {
namespace {

class MockAccessoryView
    : public AutofillKeyboardAccessoryAdapter::AccessoryView {
 public:
  MockAccessoryView() {}
  MOCK_METHOD2(Initialize, void(unsigned int, bool));
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD3(ConfirmDeletion,
               void(const base::string16&,
                    const base::string16&,
                    base::OnceClosure));
  MOCK_METHOD1(GetElidedValueWidthForRow, int(int));
  MOCK_METHOD1(GetElidedLabelWidthForRow, int(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAccessoryView);
};

Suggestion createPasswordEntry(std::string password,
                               std::string username,
                               std::string psl_origin) {
  Suggestion s(/*value=*/username, /*label=*/psl_origin, /*icon=*/"",
               PopupItemId::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
  s.additional_label = ASCIIToUTF16(password);
  return s;
}

std::vector<Suggestion> createSuggestions() {
  std::vector<Suggestion> suggestions = {
      createPasswordEntry("****************", "Alf", ""),
      createPasswordEntry("****************", "Berta", "psl.origin.eg"),
      createPasswordEntry("***", "Carl", "")};
  return suggestions;
}

std::vector<Suggestion> createSuggestions(int clearItemOffset) {
  std::vector<Suggestion> suggestions = createSuggestions();
  suggestions.emplace(
      suggestions.begin() + clearItemOffset,
      Suggestion("Clear", "", "", PopupItemId::POPUP_ITEM_ID_CLEAR_FORM));
  return suggestions;
}

// Matcher returning true if suggestions have equal members.
MATCHER_P(equalsSuggestion, other, "") {
  if (arg.frontend_id != other.frontend_id) {
    *result_listener << "has frontend_id " << arg.frontend_id;
    return false;
  }
  if (arg.value != other.value) {
    *result_listener << "has value " << arg.value;
    return false;
  }
  if (arg.label != other.label) {
    *result_listener << "has label " << arg.label;
    return false;
  }
  if (arg.icon != other.icon) {
    *result_listener << "has icon " << arg.icon;
    return false;
  }
  return true;
}

}  // namespace

// Automagically used to pretty-print Suggestion. Must be in same namespace.
void PrintTo(const Suggestion& suggestion, std::ostream* os) {
  *os << "(value: \"" << suggestion.value << "\", label: \"" << suggestion.label
      << "\", frontend_id: " << suggestion.frontend_id
      << ", additional_label: \"" << suggestion.additional_label << "\")";
}

class AutofillKeyboardAccessoryAdapterTest : public testing::Test {
 public:
  AutofillKeyboardAccessoryAdapterTest()
      : popup_controller_(
            std::make_unique<StrictMock<MockAutofillPopupController>>()) {
    auto view = std::make_unique<StrictMock<MockAccessoryView>>();
    accessory_view_ = view.get();

    autofill_accessory_adapter_ =
        std::make_unique<AutofillKeyboardAccessoryAdapter>(
            popup_controller_->GetWeakPtr(), 0, false);
    autofill_accessory_adapter_->SetAccessoryView(std::move(view));
  }

  void NotifyAboutSuggestions() {
    EXPECT_CALL(*view(), Show());

    adapter_as_view()->OnSuggestionsChanged();

    testing::Mock::VerifyAndClearExpectations(view());
  }

  const Suggestion& suggestion(int i) {
    return controller()->GetSuggestionAt(i);
  }

  AutofillPopupController* adapter_as_controller() {
    return autofill_accessory_adapter_.get();
  }

  AutofillPopupView* adapter_as_view() {
    return autofill_accessory_adapter_.get();
  }

  MockAutofillPopupController* controller() { return popup_controller_.get(); }

  MockAccessoryView* view() { return accessory_view_; }

 private:
  StrictMock<MockAccessoryView>* accessory_view_;
  std::unique_ptr<StrictMock<MockAutofillPopupController>> popup_controller_;
  std::unique_ptr<AutofillKeyboardAccessoryAdapter> autofill_accessory_adapter_;
};

TEST_F(AutofillKeyboardAccessoryAdapterTest, ShowingInitializesAndUpdatesView) {
  {
    ::testing::Sequence s;
    EXPECT_CALL(*view(), Initialize(_, _));
    EXPECT_CALL(*view(), Show());
  }
  adapter_as_view()->Show();
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, HidingAdapterHidesView) {
  EXPECT_CALL(*view(), Hide());
  adapter_as_view()->Hide();
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, ReorderUpdatedSuggestions) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/2));
  EXPECT_CALL(*view(), Show());

  adapter_as_view()->OnSuggestionsChanged();

  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(0),
              equalsSuggestion(suggestion(2)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(1),
              equalsSuggestion(suggestion(0)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(2),
              equalsSuggestion(suggestion(1)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(3),
              equalsSuggestion(suggestion(3)));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, UseAdditionalLabelForElidedLabel) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/1));
  NotifyAboutSuggestions();

  // The 1st item is usually not visible (something like clear form) and has an
  // empty label. But it needs to be handled since UI might ask for it anyway.
  EXPECT_EQ(adapter_as_controller()->GetElidedLabelAt(0), base::string16());

  // If there is a label, use it but cap at 8 bullets.
  EXPECT_EQ(adapter_as_controller()->GetElidedLabelAt(1),
            ASCIIToUTF16("********"));

  // If the label is empty, use the additional label:
  EXPECT_EQ(adapter_as_controller()->GetElidedLabelAt(2),
            ASCIIToUTF16("psl.origin.eg ********"));

  // If the password has less than 8 bullets, show the exact amount.
  EXPECT_EQ(adapter_as_controller()->GetElidedLabelAt(3), ASCIIToUTF16("***"));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, ProvideReorderedSuggestions) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/2));
  NotifyAboutSuggestions();

  EXPECT_THAT(adapter_as_controller()->GetSuggestions(),
              testing::ElementsAre(equalsSuggestion(suggestion(2)),
                                   equalsSuggestion(suggestion(0)),
                                   equalsSuggestion(suggestion(1)),
                                   equalsSuggestion(suggestion(3))));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, RemoveAfterConfirmation) {
  controller()->set_suggestions(createSuggestions());
  NotifyAboutSuggestions();

  base::OnceClosure confirm;
  EXPECT_CALL(*controller(), GetRemovalConfirmationText(0, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*view(), ConfirmDeletion(_, _, _))
      .WillOnce(WithArg<2>(Invoke([&](base::OnceClosure closure) -> void {
        confirm = std::move(closure);
      })));
  EXPECT_TRUE(adapter_as_controller()->RemoveSuggestion(0));

  EXPECT_CALL(*controller(), RemoveSuggestion(0)).WillOnce(Return(true));
  std::move(confirm).Run();
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, MapSelectedLineToChangedIndices) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/2));
  NotifyAboutSuggestions();

  EXPECT_CALL(*controller(), SetSelectedLine(base::Optional<int>(0)));
  adapter_as_controller()->SetSelectedLine(1);

  EXPECT_CALL(*controller(), selected_line()).WillRepeatedly(Return(0));
  EXPECT_EQ(adapter_as_controller()->selected_line(), 1);

  EXPECT_CALL(*controller(), AcceptSelectedLine());
  adapter_as_controller()->AcceptSelectedLine();
}

}  // namespace autofill
