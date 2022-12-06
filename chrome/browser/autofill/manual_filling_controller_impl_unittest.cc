// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::mojom::FocusedFieldType;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArgs;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using WaitForKeyboard = ManualFillingViewInterface::WaitForKeyboard;

AccessorySheetData empty_passwords_sheet() {
  constexpr char16_t kTitle[] = u"Example title";
  return AccessorySheetData(AccessoryTabType::PASSWORDS, kTitle);
}

AccessorySheetData filled_passwords_sheet() {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS, u"Pwds")
      .AddUserInfo("example.com", autofill::UserInfo::IsExactMatch(true))
      .AppendField(u"Ben", u"Ben", false, true)
      .AppendField(u"S3cur3", u"Ben's PW", true, false)
      .Build();
}

AccessorySheetData populate_sheet(AccessoryTabType type) {
  constexpr char16_t kTitle[] = u"Suggestions available!";
  return AccessorySheetData::Builder(type, kTitle).AddUserInfo().Build();
}

constexpr autofill::FieldRendererId kFocusedFieldId(123);

}  // namespace

// Fixture that tests the manual filling experience with the most recent version
// of the keyboard accessory and all its fallback sheets.
class ManualFillingControllerTest : public testing::Test {
 public:
  ManualFillingControllerTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {autofill::features::kAutofillKeyboardAccessory,
         autofill::features::kAutofillManualFallbackAndroid},
        /*disabled_features=*/{});

    ON_CALL(mock_pwd_controller_, RegisterFillingSourceObserver(_))
        .WillByDefault(SaveArg<0>(&pwd_source_observer_));
    ON_CALL(mock_cc_controller_, RegisterFillingSourceObserver(_))
        .WillByDefault(SaveArg<0>(&cc_source_observer_));
    ON_CALL(mock_address_controller_, RegisterFillingSourceObserver(_))
        .WillByDefault(SaveArg<0>(&address_source_observer_));
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
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

  content::WebContents* web_contents() { return web_contents_; }

  MockManualFillingView* view() {
    return static_cast<MockManualFillingView*>(controller()->view());
  }

  void NotifyPasswordSourceObserver(IsFillingSourceAvailable source_available) {
    pwd_source_observer_.Run(&mock_pwd_controller_, source_available);
  }

  void NotifyCreditCardSourceObserver(
      IsFillingSourceAvailable source_available) {
    cc_source_observer_.Run(&mock_cc_controller_, source_available);
  }

  void NotifyAddressSourceObserver(IsFillingSourceAvailable source_available) {
    address_source_observer_.Run(&mock_address_controller_, source_available);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ =
      web_contents_factory_.CreateWebContents(&profile_);

  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;

  AccessoryController::FillingSourceObserver pwd_source_observer_;
  AccessoryController::FillingSourceObserver cc_source_observer_;
  AccessoryController::FillingSourceObserver address_source_observer_;
};

// Fixture for tests that the old but widely used legacy behavior of the manual
// filling experience. It doesn't expect autofill suggestions and uses only the
// fallback sheets for passwords.
class ManualFillingControllerLegacyTest : public ManualFillingControllerTest {
 public:
  ManualFillingControllerLegacyTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            autofill::features::kAutofillEnableManualFallbackForVirtualCards,
            autofill::features::kAutofillKeyboardAccessory,
            autofill::features::kAutofillManualFallbackAndroid});
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
  }
};

TEST_F(ManualFillingControllerLegacyTest, IsNotRecreatedForSameWebContents) {
  ManualFillingController* initial_controller =
      ManualFillingControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  ManualFillingControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(ManualFillingControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(ManualFillingControllerLegacyTest,
       ClosesSheetWhenFocusingUnfillableField) {
  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          FocusedFieldType::kUnfillableElement);
}

TEST_F(ManualFillingControllerLegacyTest,
       SwapsWithKeyboardWhenFocusingFillableFields) {
  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kFillablePasswordField);
}

