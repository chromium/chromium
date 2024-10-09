// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/address_accessory_controller_impl.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_provider.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_affiliated_plus_profiles_provider.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace autofill {
namespace {
using autofill::UserInfo;
using base::ASCIIToUTF16;
using plus_addresses::FakePlusAddressService;
using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using PlusProfile = plus_addresses::PlusProfile;

constexpr char kExampleSite[] = "https://example.com";

std::u16string addresses_empty_str() {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
}

std::u16string plus_addresses_title() {
  return l10n_util::GetStringFUTF16(
      IDS_PLUS_ADDRESS_FALLBACK_MANUAL_FILLING_SHEET_TITLE, u"example.com");
}

std::u16string manage_addresses_str() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK);
}

// Creates a AccessorySheetData::Builder with a "Manage Addresses" footer.
AccessorySheetData::Builder AddressAccessorySheetDataBuilder(
    const std::u16string& userInfoTitle,
    const std::u16string& plusAddressTitle) {
  return AccessorySheetData::Builder(AccessoryTabType::ADDRESSES, userInfoTitle,
                                     plusAddressTitle)
      .AppendFooterCommand(manage_addresses_str(),
                           AccessoryAction::MANAGE_ADDRESSES);
}

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager = std::make_unique<TestPersonalDataManager>();
  personal_data_manager->test_address_data_manager().SetAutofillProfileEnabled(
      true);
  return personal_data_manager;
}

std::unique_ptr<KeyedService> BuildFakePlusAddressService(
    content::BrowserContext* context) {
  return std::make_unique<FakePlusAddressService>();
}

class MockAutofillClient : public TestContentAutofillClient {
 public:
  using autofill::TestContentAutofillClient::TestContentAutofillClient;
  MOCK_METHOD(void,
              OfferPlusAddressCreation,
              (const url::Origin&, PlusAddressCallback),
              (override));
  MOCK_METHOD(url::Origin,
              GetLastCommittedPrimaryMainFrameOrigin,
              (),
              (const, override));
};

class MockAutofillDriver : public TestContentAutofillDriver {
 public:
  using TestContentAutofillDriver::TestContentAutofillDriver;
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType,
               mojom::ActionPersistence,
               const FieldGlobalId&,
               const std::u16string&),
              (override));
};

}  // namespace

class AddressAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  AddressAccessoryControllerTest() {
    features_.InitWithFeatures(
        {plus_addresses::features::kPlusAddressesEnabled,
         plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(), base::BindRepeating(&BuildFakePlusAddressService));

    NavigateAndCommit(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    AddressAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_manual_filling_controller_.AsWeakPtr());
    controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());

    ON_CALL(autofill_client(), GetLastCommittedPrimaryMainFrameOrigin)
        .WillByDefault(Return(url::Origin::Create(GURL(kExampleSite))));
  }

  void TearDown() override {
    personal_data_manager()->test_address_data_manager().ClearProfiles();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  AddressAccessoryController* controller() {
    return AddressAccessoryControllerImpl::FromWebContents(web_contents());
  }

  TestPersonalDataManager* personal_data_manager() {
    return static_cast<TestPersonalDataManager*>(
        PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

  MockAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  MockAutofillDriver& main_frame_autofill_driver() {
    return *autofill_driver_injector_[web_contents()];
  }

  FakePlusAddressService& plus_address_service() {
    return *static_cast<FakePlusAddressService*>(
        PlusAddressServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext()));
  }

  base::test::ScopedFeatureList features_;
  test::AutofillUnitTestEnvironment test_environment_;
  StrictMock<MockManualFillingController> mock_manual_filling_controller_;
  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;
  TestAutofillClientInjector<NiceMock<MockAutofillClient>>
      autofill_client_injector_;
  TestAutofillDriverInjector<NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
};

TEST_F(AddressAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());
}

