// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address_accessory_controller_impl.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {
using autofill::UserInfo;
using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using FillingSource = ManualFillingController::FillingSource;

base::string16 addresses_empty_str() {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SHEET_EMPTY_MESSAGE);
}

base::string16 manage_addresses_str() {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ADDRESS_SHEET_ALL_ADDRESSES_LINK);
}

// Creates a AccessorySheetData::Builder with a "Manage Addresses" footer.
AccessorySheetData::Builder AddressAccessorySheetDataBuilder(
    const base::string16& title) {
  return AccessorySheetData::Builder(AccessoryTabType::ADDRESSES, title)
      .AppendFooterCommand(manage_addresses_str(),
                           AccessoryAction::MANAGE_ADDRESSES);
}

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager = std::make_unique<TestPersonalDataManager>();
  personal_data_manager->SetAutofillProfileEnabled(true);
  return personal_data_manager;
}

}  // namespace

class AddressAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  AddressAccessoryControllerTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&BuildTestPersonalDataManager));

    AddressAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_manual_filling_controller_.AsWeakPtr());
  }

  void TearDown() override { personal_data_manager()->ClearProfiles(); }

  AddressAccessoryController* controller() {
    return AddressAccessoryControllerImpl::FromWebContents(web_contents());
  }

  TestPersonalDataManager* personal_data_manager() {
    return static_cast<TestPersonalDataManager*>(
        PersonalDataManagerFactory::GetForProfile(profile()));
  }

 protected:
  StrictMock<MockManualFillingController> mock_manual_filling_controller_;
};

TEST_F(AddressAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  AddressAccessoryControllerImpl* initial_controller =
      AddressAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  AddressAccessoryControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(AddressAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(AddressAccessoryControllerTest, RefreshSuggestionsCallsUI) {
  AutofillProfile canadian = test::GetFullValidProfileForCanada();
  personal_data_manager()->AddProfile(canadian);

  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestions();

  ASSERT_EQ(
      result,
      AddressAccessorySheetDataBuilder(base::string16())
          .AddUserInfo()
          .AppendSimpleField(canadian.GetRawInfo(ServerFieldType::NAME_FULL))
          .AppendSimpleField(canadian.GetRawInfo(ServerFieldType::COMPANY_NAME))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_LINE1))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_LINE2))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_ZIP))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_CITY))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_STATE))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::ADDRESS_HOME_COUNTRY))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::PHONE_HOME_WHOLE_NUMBER))
          .AppendSimpleField(
              canadian.GetRawInfo(ServerFieldType::EMAIL_ADDRESS))
          .Build());
}

TEST_F(AddressAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestions();

  ASSERT_EQ(result,
            AddressAccessorySheetDataBuilder(addresses_empty_str()).Build());
}

TEST_F(AddressAccessoryControllerTest, TriggersRefreshWhenDataChanges) {
  AccessorySheetData result(AccessoryTabType::PASSWORDS, base::string16());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions(_))
      .WillRepeatedly(SaveArg<0>(&result));

  // A refresh without data stores an empty sheet and registers an observer.
  controller()->RefreshSuggestions();
  ASSERT_EQ(result,
            AddressAccessorySheetDataBuilder(addresses_empty_str()).Build());

  // When new data is added, a refresh is automatically triggered.
  AutofillProfile email = test::GetIncompleteProfile2();
  personal_data_manager()->AddProfile(email);
  ASSERT_EQ(result, AddressAccessorySheetDataBuilder(base::string16())
                        .AddUserInfo()
                        /*name full:*/
                        .AppendSimpleField(base::string16())
                        /*company name:*/
                        .AppendSimpleField(base::string16())
                        /*address line1:*/
                        .AppendSimpleField(base::string16())
                        /*address line2:*/
                        .AppendSimpleField(base::string16())
                        /*address zip:*/
                        .AppendSimpleField(base::string16())
                        /*address city:*/
                        .AppendSimpleField(base::string16())
                        /*address state:*/
                        .AppendSimpleField(base::string16())
                        /*address country:*/
                        .AppendSimpleField(base::string16())
                        /*phone number:*/.AppendSimpleField(base::string16())
                        .AppendSimpleField(
                            email.GetRawInfo(ServerFieldType::EMAIL_ADDRESS))
                        .Build());
}

}  // namespace autofill
