// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_at_memory_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessorySuggestionType;
using autofill::AccessoryTabType;
using autofill::NavigationAction;
using autofill::mojom::FocusedFieldType;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::Values;
using testing::WithParamInterface;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using WaitForKeyboard = ManualFillingViewInterface::WaitForKeyboard;
using ShouldShowOnLargeFormFactor =
    ManualFillingViewInterface::ShouldShowOnLargeFormFactor;
using IsUnstructuredField = ManualFillingViewInterface::IsUnstructuredField;

AccessorySheetData filled_passwords_sheet() {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS, u"Pwds")
      .AddUserInfo("example.com", autofill::UserInfo::IsExactMatch(true))
      .AppendField(AccessorySuggestionType::kCredentialUsername, u"Ben", u"Ben",
                   false, true)
      .AppendField(AccessorySuggestionType::kCredentialPassword, u"S3cur3",
                   u"Ben's PW", true, false)
      .Build();
}

AccessorySheetData populate_sheet(AccessoryTabType type) {
  constexpr char16_t kTitle[] = u"Suggestions available!";
  return AccessorySheetData::Builder(type, kTitle).AddUserInfo().Build();
}

std::vector<uint8_t> test_passkey_id() {
  return {23, 24, 25, 26, 27};
}

const autofill::FieldGlobalId kFocusedFieldId{
    autofill::LocalFrameToken(base::UnguessableToken::CreateForTesting(1, 2)),
    autofill::FieldRendererId(123)};

// Fixture that tests the manual filling experience with the most recent version
// of the keyboard accessory and all its fallback sheets.
class ManualFillingControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->WasShown();

    EXPECT_CALL(mock_pwd_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&pwd_source_observer_));

    EXPECT_CALL(mock_payment_method_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&cc_source_observer_));
    EXPECT_CALL(mock_address_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&address_source_observer_));
    EXPECT_CALL(mock_at_memory_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&at_memory_source_observer_));
    ON_CALL(mock_at_memory_controller_, IsAtMemoryAvailable)
        .WillByDefault(Return(true));
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(),
        mock_payment_method_controller_.AsWeakPtr(),
        mock_at_memory_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
  }

  void FocusFieldAndClearExpectations(FocusedFieldType fieldType) {
    // Depending on |fieldType|, different calls can be expected. All of them
    // are irrelevant during setup.
    controller()->NotifyFocusedInputChanged(kFocusedFieldId, fieldType);
    testing::Mock::VerifyAndClearExpectations(view());
  }

  ManualFillingControllerImpl* controller() {
    return ManualFillingControllerImpl::FromWebContents(web_contents());
  }

  MockManualFillingView* view() {
    return static_cast<MockManualFillingView*>(controller()->view());
  }

  void NotifyPasswordSourceObserver(IsFillingSourceAvailable source_available) {
    pwd_source_observer_.Run(&mock_pwd_controller_, source_available);
  }

  void NotifyCreditCardSourceObserver(
      IsFillingSourceAvailable source_available) {
    cc_source_observer_.Run(&mock_payment_method_controller_, source_available);
  }

  void NotifyAddressSourceObserver(IsFillingSourceAvailable source_available) {
    address_source_observer_.Run(&mock_address_controller_, source_available);
  }

  void NotifyAtMemorySourceObserver(IsFillingSourceAvailable source_available) {
    at_memory_source_observer_.Run(&mock_at_memory_controller_,
                                   source_available);
  }

 protected:
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockPaymentMethodAccessoryController>
      mock_payment_method_controller_;
  NiceMock<MockAtMemoryAccessoryController> mock_at_memory_controller_;

  AccessoryController::FillingSourceObserver pwd_source_observer_;
  AccessoryController::FillingSourceObserver cc_source_observer_;
  AccessoryController::FillingSourceObserver address_source_observer_;
  AccessoryController::FillingSourceObserver at_memory_source_observer_;
};

