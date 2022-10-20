// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
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

  void FetchCreditCard(const CreditCard* card,
                       base::WeakPtr<Accessor> accessor) override {
    accessor->OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card_, u"");
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
      : af_manager_(&mock_af_driver_, &client_) {
    af_manager_.set_credit_card_access_manager_for_test(
        std::make_unique<TestAccessManager>(&mock_af_driver_, &client_,
                                            &data_manager_));
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableManualFallbackForVirtualCards,
         features::kAutofillShowUnmaskedCachedCardInManualFillingView},
        /*disabled_features=*/{features::kAutofillFillMerchantPromoCodeFields});
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
    data_manager_.ClearCreditCardOfferData();
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
    // Promo codes are filtered by the last_committed_primary_main_frame_url.
    client_.set_last_committed_primary_main_frame_url(GURL(kExampleSite));
  }

 protected:
  TestAutofillClient client_;
  testing::NiceMock<MockAutofillDriver> mock_af_driver_;
  TestPersonalDataManager data_manager_;
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kAutofillShowUnmaskedCachedCardInManualFillingView,
            features::kAutofillFillMerchantPromoCodeFields});
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

class CreditCardAccessoryControllerTestSupportingPromoCodeOffers
    : public CreditCardAccessoryControllerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillShowUnmaskedCachedCardInManualFillingView,
         features::kAutofillFillMerchantPromoCodeFields},
        /*disabled_features=*/{});
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

TEST_F(CreditCardAccessoryControllerTest,
       AllowedForWebContentsForNonVirtualCards) {
  prefs::SetPaymentsIntegrationEnabled(profile()->GetPrefs(), true);
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profile());
  personal_data_manager->SetSyncingForTest(true);
  // Add a non-virtual card.
  CreditCard card = test::GetMaskedServerCard();
  personal_data_manager->AddServerCreditCardForTest(
      std::make_unique<CreditCard>(card));

  // Verify that the accessory sheet is not allowed.
  ASSERT_FALSE(
      CreditCardAccessoryController::AllowedForWebContents(web_contents()));
}

TEST_F(CreditCardAccessoryControllerTest,
       AllowedForWebContentsForVirtualCards) {
  prefs::SetPaymentsIntegrationEnabled(profile()->GetPrefs(), true);
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(profile());
  personal_data_manager->SetSyncingForTest(true);
  // Add a virtual card.
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(CreditCard::ENROLLED);
  personal_data_manager->AddServerCreditCardForTest(
      std::make_unique<CreditCard>(card));

  // Verify that the accessory sheet is allowed.
  ASSERT_TRUE(
      CreditCardAccessoryController::AllowedForWebContents(web_contents()));
}

TEST_F(CreditCardAccessoryControllerTest, RefreshSuggestions) {
  CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  AccessorySheetData result(AccessoryTabType::CREDIT_CARDS, std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .Build());
}

