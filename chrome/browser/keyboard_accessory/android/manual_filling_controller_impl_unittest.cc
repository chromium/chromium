// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessorySuggestionType;
using autofill::AccessoryTabType;
using autofill::TestAutofillClientInjector;
using autofill::TestContentAutofillClient;
using autofill::mojom::FocusedFieldType;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using WaitForKeyboard = ManualFillingViewInterface::WaitForKeyboard;
using ShouldShowOnLargeFormFactor =
    ManualFillingViewInterface::ShouldShowOnLargeFormFactor;
using IsContentEditable = ManualFillingViewInterface::IsContentEditable;

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

constexpr autofill::FieldRendererId kFocusedFieldId(123);

// Fixture that tests the manual filling experience with the most recent version
// of the keyboard accessory and all its fallback sheets.
class ManualFillingControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

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

  TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
};

TEST_F(ManualFillingControllerTest, ShowsAccessoryForAutofillOnSearchField) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillEnableKeyboardAccessoryOnSearchFields);

  FocusFieldAndClearExpectations(FocusedFieldType::kFillableSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(false)));
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsContentEditable(false)));
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
                   IsContentEditable(false)));

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
                   IsContentEditable(false)));

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
                   IsContentEditable(false)));

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest, HidesAccessoryWithoutAvailableSources) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(false)));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsContentEditable(false)));
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
                   IsContentEditable(false)))
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
                   IsContentEditable(false)));
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
                   IsContentEditable(false)));

  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsContentEditable(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnNonCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(true),
                   IsContentEditable(false)));
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceNotAvailableNonCredentialFields) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(false)));
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
              OnFillingTriggered(controller()->GetLastFocusedFieldId(),
                                 selected_field));

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

// Tests that focusing a contenteditable element shows the keyboard accessory.
TEST_F(ManualFillingControllerTest, ShowsAccessoryForContentEditableField) {
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(true)));

  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kContenteditableField);
}

// Tests that when sheet data is available for AtMemory, it is forwarded to the
// view when the AtMemory filling source is updated.
TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForContentEditableWithSheetData) {
  const AccessorySheetData kTestSheet =
      AccessorySheetData::Builder(AccessoryTabType::ALL, u"AtMemory").Build();

  FocusFieldAndClearExpectations(FocusedFieldType::kContenteditableField);

  EXPECT_CALL(mock_at_memory_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestSheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(true)));

  NotifyAtMemorySourceObserver(IsFillingSourceAvailable(true));
}

// Tests that the keyboard accessory is hidden when focus shifts away from a
// contenteditable element.
TEST_F(ManualFillingControllerTest,
       HidesAccessoryWhenFocusLeavesContentEditable) {
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(true)));

  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kContenteditableField);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->NotifyFocusedInputChanged(autofill::FieldRendererId(),
                                          FocusedFieldType::kUnknown);
}

// Tests that on contenteditable fields, non-AtMemory sheets (passwords,
// addresses, credit cards) are not forwarded to the view, while AtMemory
// sheet data is still forwarded.
TEST_F(ManualFillingControllerTest,
       HidesOtherAccessoryTabsOnContentEditableField) {
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

  // None of the fallback sheets should be sent to the view on contenteditable,
  // but the AtMemory sheet should be forwarded.
  EXPECT_CALL(*view(), OnItemsAvailable(filled_passwords_sheet())).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestCreditCardSheet)).Times(0);
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAtMemorySheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(true)));

  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::CREDIT_CARD_FALLBACKS,
                                         /*has_suggestions=*/true);
  NotifyAtMemorySourceObserver(IsFillingSourceAvailable(true));

  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kContenteditableField);
}

// Tests that when focus leaves contenteditable and moves to a fillable field,
// non-AtMemory sheets are forwarded to the view again.
TEST_F(ManualFillingControllerTest,
       ShowsOtherAccessoryTabsWhenFocusLeavesContentEditable) {
  const AccessorySheetData kTestAddressSheet =
      populate_sheet(AccessoryTabType::ADDRESSES);

  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .WillRepeatedly(Return(kTestAddressSheet));

  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/true);

  // Focus contenteditable: address sheet is suppressed.
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(0);
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(true)));
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kContenteditableField);
  testing::Mock::VerifyAndClearExpectations(view());

  // Focus regular fillable field: address sheet is forwarded.
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(AtLeast(1));
  EXPECT_CALL(*view(),
              Show(WaitForKeyboard(true), ShouldShowOnLargeFormFactor(false),
                   IsContentEditable(false)));
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kFillableNonSearchField);
}

// Tests that on contenteditable fields, if AtMemory is not available, the
// accessory is hidden.
TEST_F(ManualFillingControllerTest,
       HidesAccessoryOnContentEditableWhenAtMemoryNotAvailable) {
  EXPECT_CALL(mock_at_memory_controller_, IsAtMemoryAvailable)
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*view(), Hide());
  EXPECT_CALL(*view(), Show).Times(0);

  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kContenteditableField);
}

}  // namespace
