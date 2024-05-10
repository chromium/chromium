// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::SaveArg;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;

constexpr char kExampleSite[] = "https://example.com";

namespace autofill {
namespace {

AccessorySheetData::Builder PaymentMethodAccessorySheetDataBuilder() {
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
                                /*credit_card_form_event_logger=*/nullptr) {}

  void FetchCreditCard(
      const CreditCard* card,
      OnCreditCardFetchedCallback on_credit_card_fetched) override {
    std::move(on_credit_card_fetched)
        .Run(CreditCardFetchResult::kSuccess, card);
  }
};

class MockAutofillDriver : public TestContentAutofillDriver {
 public:
  using TestContentAutofillDriver::TestContentAutofillDriver;
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType action_type,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field,
               const std::u16string&),
              (override));
};

class PaymentMethodAccessoryControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    SetFormOrigin(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    test_api(autofill_manager())
        .set_credit_card_access_manager(std::make_unique<TestAccessManager>(
            &autofill_driver(), &autofill_client(), &data_manager_));
    PaymentMethodAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &data_manager_,
        &autofill_manager(), &autofill_driver());
    data_manager_.SetPrefService(profile()->GetPrefs());
    data_manager_.SetSyncServiceForTest(&sync_service_);
  }

  void TearDown() override {
    data_manager_.SetSyncServiceForTest(nullptr);
    data_manager_.SetPrefService(nullptr);
    data_manager_.test_payments_data_manager().ClearCreditCards();
    data_manager_.test_payments_data_manager().ClearCreditCardOfferData();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PaymentMethodAccessoryController* controller() {
    return PaymentMethodAccessoryControllerImpl::FromWebContents(web_contents());
  }

  void SetFormOrigin(GURL origin) {
    FormData form;
    form.renderer_id = FormRendererId(1);
    form.action = origin;
    form.main_frame_origin = url::Origin::Create(origin);
    autofill_client().set_form_origin(origin);
    // Promo codes are filtered by the last_committed_primary_main_frame_url.
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL(kExampleSite));
  }

 protected:
  TestContentAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  testing::NiceMock<MockAutofillDriver>& autofill_driver() {
    return *autofill_driver_injector_[web_contents()];
  }

  TestBrowserAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  MockIbanAccessManager& iban_access_manager() {
    return *static_cast<MockIbanAccessManager*>(
        autofill_client().GetIbanAccessManager());
  }

  syncer::TestSyncService sync_service_;
  TestPersonalDataManager data_manager_;
  testing::NiceMock<MockManualFillingController> mock_mf_controller_;
  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<testing::NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
};

TEST_F(PaymentMethodAccessoryControllerTest, RefreshSuggestions) {
  CreditCard card = test::GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(card);
  AccessorySheetData result(AccessoryTabType::CREDIT_CARDS, std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  EXPECT_EQ(result,
            PaymentMethodAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest, PreventsFillingInsecureContexts) {
  CreditCard card = test::GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(card);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  SetFormOrigin(GURL("http://insecure.http-site.com"));

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  EXPECT_EQ(result,
            PaymentMethodAccessorySheetDataBuilder()
                .SetWarning(l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_WARNING_INSECURE_CONNECTION))
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             card.guid(),
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

class PaymentMethodAccessoryControllerCardUnmaskTest
    : public PaymentMethodAccessoryControllerTest,
      public testing::WithParamInterface<CreditCard::RecordType> {
 public:
  PaymentMethodAccessoryControllerCardUnmaskTest() = default;
  ~PaymentMethodAccessoryControllerCardUnmaskTest() override = default;

  CreditCard GetCreditCard() {
    switch (GetParam()) {
      case CreditCard::RecordType::kLocalCard:
        return test::GetCreditCard();
      case CreditCard::RecordType::kMaskedServerCard:
        return test::GetMaskedServerCard();
      case CreditCard::RecordType::kFullServerCard:
        return test::GetFullServerCard();
      case CreditCard::RecordType::kVirtualCard: {
        // The PaymentMethodAccessoryController will automatically create a virtual
        // card for this masked server card.
        CreditCard card = test::GetMaskedServerCard();
        card.set_virtual_card_enrollment_state(
            CreditCard::VirtualCardEnrollmentState::kEnrolled);
        return card;
      }
    }
  }

  bool IsMaskedServerCard() {
    return GetParam() == CreditCard::RecordType::kMaskedServerCard;
  }
};

TEST_P(PaymentMethodAccessoryControllerCardUnmaskTest, CardUnmask) {
  // TODO(crbug.com/40165275): Move this into setup once controllers don't push
  // updated sheets proactively anymore.
  controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());

  CreditCard card = GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(card);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                            /*text_to_fill=*/std::u16string(),
                            card.ObfuscatedNumberWithVisibleLastFourDigits(),
                            card.guid(),
                            /*is_obfuscated=*/false,
                            /*selectable=*/true);

  CreditCard card_to_unmask;

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                               mojom::ActionPersistence::kFill, field_id,
                               card.number()));

  controller()->OnFillingTriggered(field_id, field);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PaymentMethodAccessoryControllerCardUnmaskTest,
    testing::Values(CreditCard::RecordType::kLocalCard,
                    CreditCard::RecordType::kMaskedServerCard,
                    CreditCard::RecordType::kFullServerCard,
                    CreditCard::RecordType::kVirtualCard));

TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsUnmaskedCachedCardNotAdded) {
  // Store a full server card in the credit_card_access_manager's
  // unmasked_cards_cache.
  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  data_manager_.payments_data_manager().AddCreditCard(card);
  std::u16string cvc = u"123";
  autofill_manager().GetCreditCardAccessManager().CacheUnmaskedCardInfo(card,
                                                                        cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that the only the obfuscated last four and no cvc is added to the
  // accessory sheet data.
  EXPECT_EQ(result,
            PaymentMethodAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsAddsCachedVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard unmasked_card = test::GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(unmasked_card);
  // Update the record type to kVirtualCard and add it to the unmasked cards
  // cache.
  unmasked_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  std::u16string cvc = u"123";
  autofill_manager().GetCreditCardAccessManager().CacheUnmaskedCardInfo(
      unmasked_card, cvc);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
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
      PaymentMethodAccessorySheetDataBuilder()
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
          .AppendField(
              unmasked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              unmasked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
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
    PaymentMethodAccessoryControllerTest,
    RefreshSuggestionsAddsVirtualCardWhenOriginalCardIsEnrolledForVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  data_manager_.payments_data_manager().AddCreditCard(masked_card);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string virtual_card_label =
      u"Virtual card " +
      masked_card.ObfuscatedNumberWithVisibleLastFourDigits();
  EXPECT_EQ(result, controller()->GetSheetData());
  // Verify that a virtual card is inserted before the actual masked card.
  EXPECT_EQ(
      result,
      PaymentMethodAccessorySheetDataBuilder()
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
          .AppendField(masked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                       /*text_to_fill*/ std::u16string(),
                       masked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(std::u16string())
          .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest,
       CardArtIsNotShownEvenWhenMetadataIsAvailableAndEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardArtImage);

  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_card_art_url(GURL("http://www.example.com/image.png"));
  masked_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  data_manager_.payments_data_manager().AddCreditCard(masked_card);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());

  // Verify both the virtual card and the masked server card are in the
  // suggestions.
  EXPECT_EQ(result.user_info_list().size(), 2u);
  // Verify card art is not shown for the virtual card.
  EXPECT_EQ(result.user_info_list()[0].icon_url(), GURL());
  // Verify card art is not shown for the masked server card.
  EXPECT_EQ(result.user_info_list()[1].icon_url(), GURL());
}

TEST_F(
    PaymentMethodAccessoryControllerTest,
    CapitalOneVirtualCardIconIsShownForVirtualCardsEvenWhenMetadataIsNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableCardArtImage);

  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
  masked_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  data_manager_.payments_data_manager().AddCreditCard(masked_card);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());
  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());

  // Verify both the virtual card and the masked server card are in the
  // suggestions.
  EXPECT_EQ(result.user_info_list().size(), 2u);
  // Verify the the Capital One virtual card icon is shown for the virtual card.
  EXPECT_EQ(result.user_info_list()[0].icon_url(), GURL(kCapitalOneCardArtUrl));
  // Verify card art is not shown for the masked server card.
  EXPECT_EQ(result.user_info_list()[1].icon_url(), GURL());
}

// Tests that promo codes are shown.
TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsWithPromoCodeOffers) {
  CreditCard card = test::GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(card);
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
  data_manager_.test_payments_data_manager().AddAutofillOfferData(
      promo_code_valid);
  data_manager_.test_payments_data_manager().AddAutofillOfferData(
      promo_code_origin_mismatch);
  data_manager_.test_payments_data_manager().AddAutofillOfferData(
      promo_code_expired);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // Only valid promo code should appear in the AccessorySheet.
  EXPECT_EQ(result,
            PaymentMethodAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             card.guid(),
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

// Tests that both credit cards and IBANs are shown.
TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsWithCreditCardAndIbans) {
  CreditCard card = test::GetCreditCard();
  data_manager_.payments_data_manager().AddCreditCard(card);

  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  std::string guid =
      data_manager_.test_payments_data_manager().AddAsLocalIban(iban);

  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            std::u16string());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(result, controller()->GetSheetData());
  // IBANs should appear in the AccessorySheet.
  EXPECT_EQ(result,
            PaymentMethodAccessorySheetDataBuilder()
                .AddUserInfo(kVisaCard)
                .AppendField(card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             /*text_to_fill=*/std::u16string(),
                             card.ObfuscatedNumberWithVisibleLastFourDigits(),
                             card.guid(),
                             /*is_obfuscated=*/false,
                             /*selectable=*/true)
                .AppendSimpleField(card.Expiration2DigitMonthAsString())
                .AppendSimpleField(card.Expiration4DigitYearAsString())
                .AppendSimpleField(card.GetRawInfo(CREDIT_CARD_NAME_FULL))
                .AppendSimpleField(std::u16string())
                .AddIbanInfo(iban.GetIdentifierStringForAutofillDisplay(),
                             iban.value(), guid)
                .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest, FetchLocalIban) {
  controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());

  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  std::string guid =
      data_manager_.test_payments_data_manager().AddAsLocalIban(iban);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field(iban.GetIdentifierStringForAutofillDisplay(),
                            /*text_to_fill=*/iban.value(),
                            iban.GetIdentifierStringForAutofillDisplay(),
                            /*id=*/"",
                            /*is_obfuscated=*/false,
                            /*selectable=*/true);

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                               mojom::ActionPersistence::kFill, field_id,
                               iban.value()));

  controller()->OnFillingTriggered(field_id, field);
}

TEST_F(PaymentMethodAccessoryControllerTest, FetchServerIban) {
  controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());

  Iban iban = test::GetServerIban();
  data_manager_.test_payments_data_manager().AddServerIban(iban);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field(iban.GetIdentifierStringForAutofillDisplay(),
                            /*text_to_fill=*/iban.value(),
                            iban.GetIdentifierStringForAutofillDisplay(),
                            /*id=*/base::NumberToString(iban.instrument_id()),
                            /*is_obfuscated=*/false,
                            /*selectable=*/true);

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  EXPECT_CALL(iban_access_manager(), FetchValue);

  controller()->OnFillingTriggered(field_id, field);
}

}  // namespace autofill