TEST_F(CreditCardAccessoryControllerTest, PreventsFillingInsecureContexts) {
  CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
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
                             /*text_to_fill=*/std::u16string(),
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
                .AppendField(card.GetRawInfo(CREDIT_CARD_NAME_FULL),
                             card.GetRawInfo(CREDIT_CARD_NAME_FULL),
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

  CreditCard card = test::GetMaskedServerCard();
  data_manager_.AddCreditCard(card);
  data_manager_.AddCreditCard(test::GetCreditCard());

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field(card.ObfuscatedLastFourDigits(),
                            /*text_to_fill=*/std::u16string(),
                            card.ObfuscatedLastFourDigits(), card.guid(),
                            /*is_obfuscated=*/false,
                            /*selectable=*/true);

  CreditCard card_to_unmask;

  std::u16string expected_number = kFirstTwelveDigits + card.number();

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
      features::kAutofillShowUnmaskedCachedCardInManualFillingView);
  // Store a full server card in the credit_card_access_manager's
  // unmasked_cards_cache.
  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(card);
  std::u16string cvc = u"123";
  af_manager_.GetCreditCardAccessManager()->CacheUnmaskedCardInfo(card, cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = card.FullDigitsForDisplay();
  std::u16string card_number_for_fill = card.GetRawInfo(CREDIT_CARD_NUMBER);
  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the full card number and the cvc fields are added to the
  // accessory sheet data.
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(/*display_text=*/card_number_for_display,
                             /*text_to_fill=*/card_number_for_fill,
                             /*a11y_description=*/card_number_for_fill,
                             /*id=*/std::string(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(cvc)
                .Build());
}

TEST_F(CreditCardAccessoryControllerTest, UnmaskedCacheCardsReorderedToTheTop) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  data_manager_.AddCreditCard(masked_card);
  // Add a full server card to PersonalDataManager and also cache it in the
  // CreditCardAccessManager.
  CreditCard unmasked_card = test::GetCreditCard();
  unmasked_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(unmasked_card);
  std::u16string cvc = u"123";
  af_manager_.GetCreditCardAccessManager()->CacheUnmaskedCardInfo(unmasked_card,
                                                                  cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = unmasked_card.FullDigitsForDisplay();
  std::u16string card_number_for_fill =
      unmasked_card.GetRawInfo(CREDIT_CARD_NUMBER);
  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the unmasked card is at the top followed by the masked card.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(/*display_text=*/card_number_for_display,
                       /*text_to_fill=*/card_number_for_fill,
                       /*a11y_description=*/card_number_for_fill,
                       /*id=*/std::string(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(cvc)
          .AddUserInfo(kMasterCard)
          .AppendField(masked_card.ObfuscatedLastFourDigits(),
                       /*text_to_fill=*/std::u16string(),
                       masked_card.ObfuscatedLastFourDigits(),
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(CreditCardAccessoryControllerTestWithoutSupportingUnmaskedCards,
       RefreshSuggestionsUnmaskedCachedCard) {
  // Store a full server card in the credit_card_access_manager's
  // unmasked_cards_cache.
  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::FULL_SERVER_CARD);
  data_manager_.AddCreditCard(card);
  std::u16string cvc = u"123";
  af_manager_.GetCreditCardAccessManager()->CacheUnmaskedCardInfo(card, cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Since the experiment is disabled, verify that the only the obfuscated last
  // four and no cvc is added to the accessory sheet data.
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .Build());
}

TEST_F(CreditCardAccessoryControllerTestWithoutSupportingUnmaskedCards,
       RefreshSuggestionsAddsCachedVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard unmasked_card = test::GetCreditCard();
  data_manager_.AddCreditCard(unmasked_card);
  // Update the record type to VIRTUAL_CARD and add it to the unmasked cards
  // cache.
  unmasked_card.set_record_type(CreditCard::VIRTUAL_CARD);
  std::u16string cvc = u"123";
  af_manager_.GetCreditCardAccessManager()->CacheUnmaskedCardInfo(unmasked_card,
                                                                  cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = unmasked_card.FullDigitsForDisplay();
  std::u16string card_number_for_fill =
      unmasked_card.GetRawInfo(CREDIT_CARD_NUMBER);
  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the unmasked virtual card is at the top followed by the masked
  // card.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(/*display_text=*/card_number_for_display,
                       /*text_to_fill=*/card_number_for_fill,
                       /*a11y_description=*/card_number_for_fill,
                       /*id=*/std::string(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(cvc)
          .AddUserInfo(kVisaCard)
          .AppendField(unmasked_card.ObfuscatedLastFourDigits(),
                       /*text_to_fill=*/std::u16string(),
                       unmasked_card.ObfuscatedLastFourDigits(),
                       unmasked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(
    CreditCardAccessoryControllerTest,
    RefreshSuggestionsAddsVirtualCardWhenOriginalCardIsEnrolledForVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_virtual_card_enrollment_state(CreditCard::ENROLLED);
  data_manager_.AddCreditCard(masked_card);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string virtual_card_label =
      u"Virtual card " + masked_card.ObfuscatedLastFourDigits();
  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that a virtual card is inserted before the actual masked card.
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kMasterCard)
          .AppendField(virtual_card_label, /*text_to_fill*/ std::u16string(),
                       virtual_card_label, masked_card.guid() + "_vcn",
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .AddUserInfo(kMasterCard)
          .AppendField(masked_card.ObfuscatedLastFourDigits(),
                       /*text_to_fill*/ std::u16string(),
                       masked_card.ObfuscatedLastFourDigits(),
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(CreditCardAccessoryControllerTest, VirtualCreditCardWithCardArtUrl) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_card_art_url(GURL("http://www.example.com/image.png"));
  masked_card.set_virtual_card_enrollment_state(CreditCard::ENROLLED);
  data_manager_.AddCreditCard(masked_card);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that a virtual card is inserted before the actual masked card.
  std::u16string virtual_card_label =
      u"Virtual card " + masked_card.ObfuscatedLastFourDigits();
  EXPECT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo(kMasterCard, UserInfo::IsExactMatch(true),
                       GURL("http://www.example.com/image.png"))
          .AppendField(virtual_card_label, /*text_to_fill*/ std::u16string(),
                       virtual_card_label, masked_card.guid() + "_vcn",
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .AddUserInfo(kMasterCard, UserInfo::IsExactMatch(true))
          .AppendField(masked_card.ObfuscatedLastFourDigits(),
                       /*text_to_fill*/ std::u16string(),
                       masked_card.ObfuscatedLastFourDigits(),
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

// Tests that when |kAutofillFillMerchantPromoCodeFields| feature is enabled,
// promo codes are shown.
TEST_F(CreditCardAccessoryControllerTestSupportingPromoCodeOffers,
       RefreshSuggestionsWithPromoCodeOffers) {
  CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  // Getting a promo code whose |merchant_origins| contains AutofillClient's
  // |last_committed_url_|.
  AutofillOfferData promo_code_valid = test::GetPromoCodeOfferData(
      /*merchant_origin=*/GURL(kExampleSite),
      /*is_expired=*/false);
  AutofillOfferData promo_code_origin_mismatch = test::GetPromoCodeOfferData(
      /*merchant_origin=*/GURL("https://someorigin.com"),
      /*is_expired=*/false);
  AutofillOfferData promo_code_expired = test::GetPromoCodeOfferData(
      /*merchant_origin=*/GURL(kExampleSite),
      /*is_expired=*/true);
  data_manager_.AddAutofillOfferData(promo_code_valid);
  data_manager_.AddAutofillOfferData(promo_code_origin_mismatch);
  data_manager_.AddAutofillOfferData(promo_code_expired);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Only valid promo code should appear in the AccessorySheet.
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .AddPromoCodeInfo(
                    base::ASCIIToUTF16(promo_code_valid.GetPromoCode()),
                    base::ASCIIToUTF16(
                        promo_code_valid.GetDisplayStrings().value_prop_text))
                .Build());
}

// Tests that when |kAutofillFillMerchantPromoCodeFields| feature is disabled,
// promo codes are not shown.
TEST_F(CreditCardAccessoryControllerTest,
       RefreshSuggestionsWithPromoCodeOffers) {
  CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);
  AutofillOfferData promo_code = test::GetPromoCodeOfferData(
      /*merchant_origin=*/GURL(kExampleSite));
  data_manager_.AddAutofillOfferData(promo_code);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Promo code offers are available, but not shown.
  EXPECT_EQ(result,
            CreditCardAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedLastFourDigits(), card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .Build());
}

}  // namespace autofill