TEST_F(ManualFillingControllerTest, ShowsAccessoryForAutofillOnSearchField) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillEnableKeyboardAccessoryOnSearchFields);

  FocusFieldAndClearExpectations(FocusedFieldType::kFillableSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding autofill doesn't hide the accessory because fallbacks are still
  // available.
  EXPECT_CALL(*view(), Hide()).Times(0);
  EXPECT_CALL(*view(), Show).Times(0);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest, PasswordAccessoryControllerReturnsNoData) {
  EXPECT_CALL(mock_pwd_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*view(), OnItemsAvailable).Times(0);
  EXPECT_CALL(*view(), Hide());

  NotifyPasswordSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForPasswordsTriggeredByObserver) {
  // TODO(crbug.com/40165275): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_pwd_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(filled_passwords_sheet()));
  EXPECT_CALL(*view(), OnItemsAvailable(filled_passwords_sheet()))
      .Times(AnyNumber());
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));

  NotifyPasswordSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Hide());
  NotifyPasswordSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest, AddressAccessoryControllerReturnsNoData) {
  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*view(), OnItemsAvailable).Times(0);
  EXPECT_CALL(*view(), Hide());

  NotifyAddressSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForAddressesTriggeredByObserver) {
  const AccessorySheetData kTestAddressSheet =
      populate_sheet(AccessoryTabType::ADDRESSES);

  // TODO(crbug.com/40165275): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestAddressSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(AnyNumber());
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));

  NotifyAddressSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyAddressSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest,
       CreditCardAccessoryControllerReturnsNoData) {
  EXPECT_CALL(mock_payment_method_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*view(), OnItemsAvailable).Times(0);
  EXPECT_CALL(*view(), Hide());

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForCreditCardsTriggeredByObserver) {
  const AccessorySheetData kTestCreditCardSheet =
      populate_sheet(AccessoryTabType::CREDIT_CARDS);

  // TODO(crbug.com/40165275): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_payment_method_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestCreditCardSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestCreditCardSheet))
      .Times(AnyNumber());
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest, HidesAccessoryWithoutAvailableSources) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  // This duplicate call is a noop.
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding just one of two active filling sources won't have any effect at all.
  EXPECT_CALL(*view(), Hide()).Times(0);
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)))
      .Times(0);
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest, ForwardsCredManActionToPasswordController) {
  EXPECT_CALL(
      mock_pwd_controller_,
      OnOptionSelected(AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY));
  controller()->OnOptionSelected(
      AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
}

TEST_F(ManualFillingControllerTest,
       ForwardsPasskeySelectionToPasswordController) {
  EXPECT_CALL(mock_pwd_controller_, OnPasskeySelected(Eq(test_passkey_id())));
  EXPECT_CALL(*view(), Hide());  // Make room for passkey sheet!
  controller()->OnPasskeySelected(AccessoryTabType::PASSWORDS,
                                  test_passkey_id());
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnUnknownField) {
  FocusFieldAndClearExpectations(FocusedFieldType::kUnknown);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(false), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  // Noop duplicate call.
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceNotAvailableOnCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));

  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnNonCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceNotAvailableNonCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest, ForwardsFillingTriggeredToController) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  autofill::AccessorySheetField selected_field =
      autofill::AccessorySheetField::Builder()
          .SetSuggestionType(AccessorySuggestionType::kCreditCardNumber)
          .SetDisplayText(u"4111111111111111")
          .SetSelectable(true)
          .Build();
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  EXPECT_CALL(mock_payment_method_controller_,
              OnFillingTriggered(kFocusedFieldId, selected_field));

  controller()->OnFillingTriggered(AccessoryTabType::CREDIT_CARDS,
                                   selected_field);
}

TEST_F(ManualFillingControllerTest, LogsHistogramOnFillingTriggered) {
  base::HistogramTester histogram_tester;
  // User selects non credential field that does not have autofill suggestions.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  autofill::AccessorySheetField selected_field =
      autofill::AccessorySheetField::Builder()
          .SetSuggestionType(AccessorySuggestionType::kCreditCardNumber)
          .SetDisplayText(u"4111111111111111")
          .SetSelectable(true)
          .Build();
  controller()->OnFillingTriggered(AccessoryTabType::CREDIT_CARDS,
                                   selected_field);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      true, 1);

  // User selects non credential field that has autofill suggestions.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);

  controller()->OnFillingTriggered(AccessoryTabType::CREDIT_CARDS,
                                   selected_field);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      false, 1);

  // User selects a credential field.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);

  controller()->OnFillingTriggered(AccessoryTabType::CREDIT_CARDS,
                                   selected_field);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      false, 2);
}

TEST_F(ManualFillingControllerTest, ForwardsOptionSelectedToController) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(mock_payment_method_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS));

  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);
}

