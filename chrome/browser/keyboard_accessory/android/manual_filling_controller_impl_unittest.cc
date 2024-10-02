// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_cache.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessoryTabType;
using autofill::TestAutofillClientInjector;
using autofill::TestContentAutofillClient;
using autofill::mojom::FocusedFieldType;
using plus_addresses::FakePlusAddressService;
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

AccessorySheetData filled_passwords_sheet() {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS, u"Pwds",
                                     /*plus_address_title=*/std::u16string())
      .AddUserInfo("example.com", autofill::UserInfo::IsExactMatch(true))
      .AppendField(u"Ben", u"Ben", false, true)
      .AppendField(u"S3cur3", u"Ben's PW", true, false)
      .Build();
}

AccessorySheetData populate_sheet(AccessoryTabType type) {
  constexpr char16_t kTitle[] = u"Suggestions available!";
  return AccessorySheetData::Builder(type, kTitle,
                                     /*plus_address_title=*/std::u16string())
      .AddUserInfo()
      .Build();
}

std::vector<uint8_t> test_passkey_id() {
  return {23, 24, 25, 26, 27};
}

std::unique_ptr<KeyedService> BuildFakePlusAddressService(
    content::BrowserContext* context) {
  return std::make_unique<FakePlusAddressService>();
}

constexpr autofill::FieldRendererId kFocusedFieldId(123);

}  // namespace

// Fixture that tests the manual filling experience with the most recent version
// of the keyboard accessory and all its fallback sheets.
class ManualFillingControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManualFillingControllerTest() {
    features_.InitWithFeatures(
        {plus_addresses::features::kPlusAddressesEnabled,
         plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(), base::BindRepeating(&BuildFakePlusAddressService));

    EXPECT_CALL(mock_pwd_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&pwd_source_observer_));
    EXPECT_CALL(mock_pwd_controller_, RegisterPlusProfilesProvider);
    EXPECT_CALL(mock_payment_method_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&cc_source_observer_));
    EXPECT_CALL(mock_address_controller_, RegisterFillingSourceObserver)
        .WillOnce(SaveArg<0>(&address_source_observer_));
    EXPECT_CALL(mock_address_controller_, RegisterPlusProfilesProvider);
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(),
        mock_payment_method_controller_.AsWeakPtr(),
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

 protected:
  base::test::ScopedFeatureList features_;
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockPaymentMethodAccessoryController>
      mock_payment_method_controller_;

  AccessoryController::FillingSourceObserver pwd_source_observer_;
  AccessoryController::FillingSourceObserver cc_source_observer_;
  AccessoryController::FillingSourceObserver address_source_observer_;

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

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
  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                         /*has_suggestions=*/false);
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

  // TODO(crbug.com/40165275): Because the data isn't cached, test that only one
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

  // TODO(crbug.com/40165275): Because the data isn't cached, test that only one
  // call to `GetSheetData()` happens.
  EXPECT_CALL(mock_payment_method_controller_, GetSheetData)
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

TEST_F(ManualFillingControllerTest, FetchesAffiliatedPlusProfilesWhenShown) {
  FocusFieldAndClearExpectations(FocusedFieldType::kFillableNonSearchField);

  // Not plus profiles should be fetched before the first call to `Show()`.
  EXPECT_TRUE(controller()->plus_profiles_cache());
  EXPECT_EQ(
      controller()->plus_profiles_cache()->GetAffiliatedPlusProfiles().size(),
      0u);

  EXPECT_CALL(*view(), Show(WaitForKeyboard(true)));
  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/true);
  EXPECT_EQ(
      controller()->plus_profiles_cache()->GetAffiliatedPlusProfiles().size(),
      1u);

  EXPECT_CALL(*view(), Hide());
  controller()->UpdateSourceAvailability(FillingSource::ADDRESS_FALLBACKS,
                                         /*has_suggestions=*/false);
  EXPECT_EQ(
      controller()->plus_profiles_cache()->GetAffiliatedPlusProfiles().size(),
      0u);
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