TEST_F(ManualFillingControllerLegacyTest, ClosesSheetWhenFocusingSearchField) {
  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, FocusedFieldType::kFillableSearchField);
}

TEST_F(ManualFillingControllerLegacyTest, ClosesSheetWhenFocusingTextArea) {
  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->NotifyFocusedInputChanged(kFocusedFieldId,
                                          FocusedFieldType::kFillableTextArea);
}

TEST_F(ManualFillingControllerLegacyTest,
       AlwaysShowsAccessoryForPasswordFields) {
  controller()->RefreshSuggestions(empty_passwords_sheet());

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);
}

TEST_F(ManualFillingControllerTest, ShowsAccessoryForAutofillOnSearchField) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableSearchField);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/true);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding autofill hides the accessory because fallbacks alone don't provide
  // sufficient value and might be confusing.
  EXPECT_CALL(*view(), Hide()).Times(1);
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerLegacyTest, ShowsAccessoryWithSuggestions) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerLegacyTest,
       ShowsAccessoryForUsernameFieldsEvenWithoutPasswordSuggestions) {
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Hide()).Times(0);
  controller()->RefreshSuggestions(empty_passwords_sheet());
}

TEST_F(ManualFillingControllerLegacyTest,
       DoesntShowFallbacksOutsideUsernameInV1) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerTest, ShowsFallbacksOutsideUsernameInV2) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerLegacyTest,
       ShowsAccessoryWhenRefreshingSuggestions) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));
}

TEST_F(ManualFillingControllerLegacyTest, ShowsAndHidesAccessoryForPasswords) {
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/true);

  EXPECT_CALL(*view(), Hide()).Times(0);
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForPasswordsTriggeredByObserver) {
  // TODO(crbug.com/1169167): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_pwd_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(filled_passwords_sheet()));
  EXPECT_CALL(*view(), OnItemsAvailable(filled_passwords_sheet()))
      .Times(AnyNumber());
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));

  NotifyPasswordSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(*view(), Hide());
  NotifyPasswordSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForAddressesTriggeredByObserver) {
  const AccessorySheetData kTestAddressSheet =
      populate_sheet(AccessoryTabType::ADDRESSES);

  // TODO(crbug.com/1169167): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_address_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestAddressSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestAddressSheet)).Times(AnyNumber());
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));

  NotifyAddressSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyAddressSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForCreditCardsTriggeredByObserver) {
  const AccessorySheetData kTestCreditCardSheet =
      populate_sheet(AccessoryTabType::CREDIT_CARDS);

  // TODO(crbug.com/1169167): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_cc_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestCreditCardSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestCreditCardSheet))
      .Times(AnyNumber());
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryForCreditCardsWhenManualFallbackEnabledForVirtualCards) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(/*enabled_features=*/
                            {autofill::features::
                                 kAutofillEnableManualFallbackForVirtualCards},
                            /*disabled_features=*/{
                                autofill::features::kAutofillKeyboardAccessory,
                                autofill::features::
                                    kAutofillManualFallbackAndroid});
  const AccessorySheetData kTestCreditCardSheet =
      populate_sheet(AccessoryTabType::CREDIT_CARDS);

  // TODO(crbug.com/1169167): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_cc_controller_, GetSheetData)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(kTestCreditCardSheet));
  EXPECT_CALL(*view(), OnItemsAvailable(kTestCreditCardSheet))
      .Times(AnyNumber());
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));

  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(true));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Hide());
  NotifyCreditCardSourceObserver(IsFillingSourceAvailable(false));
}

TEST_F(ManualFillingControllerLegacyTest,
       UpdatesCreditCardControllerOnFocusChange) {
  EXPECT_CALL(mock_cc_controller_, RefreshSuggestions);
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);
}

TEST_F(ManualFillingControllerTest, HidesAccessoryWithoutAvailableSources) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true))).Times(2);
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
  EXPECT_CALL(*view(), Show(WaitForKeyboard(true))).Times(0);
  controller()->UpdateSourceAvailability(FillingSource::PASSWORD_FALLBACKS,
                                         /*has_suggestions=*/false);
  testing::Mock::VerifyAndClearExpectations(view());

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
}