TEST_F(ManualFillingControllerTest, LogsHistogramOnOptionSelected) {
  base::HistogramTester histogram_tester;
  // User selects non credential field that does not have autofill suggestions.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      true, 1);
  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelected2",
      AccessoryAction::MANAGE_CREDIT_CARDS, 1);

  // User selects non credential field that has autofill suggestions.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);

  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      false, 1);
  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelected2",
      AccessoryAction::MANAGE_CREDIT_CARDS, 2);

  // User selects a credential field.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);

  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);

  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelectedForNonCredentialFieldWithoutSuggestions",
      false, 2);
  histogram_tester.ExpectBucketCount(
      "KeyboardAccessory."
      "AccessoryActionSelected2",
      AccessoryAction::MANAGE_CREDIT_CARDS, 3);
}

class ManualFillingControllerAtMemoryFieldTypeTest
    : public ManualFillingControllerTest,
      public WithParamInterface<FocusedFieldType> {
 public:
  FocusedFieldType focused_field_type() const { return GetParam(); }

 private:
  // Textarea support requires `kAutofillAtMemorySupportTextAreaOnAndroid`.
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAtMemorySupportTextAreaOnAndroid};
};

INSTANTIATE_TEST_SUITE_P(All,
                         ManualFillingControllerAtMemoryFieldTypeTest,
                         Values(FocusedFieldType::kContenteditableField,
                                FocusedFieldType::kFillableTextArea));

// Tests that without `kAutofillAtMemorySupportTextAreaOnAndroid`, focusing a
// textarea never shows the keyboard accessory, even if AtMemory is available.
TEST_F(ManualFillingControllerTest,
       DoesNotShowAccessoryOnTextAreaWithoutTextAreaSupport) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAtMemorySupportTextAreaOnAndroid);

  EXPECT_CALL(*view(), Hide());
  EXPECT_CALL(*view(), Show).Times(0);

  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          FocusedFieldType::kFillableTextArea);
}

// Tests that focusing a contenteditable or textarea element shows the keyboard
// accessory.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest, ShowsAccessory) {
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(true)));

  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          focused_field_type());
}

// Tests that when sheet data is available for AtMemory, it is forwarded to the
// view when the AtMemory filling source is updated.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest,
       ShowsAccessoryWithSheetData) {
  const AccessorySheetData kTestSheet =
      AccessorySheetData::Builder(AccessoryTabType::ALL, u"AtMemory").Build();

  FocusFieldAndClearExpectations(focused_field_type());

  EXPECT_CALL(mock_at_memory_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestSheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(true)));

  NotifyAtMemorySourceObserver(IsFillingSourceAvailable(true));
}

// Tests that the keyboard accessory is hidden when focus shifts away from a
// contenteditable or textarea element.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest,
       HidesAccessoryWhenFocusLeaves) {
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(true)));

  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          focused_field_type());
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->NotifyFocusedInputChanged(autofill::FieldGlobalId(),
                                          FocusedFieldType::kUnknown);
}

// Tests that on contenteditable and textarea fields, non-AtMemory sheets
// (passwords, addresses, credit cards) are not forwarded to the view, while
// AtMemory sheet data is still forwarded.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest, HidesOtherAccessoryTabs) {
  const AccessorySheetData kTestAddressSheet =
      populate_sheet(AccessoryTabType::ADDRESSES);
  const AccessorySheetData kTestCreditCardSheet =
      populate_sheet(AccessoryTabType::CREDIT_CARDS);
  const AccessorySheetData kTestAtMemorySheet =
      AccessorySheetData::Builder(AccessoryTabType::ALL, u"AtMemory").Build();

  EXPECT_CALL(mock_pwd_controller_, GetSheetData)
      .WillRepeatedly(Return(filled_passwords_sheet()));
  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .WillRepeatedly(Return(kTestAddressSheet));
  EXPECT_CALL(mock_payment_method_controller_, GetSheetData)
      .WillRepeatedly(Return(kTestCreditCardSheet));
  EXPECT_CALL(mock_at_memory_controller_, GetSheetData)
      .WillRepeatedly(Return(kTestAtMemorySheet));

  // None of the fallback sheets should be sent to the view on contenteditable
  // or textarea, but the AtMemory sheet should be forwarded.
  EXPECT_CALL(*view(), OnItemsAvailable(filled_passwords_sheet())).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestCreditCardSheet)).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAtMemorySheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(true)));

  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::CREDIT_CARD_FALLBACKS,
                                         /*has_suggestions=*/true);
  NotifyAtMemorySourceObserver(IsFillingSourceAvailable(true));

  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          focused_field_type());
}

