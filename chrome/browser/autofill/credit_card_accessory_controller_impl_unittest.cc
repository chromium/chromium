// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::SaveArg;
using testing::SaveArgPointee;

constexpr char kExampleSite[] = "https://example.com";
const base::string16 kFirstTwelveDigits = base::ASCIIToUTF16("411111111111");

namespace autofill {
namespace {

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(
      AutofillDriver* driver,
      AutofillClient* client,
      PersonalDataManager* personal_data,
      AutocompleteHistoryManager* autocomplete_history_manager,
      std::unique_ptr<CreditCardAccessManager> cc_access_manager = nullptr)
      // Force to use the constructor designated for unit test.
      : AutofillManager(driver,
                        client,
                        personal_data,
                        autocomplete_history_manager,
                        "en-US",
                        AutofillHandler::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
                        std::move(cc_access_manager)) {}

  ~TestAutofillManager() override = default;

  const FormData& last_query_form() const override { return last_form_; }

  void SetLastForm(FormData form) { last_form_ = std::move(form); }

 private:
  FormData last_form_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

AccessorySheetData::Builder CreditCardAccessorySheetDataBuilder() {
  return AccessorySheetData::Builder(
             AccessoryTabType::CREDIT_CARDS,
             l10n_util::GetStringUTF16(
                 IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE))
      .AppendFooterCommand(
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
          AccessoryAction::MANAGE_CREDIT_CARDS);
}

}  // namespace

class TestAccessManager : public CreditCardAccessManager {
 public:
  TestAccessManager(AutofillDriver* driver,
                    AutofillClient* client,
                    PersonalDataManager* personal_data)
      : CreditCardAccessManager(driver, client, personal_data) {
    card_ = test::GetMaskedServerCard();
    card_.set_record_type(CreditCard::FULL_SERVER_CARD);
    card_.SetNumber(kFirstTwelveDigits + card_.number());
  }

  void FetchCreditCard(
      const CreditCard* card,
      base::WeakPtr<Accessor> accessor,
      const base::TimeTicks& timestamp = base::TimeTicks()) override {
    accessor->OnCreditCardFetched(/*did_succeed=*/true, &card_);
  }

  CreditCard card_;
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MOCK_METHOD1(RendererShouldFillFieldWithValue, void(const base::string16&));
};

class CreditCardAccessoryControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CreditCardAccessoryControllerTest()
      : af_manager_(&mock_af_driver_,
                    &client_,
                    &data_manager_,
                    &history_,
                    std::make_unique<TestAccessManager>(&mock_af_driver_,
                                                        &client_,
                                                        &data_manager_)) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    SetFormOrigin(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &data_manager_,
        &af_manager_, &mock_af_driver_);
    data_manager_.SetPrefService(profile_.GetPrefs());
  }

  void TearDown() override {
    data_manager_.SetPrefService(nullptr);
    data_manager_.ClearCreditCards();
  }

  CreditCardAccessoryController* controller() {
    return CreditCardAccessoryControllerImpl::FromWebContents(web_contents());
  }

  void SetFormOrigin(GURL origin) {
    FormData form;
    form.unique_renderer_id = 1;
    form.action = origin;
    form.main_frame_origin = url::Origin::Create(origin);
    client_.set_form_origin(origin);
    af_manager_.SetLastForm(std::move(form));
  }

 protected:
  TestAutofillClient client_;
  testing::NiceMock<MockAutofillDriver> mock_af_driver_;
  autofill::TestPersonalDataManager data_manager_;
  MockAutocompleteHistoryManager history_;
  testing::NiceMock<MockManualFillingController> mock_mf_controller_;
  TestAutofillManager af_manager_;
  TestingProfile profile_;
};

TEST_F(CreditCardAccessoryControllerTest, RefreshSuggestions) {
  autofill::CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      base::string16());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));

  auto* cc_controller = controller();
  ASSERT_TRUE(cc_controller);
  cc_controller->RefreshSuggestions();

  ASSERT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(card.ObfuscatedLastFourDigits(),
                       card.ObfuscatedLastFourDigits(), card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(card.ExpirationMonthAsString())
          .AppendSimpleField(card.Expiration4DigitYearAsString())
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .Build());
}

TEST_F(CreditCardAccessoryControllerTest, PreventsFillingInsecureContexts) {
  autofill::CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      base::string16());
  SetFormOrigin(GURL("http://insecure.http-site.com"));

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  controller()->RefreshSuggestions();

  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .SetWarning(l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_WARNING_INSECURE_CONNECTION))
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .AppendField(card.ExpirationMonthAsString(),
                             card.ExpirationMonthAsString(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .AppendField(card.Expiration4DigitYearAsString(),
                             card.Expiration4DigitYearAsString(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .AppendField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
                             card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .Build());
}

TEST_F(CreditCardAccessoryControllerTest, ServerCardUnmask) {
  autofill::CreditCard card = test::GetMaskedServerCard();
  data_manager_.AddCreditCard(card);
  data_manager_.AddCreditCard(test::GetCreditCard());

  auto* cc_controller = controller();
  ASSERT_TRUE(cc_controller);
  cc_controller->RefreshSuggestions();

  UserInfo::Field field(card.ObfuscatedLastFourDigits(),
                        card.ObfuscatedLastFourDigits(), card.guid(),
                        /*is_obfuscated=*/false,
                        /*selectable=*/true);

  autofill::CreditCard card_to_unmask;

  base::string16 expected_number = kFirstTwelveDigits + card.number();

  EXPECT_CALL(mock_af_driver_,
              RendererShouldFillFieldWithValue(expected_number));

  cc_controller->OnFillingTriggered(field);
}

}  // namespace autofill
