// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::SaveArg;
using testing::SaveArgPointee;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;

constexpr char kExampleSite[] = "https://example.com";
const std::u16string kFirstTwelveDigits = u"411111111111";

namespace autofill {
namespace {

class TestBrowserAutofillManager : public BrowserAutofillManager {
 public:
  TestBrowserAutofillManager(
      AutofillDriver* driver,
      AutofillClient* client,
      PersonalDataManager* personal_data,
      AutocompleteHistoryManager* autocomplete_history_manager,
      std::unique_ptr<CreditCardAccessManager> cc_access_manager = nullptr)
      // Force to use the constructor designated for unit test.
      : BrowserAutofillManager(
            driver,
            client,
            personal_data,
            autocomplete_history_manager,
            "en-US",
            AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
            std::move(cc_access_manager)) {}

  ~TestBrowserAutofillManager() override = default;

  const FormData& last_query_form() const override { return last_form_; }

  void SetLastForm(FormData form) { last_form_ = std::move(form); }

 private:
  FormData last_form_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserAutofillManager);
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
      : CreditCardAccessManager(driver,
                                client,
                                personal_data,
                                /*credit_card_form_event_logger=*/nullptr) {
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
  MOCK_METHOD2(RendererShouldFillFieldWithValue,
               void(const FieldGlobalId& field, const std::u16string&));
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
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillShowUnmaskedCachedCardInManualFillingView);
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    SetFormOrigin(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &data_manager_,
        &af_manager_, &mock_af_driver_);
    data_manager_.SetPrefService(profile()->GetPrefs());
  }

  void TearDown() override {
    data_manager_.SetPrefService(nullptr);
    data_manager_.ClearCreditCards();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CreditCardAccessoryController* controller() {
    return CreditCardAccessoryControllerImpl::FromWebContents(web_contents());
  }

  void SetFormOrigin(GURL origin) {
    FormData form;
    form.unique_renderer_id = FormRendererId(1);
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
  TestBrowserAutofillManager af_manager_;
  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class CreditCardAccessoryControllerTestWithoutSupportingUnmaskedCards
    : public CreditCardAccessoryControllerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        autofill::features::kAutofillShowUnmaskedCachedCardInManualFillingView);
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    SetFormOrigin(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &data_manager_,
        &af_manager_, &mock_af_driver_);
    data_manager_.SetPrefService(profile()->GetPrefs());
  }
};

TEST_F(CreditCardAccessoryControllerTest, RefreshSuggestions) {
  autofill::CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(card.ObfuscatedLastFourDigits(),
                       card.ObfuscatedLastFourDigits(), card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(card.Expiration2DigitMonthAsString())
          .AppendSimpleField(card.Expiration4DigitYearAsString())
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(CreditCardAccessoryControllerTest, PreventsFillingInsecureContexts) {
  autofill::CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      std::u16string());
  SetFormOrigin(GURL("http://insecure.http-site.com"));

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .SetWarning(l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_WARNING_INSECURE_CONNECTION))
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .AppendField(card.Expiration2DigitMonthAsString(),
                             card.Expiration2DigitMonthAsString(),
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
                .AppendField(std::u16string(), std::u16string(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/false)
                .Build());
}

TEST_F(CreditCardAccessoryControllerTest, ServerCardUnmask) {
  // TODO(crbug.com/1169167): Move this into setup once controllers don't push
  // updated sheets proactively anymore.
  controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());

  autofill::CreditCard card = test::GetMaskedServerCard();
  data_manager_.AddCreditCard(card);
  data_manager_.AddCreditCard(test::GetCreditCard());

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  UserInfo::Field field(card.ObfuscatedLastFourDigits(),
                        card.ObfuscatedLastFourDigits(), card.guid(),
                        /*is_obfuscated=*/false,
                        /*selectable=*/true);

  autofill::CreditCard card_to_unmask;

  std::u16string expected_number = kFirstTwelveDigits + card.number();

  // TODO(crbug/1187858): Fill in correct renderer ID here.
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  EXPECT_CALL(mock_af_driver_,
              RendererShouldFillFieldWithValue(field_id, expected_number));

  controller()->OnFillingTriggered(field_id, field);
}

TEST_F(CreditCardAccessoryControllerTest,
       RefreshSuggestionsUnmaskedCachedCard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      autofill::features::kAutofillShowUnmaskedCachedCardInManualFillingView);
  // Store a full server card in the credit_card_access_manager's
  // unmasked_cards_cache.
  autofill::CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(card);
  std::u16string cvc = u"123";
  af_manager_.credit_card_access_manager()->CacheUnmaskedCardInfo(card, cvc);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the full card number and the cvc fields are added to the
  // accessory sheet data.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NUMBER))
          .AppendSimpleField(card.Expiration2DigitMonthAsString())
          .AppendSimpleField(card.Expiration4DigitYearAsString())
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(cvc)
          .Build());
}

TEST_F(CreditCardAccessoryControllerTest, UnmaskedCacheCardsReorderedToTheTop) {
  // Add a masked card to PersonalDataManager.
  autofill::CreditCard masked_card = test::GetMaskedServerCard();
  data_manager_.AddCreditCard(masked_card);
  // Add a full server card to PersonalDataManager and also cache it in hte
  // CreditCardAccessManager.
  autofill::CreditCard unmasked_card = test::GetCreditCard();
  unmasked_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(unmasked_card);
  std::u16string cvc = u"123";
  af_manager_.credit_card_access_manager()->CacheUnmaskedCardInfo(unmasked_card,
                                                                  cvc);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the unmasked card is at the top followed by the masked card.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendSimpleField(
              unmasked_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER))
          .AppendSimpleField(unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(
              unmasked_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(cvc)
          .AddUserInfo(kMasterCard)
          .AppendField(masked_card.ObfuscatedLastFourDigits(),
                       masked_card.ObfuscatedLastFourDigits(),
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(
              masked_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(CreditCardAccessoryControllerTestWithoutSupportingUnmaskedCards,
       RefreshSuggestionsUnmaskedCachedCard) {
  // Store a full server card in the credit_card_access_manager's
  // unmasked_cards_cache.
  autofill::CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(card);
  std::u16string cvc = u"123";
  af_manager_.credit_card_access_manager()->CacheUnmaskedCardInfo(card, cvc);
  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Since the experiment is disabled, verify that the only the obfuscated last
  // four and no cvc is added to the accessory sheet data.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(card.ObfuscatedLastFourDigits(),
                       card.ObfuscatedLastFourDigits(), card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(card.Expiration2DigitMonthAsString())
          .AppendSimpleField(card.Expiration4DigitYearAsString())
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

}  // namespace autofill