// Tests that when focus leaves contenteditable/textarea and moves to a fillable
// field, non-AtMemory sheets are forwarded to the view again.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest,
       ShowsOtherAccessoryTabsWhenFocusLeaves) {
  const AccessorySheetData kTestAddressSheet =
      populate_sheet(AccessoryTabType::ADDRESSES);

  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .WillRepeatedly(Return(kTestAddressSheet));

  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/true);

  // Focus contenteditable or textarea: address sheet is suppressed.
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(0);
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsUnstructuredField(true)));
  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          focused_field_type());
  testing::Mock::VerifyAndClearExpectations(view());

  // Focus regular fillable field: address sheet is forwarded.
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsUnstructuredField(false)));
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kFillableNonSearchField);
}

// Tests that on contenteditable and textarea fields, if AtMemory is not
// available, the accessory is hidden.
TEST_P(ManualFillingControllerAtMemoryFieldTypeTest,
       HidesAccessoryWhenAtMemoryNotAvailable) {
  EXPECT_CALL(mock_at_memory_controller_, IsAtMemoryAvailable)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*view(), Hide());
  EXPECT_CALL(*view(), Show).Times(0);

  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          focused_field_type());
}

// Tests that setting the selected suggestion forwards the call to the view.
TEST_F(ManualFillingControllerTest, SetSelectedSuggestion) {
  EXPECT_CALL(*view(), SetSelectedSuggestion(testing::Optional(1)));
  controller()->SetSelectedSuggestion(1);

  EXPECT_CALL(*view(), SetSelectedSuggestion(testing::Eq(std::nullopt)));
  controller()->SetSelectedSuggestion(std::nullopt);
}

// Tests that navigation actions are forwarded to the view.
TEST_F(ManualFillingControllerTest, Navigate) {
  EXPECT_CALL(*view(), Navigate(NavigationAction::kForward))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(controller()->Navigate(NavigationAction::kForward));

  EXPECT_CALL(*view(), Navigate(NavigationAction::kBackward))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(controller()->Navigate(NavigationAction::kBackward));

  EXPECT_CALL(*view(), Navigate(NavigationAction::kToggle))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(controller()->Navigate(NavigationAction::kToggle));
}

TEST_F(ManualFillingControllerTest, AcceptFallbackForwardsToView) {
  EXPECT_CALL(*view(), AcceptFallback())
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(controller()->AcceptFallback());
  EXPECT_FALSE(controller()->AcceptFallback());
}

TEST_F(ManualFillingControllerTest,
       GetOrCreateWithoutPasswordManagerClientDoesNotCrash) {
  std::unique_ptr<content::WebContents> test_web_contents =
      CreateTestWebContents();
  base::WeakPtr<ManualFillingController> mf_controller =
      ManualFillingController::GetOrCreate(test_web_contents.get());
  ASSERT_TRUE(mf_controller);

  // Requesting the password sheet or triggering password filling when no
  // PasswordAccessoryController is present should safely no-op.
  base::MockCallback<base::OnceCallback<void(AccessorySheetData)>> callback;
  EXPECT_CALL(callback, Run).Times(0);
  mf_controller->RequestAccessorySheet(AccessoryTabType::PASSWORDS,
                                       callback.Get());
  mf_controller->OnFillingTriggered(
      AccessoryTabType::PASSWORDS,
      autofill::AccessorySheetField::Builder().Build());
}

TEST_F(ManualFillingControllerTest, DoesNotUpdateViewWhenWebContentsIsHidden) {
  web_contents()->WasHidden();

  EXPECT_CALL(mock_payment_method_controller_, GetSheetData)
      .WillRepeatedly(Return(populate_sheet(AccessoryTabType::CREDIT_CARDS)));
  EXPECT_CALL(*view(), OnItemsAvailable).Times(0);
  EXPECT_CALL(*view(), Show).Times(0);
  EXPECT_CALL(*view(), Hide).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard).Times(0);
  EXPECT_CALL(*view(), CloseAccessorySheet).Times(0);

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kFillableNonSearchField);
}

}  // namespace