TEST_F(AddressAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  AddressAccessoryControllerImpl* initial_controller =
      AddressAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  AddressAccessoryControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(AddressAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(AddressAccessoryControllerTest, ProvidesEmptySheetBeforeInitialRefresh) {
  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());
}

TEST_F(AddressAccessoryControllerTest, RefreshSuggestionsCallsUI) {
  AutofillProfile canadian = test::GetFullValidProfileForCanada();
  personal_data_manager()->address_data_manager().AddProfile(canadian);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(
      controller()->GetSheetData(),
      AddressAccessorySheetDataBuilder(/*userInfoTitle=*/std::u16string(),
                                       /*plusAddressTitle=*/std::u16string())
          .AddUserInfo()
          .AppendSimpleField(canadian.GetRawInfo(FieldType::NAME_FULL))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::COMPANY_NAME))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_LINE1))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_LINE2))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_ZIP))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_CITY))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_STATE))
          .AppendSimpleField(
              canadian.GetRawInfo(FieldType::ADDRESS_HOME_COUNTRY))
          .AppendSimpleField(
              canadian.GetRawInfo(FieldType::PHONE_HOME_WHOLE_NUMBER))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::EMAIL_ADDRESS))
          .Build());
}

TEST_F(AddressAccessoryControllerTest, TriggersRefreshWhenDataChanges) {
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  // A refresh without data stores an empty sheet and registers an observer.
  controller()->RefreshSuggestions();

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());

  // When new data is added, a refresh is automatically triggered.
  AutofillProfile email = test::GetIncompleteProfile2();
  personal_data_manager()->address_data_manager().AddProfile(email);
  EXPECT_EQ(
      controller()->GetSheetData(),
      AddressAccessorySheetDataBuilder(/*userInfoTitle=*/std::u16string(),
                                       /*plusAddressTitle=*/std::u16string())
          .AddUserInfo()
          /*name full:*/
          .AppendSimpleField(std::u16string())
          /*company name:*/
          .AppendSimpleField(std::u16string())
          /*address line1:*/
          .AppendSimpleField(std::u16string())
          /*address line2:*/
          .AppendSimpleField(std::u16string())
          /*address zip:*/
          .AppendSimpleField(std::u16string())
          /*address city:*/
          .AppendSimpleField(std::u16string())
          /*address state:*/
          .AppendSimpleField(std::u16string())
          /*address country:*/
          .AppendSimpleField(std::u16string())
          /*phone number:*/.AppendSimpleField(std::u16string())
          .AppendSimpleField(email.GetRawInfo(FieldType::EMAIL_ADDRESS))
          .Build());
}

TEST_F(AddressAccessoryControllerTest,
       PlusAddressFillingDisabled_NoPlusAddressesActions) {
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());
}

TEST_F(AddressAccessoryControllerTest,
       NoPlusAddressesSaved_NoSelectPlusAddressAction) {
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  // Although the plus address filling is enabled, the user doesn't have any
  // saved plus addresses. The "Select plus address" should not be displayed.
  plus_address_service().set_is_plus_address_filling_enabled(true);

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .Build());
}

TEST_F(AddressAccessoryControllerTest,
       PlusAddressFillingEnabled_AppendsSelectPlusAddressAction) {
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  plus_address_service().add_plus_profile(
      plus_addresses::test::CreatePlusProfile());
  plus_address_service().set_is_plus_address_filling_enabled(true);

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(
                addresses_empty_str(), /*plusAddressTitle=*/std::u16string())
                .AppendFooterCommand(
                    l10n_util::GetStringUTF16(
                        IDS_PLUS_ADDRESS_SELECT_PLUS_ADDRESS_LINK_ANDROID),
                    AccessoryAction::SELECT_PLUS_ADDRESS_FROM_ADDRESS_SHEET)
                .Build());
}

TEST_F(AddressAccessoryControllerTest,
       NoAffiliatedPlusAddresses_AppendsCreatePlusAddressAction) {
  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver);
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::span<const PlusProfile, 0>()));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestions();

  // Plus address creation can't be supported while plus address filling is
  // disabled.
  plus_address_service().set_is_plus_address_filling_enabled(true);
  plus_address_service().set_should_offer_plus_address_creation(true);

  EXPECT_EQ(
      controller()->GetSheetData(),
      AddressAccessorySheetDataBuilder(addresses_empty_str(),
                                       /*plusAddressTitle=*/std::u16string())
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_CREATE_NEW_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET)
          .Build());
}