TEST_F(ManualFillingControllerLegacyTest, OnAutomaticGenerationStatusChanged) {
  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationStatusChanged(true);

  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(false));
  controller()->OnAutomaticGenerationStatusChanged(false);
}

TEST_F(ManualFillingControllerLegacyTest,
       OnFillingTriggeredFillsAndClosesSheet) {
  const char16_t kTextToFill[] = u"TextToFill";
  const std::u16string text_to_fill(kTextToFill);
  const autofill::AccessorySheetField field(
      /*display_text=*/text_to_fill, /*text_to_fill=*/text_to_fill,
      /*a11y_description=*/text_to_fill, /*id=*/"", /*is_obfuscated=*/false,
      /*selectable=*/true);

  EXPECT_CALL(mock_pwd_controller_,
              OnFillingTriggered(autofill::FieldGlobalId(), field));
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->OnFillingTriggered(AccessoryTabType::PASSWORDS, field);

  autofill::FieldGlobalId field_id{autofill::LocalFrameToken(),
                                   kFocusedFieldId};
  EXPECT_CALL(mock_pwd_controller_, OnFillingTriggered(field_id, field));
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->NotifyFocusedInputChanged(
      kFocusedFieldId, autofill::mojom::FocusedFieldType::kUnknown);
  controller()->OnFillingTriggered(AccessoryTabType::PASSWORDS, field);
}

TEST_F(ManualFillingControllerLegacyTest, RefreshingUpdatesCache) {
  // First, focus a field for which suggestions exist.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableUsernameField);
  controller()->RefreshSuggestions(populate_sheet(AccessoryTabType::PASSWORDS));

  // Now, focus a field (e.g. in an iframe) with different suggestions.
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);
  controller()->RefreshSuggestions(filled_passwords_sheet());

  // Triggering a subsequent (independent) update must reuse the latest sheet.
  AccessorySheetData cached(AccessoryTabType::PASSWORDS, std::u16string());
  EXPECT_CALL(*view(), OnItemsAvailable).WillOnce(SaveArg<0>(&cached));
  FocusFieldAndClearExpectations(FocusedFieldType::kFillablePasswordField);
  EXPECT_EQ(cached, filled_passwords_sheet());
}

TEST_F(ManualFillingControllerLegacyTest,
       ForwardsPasswordManagingToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS);
}

TEST_F(ManualFillingControllerLegacyTest,
       ForwardsPasswordGenerationToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerLegacyTest, ForwardsAddressManagingToController) {
  EXPECT_CALL(mock_address_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES);
}

TEST_F(ManualFillingControllerLegacyTest,
       ForwardsCreditCardManagingToController) {
  EXPECT_CALL(mock_cc_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_CREDIT_CARDS);
}

TEST_F(ManualFillingControllerLegacyTest, OnAutomaticGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
}

TEST_F(ManualFillingControllerLegacyTest, OnManualGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerLegacyTest, OnSavePasswordsToggledTrue) {
  EXPECT_CALL(mock_pwd_controller_,
              OnToggleChanged(AccessoryAction::TOGGLE_SAVE_PASSWORDS, true));
  controller()->OnToggleChanged(AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(ManualFillingControllerLegacyTest, OnSavePasswordsToggledFalse) {
  EXPECT_CALL(mock_pwd_controller_,
              OnToggleChanged(AccessoryAction::TOGGLE_SAVE_PASSWORDS, false));
  controller()->OnToggleChanged(AccessoryAction::TOGGLE_SAVE_PASSWORDS, false);
}

TEST_F(ManualFillingControllerTest,
       ShowsAccessoryWhenAutofillSourceAvailableOnUnknownField) {
  FocusFieldAndClearExpectations(FocusedFieldType::kUnknown);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(false)));
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