TEST_F(AddressAccessoryControllerTest,
       HasAffiliatedPlusAddresses_NoCreatePlusAddressAction) {
  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver);
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile()};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::make_span(profiles)));
  controller()->RefreshSuggestions();

  // Plus address creation can't be supported while plus address filling is
  // disabled.
  plus_address_service().set_is_plus_address_filling_enabled(true);
  plus_address_service().set_should_offer_plus_address_creation(true);

  // Although the plus address creation is supported, the user has an affiliated
  // plus address for the current domain. The "Create plus address" action
  // should not be displayed.
  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(addresses_empty_str(),
                                             plus_addresses_title())
                .AddPlusAddressInfo("https://foo.com", u"plus+foo@plus.plus")
                .AppendFooterCommand(
                    l10n_util::GetStringUTF16(
                        IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
                    AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET)
                .Build());
}

TEST_F(AddressAccessoryControllerTest, AppendsPlusAddressesSection) {
  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver);
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile()};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::make_span(profiles)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(controller()->GetSheetData(),
            AddressAccessorySheetDataBuilder(addresses_empty_str(),
                                             plus_addresses_title())
                .AddPlusAddressInfo("https://foo.com", u"plus+foo@plus.plus")
                .AppendFooterCommand(
                    l10n_util::GetStringUTF16(
                        IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
                    AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET)
                .Build());
}

TEST_F(AddressAccessoryControllerTest,
       AppendsAddressProfileAndPlusAddressesSections) {
  AutofillProfile canadian = test::GetFullValidProfileForCanada();
  personal_data_manager()->address_data_manager().AddProfile(canadian);

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver);
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile()};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::make_span(profiles)));
  controller()->RefreshSuggestions();

  EXPECT_EQ(
      controller()->GetSheetData(),
      AddressAccessorySheetDataBuilder(/*userInfoTitle=*/std::u16string(),
                                       /*plusAddressTitle=*/std::u16string())
          .AddPlusAddressInfo("https://foo.com", u"plus+foo@plus.plus")
          .AddUserInfo()
          .AppendSimpleField(canadian.GetRawInfo(FieldType::NAME_FULL))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::COMPANY_NAME))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_LINE1))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_LINE2))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_ZIP))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_CITY))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::ADDRESS_HOME_STATE))
          .AppendSimpleField(
              canadian.GetRawInfo(FieldType::ADDRESS_HOME_COUNTRY))
          .AppendSimpleField(
              canadian.GetRawInfo(FieldType::PHONE_HOME_WHOLE_NUMBER))
          .AppendSimpleField(canadian.GetRawInfo(FieldType::EMAIL_ADDRESS))
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_ADDRESS_SHEET)
          .Build());
}

TEST_F(AddressAccessoryControllerTest, TriggersPlusAddressCreationBottomSheet) {
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  EXPECT_CALL(mock_manual_filling_controller_, GetLastFocusedFieldId)
      .WillOnce(Return(field_id));
  EXPECT_CALL(mock_manual_filling_controller_, Hide);
  const std::string plus_address = "example@gmail.com";
  EXPECT_CALL(autofill_client(), OfferPlusAddressCreation)
      .WillOnce(
          [&plus_address](const url::Origin&, PlusAddressCallback callback) {
            std::move(callback).Run(plus_address);
          });
  EXPECT_CALL(main_frame_autofill_driver(),
              ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                               mojom::ActionPersistence::kFill, field_id,
                               base::UTF8ToUTF16(plus_address)));
  controller()->OnOptionSelected(
      AccessoryAction::CREATE_PLUS_ADDRESS_FROM_ADDRESS_SHEET);
}

}  // namespace autofill
