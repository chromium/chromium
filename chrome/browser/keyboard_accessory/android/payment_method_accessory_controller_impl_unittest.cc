// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::InSequence;
using testing::SaveArg;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;

namespace autofill {
namespace {

AccessorySheetData::Builder PaymentMethodAccessorySheetDataBuilder() {
  return AccessorySheetData::Builder(AccessoryTabType::CREDIT_CARDS,
                                     /*user_info_title=*/std::u16string(),
                                     /*plus_address_title=*/std::u16string())
      .AppendFooterCommand(
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
          AccessoryAction::MANAGE_CREDIT_CARDS);
}

}  // namespace

class TestAccessManager : public CreditCardAccessManager {
 public:
  using CreditCardAccessManager::CreditCardAccessManager;
  void FetchCreditCard(
      const CreditCard* card,
      OnCreditCardFetchedCallback on_credit_card_fetched) override {
    std::move(on_credit_card_fetched).Run(CHECK_DEREF(card));
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

class PaymentMethodAccessoryControllerTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit PaymentMethodAccessoryControllerTestBase(GURL url) : url_(url) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(url());
    autofill_client().set_last_committed_primary_main_frame_url(url());
    FocusWebContentsOnMainFrame();

    test_api(autofill_manager())
        .set_credit_card_access_manager(
            std::make_unique<TestAccessManager>(&autofill_manager()));
    PaymentMethodAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &paydm(),
        &valuables_data_manager(), &autofill_manager(), &autofill_driver());
    controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());
    paydm().SetPrefService(profile()->GetPrefs());
    paydm().SetSyncServiceForTest(&sync_service_);
  }

  void TearDown() override {
    paydm().SetSyncServiceForTest(nullptr);
    paydm().SetPrefService(nullptr);
    paydm().ClearCreditCards();
    paydm().ClearCreditCardOfferData();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  const GURL& url() const { return url_; }

  PaymentMethodAccessoryController* controller() {
    return PaymentMethodAccessoryControllerImpl::FromWebContents(web_contents());
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
    return *autofill_client()
                .GetPaymentsAutofillClient()
                ->GetIbanAccessManager();
  }

  TestPaymentsDataManager& paydm() { return paydm_; }

  TestValuablesDataManager& valuables_data_manager() {
    return valuables_data_manager_;
  }

  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;
 private:
  syncer::TestSyncService sync_service_;
  TestPaymentsDataManager paydm_;
  TestValuablesDataManager valuables_data_manager_;
  testing::NiceMock<MockManualFillingController> mock_mf_controller_;
  GURL url_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<testing::NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
};

// Test with a secure context ("https").
class PaymentMethodAccessoryControllerTest
    : public PaymentMethodAccessoryControllerTestBase {
 public:
  PaymentMethodAccessoryControllerTest()
      : PaymentMethodAccessoryControllerTestBase(GURL("https://example.com")) {}
};

TEST_F(PaymentMethodAccessoryControllerTest, RefreshSuggestions) {
  CreditCard card = test::GetCreditCard();
  paydm().AddCreditCard(card);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(
              AccessorySuggestionType::kCreditCardNumber,
              card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              /*a11y_description=*/
              card.ObfuscatedNumberWithVisibleLastFourDigits() + u" Visa",
              card.guid(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
          .Build());
}

// Test with an insecure context ("ttps").
class PaymentMethodAccessoryControllerTest_Insecure
    : public PaymentMethodAccessoryControllerTestBase {
 public:
  PaymentMethodAccessoryControllerTest_Insecure()
      : PaymentMethodAccessoryControllerTestBase(
            GURL("http://insecure.http-site.com")) {}
};

TEST_F(PaymentMethodAccessoryControllerTest_Insecure,
       PreventsFillingInsecureContexts) {
  CreditCard card = test::GetCreditCard();
  paydm().AddCreditCard(card);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .SetWarning(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_WARNING_INSECURE_CONNECTION))
          .AddUserInfo(kVisaCard)
          .AppendField(
              AccessorySuggestionType::kCreditCardNumber,
              card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              /*a11y_description=*/
              card.ObfuscatedNumberWithVisibleLastFourDigits() + u" Visa",
              card.guid(),
              /*is_obfuscated=*/false,
              /*selectable=*/false)
          .AppendField(AccessorySuggestionType::kCreditCardExpirationMonth,
                       card.Expiration2DigitMonthAsString(),
                       card.Expiration2DigitMonthAsString(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/false)
          .AppendField(AccessorySuggestionType::kCreditCardExpirationYear,
                       card.Expiration4DigitYearAsString(),
                       card.Expiration4DigitYearAsString(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/false)
          .AppendField(AccessorySuggestionType::kCreditCardNameFull,
                       card.GetRawInfo(CREDIT_CARD_NAME_FULL),
                       card.GetRawInfo(CREDIT_CARD_NAME_FULL),
                       /*is_obfuscated=*/false,
                       /*selectable=*/false)
          .AppendField(AccessorySuggestionType::kCreditCardCvc,
                       std::u16string(), std::u16string(),
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
      case CreditCard::RecordType::kVirtualCard: {
        // The PaymentMethodAccessoryController will automatically create a virtual
        // card for this masked server card.
        CreditCard card = test::GetMaskedServerCard();
        card.set_virtual_card_enrollment_state(
            CreditCard::VirtualCardEnrollmentState::kEnrolled);
        return card;
      }
      case CreditCard::RecordType::kFullServerCard:
        // Full server cards are never unmasked, so they are not tested.
        NOTREACHED();
    }
  }

  bool IsMaskedServerCard() {
    return GetParam() == CreditCard::RecordType::kMaskedServerCard;
  }
};

TEST_P(PaymentMethodAccessoryControllerCardUnmaskTest, CardUnmask) {
  CreditCard card = GetCreditCard();
  paydm().AddCreditCard(card);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field =
      AccessorySheetField::Builder()
          .SetSuggestionType(AccessorySuggestionType::kCreditCardNumber)
          .SetDisplayText(card.ObfuscatedNumberWithVisibleLastFourDigits())
          .SetId(card.guid())
          .SetSelectable(true)
          .Build();

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
    // Full server cards are never unmasked, so they should not be present in
    // this test.
    testing::Values(CreditCard::RecordType::kLocalCard,
                    CreditCard::RecordType::kMaskedServerCard,
                    CreditCard::RecordType::kVirtualCard));

TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsAddsCachedVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard unmasked_card = test::GetCreditCard();
  paydm().AddCreditCard(unmasked_card);
  // Update the record type to kVirtualCard and add it to the unmasked cards
  // cache.
  unmasked_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  std::u16string cvc = u"123";
  autofill_manager().GetCreditCardAccessManager()->CacheUnmaskedCardInfo(
      unmasked_card, cvc);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = unmasked_card.FullDigitsForDisplay();
  std::u16string card_number_for_fill =
      unmasked_card.GetRawInfo(CREDIT_CARD_NUMBER);
  // Verify that the unmasked virtual card is at the top followed by the masked
  // card.
  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(
              /*suggestion_type=*/AccessorySuggestionType::kCreditCardNumber,
              /*display_text=*/card_number_for_display,
              /*text_to_fill=*/card_number_for_fill,
              /*a11y_description=*/card_number_for_fill + u" Visa",
              /*id=*/std::string(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc, cvc)
          .AddUserInfo(kVisaCard)
          .AppendField(
              AccessorySuggestionType::kCreditCardNumber,
              unmasked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              /*a11y_description=*/
              unmasked_card.ObfuscatedNumberWithVisibleLastFourDigits() +
                  u" Visa",
              unmasked_card.guid(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
          .Build());
}

TEST_F(
    PaymentMethodAccessoryControllerTest,
    RefreshSuggestionsAddsVirtualCardWhenOriginalCardIsEnrolledForVirtualCards) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  paydm().AddCreditCard(masked_card);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string virtual_card_label =
      u"Virtual card " +
      masked_card.ObfuscatedNumberWithVisibleLastFourDigits();
  // Verify that a virtual card is inserted before the actual masked card.
  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kMasterCard)
          .AppendField(AccessorySuggestionType::kCreditCardNumber,
                       virtual_card_label, /*text_to_fill*/ std::u16string(),
                       /*a11y_description=*/virtual_card_label + u" Mastercard",
                       masked_card.guid() + "_vcn",
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
          .AddUserInfo(kMasterCard)
          .AppendField(AccessorySuggestionType::kCreditCardNumber,
                       masked_card.ObfuscatedNumberWithVisibleLastFourDigits(),
                       /*text_to_fill*/ std::u16string(),
                       /*a11y_description=*/
                       masked_card.ObfuscatedNumberWithVisibleLastFourDigits() +
                           u" Mastercard",
                       masked_card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              masked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             masked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             masked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
          .Build());
}

// Test to ensure that cards enrolled in card info retrieval are unmasked
// in the manual fallback bubble.
TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestion_CardInfoRetrievalCardsRemainUnmasked) {
  // Add a masked card to PersonalDataManager.
  CreditCard unmasked_card = test::GetCreditCard();
  unmasked_card.set_card_info_retrieval_enrollment_state(
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled);
  paydm().AddCreditCard(unmasked_card);
  // Update the record type and add it to the unmasked cards cache.
  unmasked_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  std::u16string cvc = u"123";
  autofill_manager().GetCreditCardAccessManager()->CacheUnmaskedCardInfo(
      unmasked_card, cvc);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = unmasked_card.FullDigitsForDisplay();
  std::u16string card_number_for_fill =
      unmasked_card.GetRawInfo(CREDIT_CARD_NUMBER);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(
              /*suggestion_type=*/AccessorySuggestionType::kCreditCardNumber,
              /*display_text=*/card_number_for_display,
              /*text_to_fill=*/card_number_for_fill,
              /*a11y_description=*/card_number_for_fill + u" Visa",
              /*id=*/std::string(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              unmasked_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             unmasked_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             unmasked_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc, cvc)
          .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest,
       CardArtIsNotShownEvenWhenMetadataIsAvailable) {
  // Add a masked card to PersonalDataManager.
  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_card_art_url(GURL("http://www.example.com/image.png"));
  masked_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  paydm().AddCreditCard(masked_card);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::optional<AccessorySheetData> result = controller()->GetSheetData();
  EXPECT_TRUE(result.has_value());
  // Verify both the virtual card and the masked server card are in the
  // suggestions.
  EXPECT_EQ(result->user_info_list().size(), 2u);
  // Verify card art is not shown for the virtual card.
  EXPECT_EQ(result->user_info_list()[0].icon_url(), GURL());
  // Verify card art is not shown for the masked server card.
  EXPECT_EQ(result->user_info_list()[1].icon_url(), GURL());
}

// Tests that promo codes are shown.
TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsWithPromoCodeOffers) {
  CreditCard card = test::GetCreditCard();
  paydm().AddCreditCard(card);
  // Getting a promo code whose |merchant_origins| contains AutofillClient's
  // |last_committed_url_|.
  AutofillOfferData promo_code_valid = test::GetPromoCodeOfferData(
      /*origin=*/GURL(url()),
      /*is_expired=*/false);
  AutofillOfferData promo_code_origin_mismatch = test::GetPromoCodeOfferData(
      /*merchant_origin=*/GURL("https://someorigin.com"),
      /*is_expired=*/false);
  AutofillOfferData promo_code_expired = test::GetPromoCodeOfferData(
      /*origin=*/GURL(url()),
      /*is_expired=*/true);
  paydm().AddAutofillOfferData(promo_code_valid);
  paydm().AddAutofillOfferData(promo_code_origin_mismatch);
  paydm().AddAutofillOfferData(promo_code_expired);
  AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                            /*user_info_title=*/std::u16string(),
                            /*plus_address_title=*/std::u16string());

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  // Only valid promo code should appear in the AccessorySheet.
  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(
              AccessorySuggestionType::kCreditCardNumber,
              card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              /*a11y_description=*/
              card.ObfuscatedNumberWithVisibleLastFourDigits() + u" Visa",
              card.guid(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
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
  paydm().AddCreditCard(card);

  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  paydm().AddAsLocalIban(iban);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  // IBANs should appear in the AccessorySheet.
  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(kVisaCard)
          .AppendField(
              AccessorySuggestionType::kCreditCardNumber,
              card.ObfuscatedNumberWithVisibleLastFourDigits(),
              /*text_to_fill=*/std::u16string(),
              /*a11y_description=*/
              card.ObfuscatedNumberWithVisibleLastFourDigits() + u" Visa",
              card.guid(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc,
                             std::u16string())
          .AddIbanInfo(iban.GetIdentifierStringForAutofillDisplay(),
                       iban.value(), /*id=*/"")
          .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest, FetchLocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  std::string guid = paydm().AddAsLocalIban(iban);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field =
      AccessorySheetField::Builder()
          .SetSuggestionType(AccessorySuggestionType::kIban)
          .SetDisplayText(iban.GetIdentifierStringForAutofillDisplay())
          .SetTextToFill(iban.value())
          .SetSelectable(true)
          .Build();

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
  Iban iban = test::GetServerIban();
  paydm().AddServerIban(iban);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  AccessorySheetField field =
      AccessorySheetField::Builder()
          .SetSuggestionType(AccessorySuggestionType::kIban)
          .SetDisplayText(iban.GetIdentifierStringForAutofillDisplay())
          .SetTextToFill(iban.value())
          .SetId(base::NumberToString(iban.instrument_id()))
          .SetSelectable(true)
          .Build();

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  EXPECT_CALL(iban_access_manager(), FetchValue);

  controller()->OnFillingTriggered(field_id, field);
}

TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsWithLoyaltyCards) {
  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(valuables_data_manager()).AddLoyaltyCard(loyalty_card);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(
          AccessoryTabType::CREDIT_CARDS,
          /*user_info_title=*/
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE),
          /*plus_address_title=*/std::u16string())
          .AddLoyaltyCardInfo(
              loyalty_card.merchant_name(), loyalty_card.program_logo(),
              base::UTF8ToUTF16(loyalty_card.loyalty_card_number()))
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
              AccessoryAction::MANAGE_CREDIT_CARDS)
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_LOYALTY_CARDS_LINK),
              AccessoryAction::MANAGE_LOYALTY_CARDS)
          .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest, LoyaltyCardDataIsChangedBySync) {
  {
    InSequence seq;
    // First, there're no loyalty cards, so the filling source is not available.
    EXPECT_CALL(filling_source_observer_,
                Run(controller(), IsFillingSourceAvailable(false)));
    // The filling source should become available after a loyalty card is added.
    EXPECT_CALL(filling_source_observer_,
                Run(controller(), IsFillingSourceAvailable(true)));
  }

  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(
          AccessoryTabType::CREDIT_CARDS,
          /*user_info_title=*/
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE),
          /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
              AccessoryAction::MANAGE_CREDIT_CARDS)
          .Build());

  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(valuables_data_manager()).AddLoyaltyCard(loyalty_card);
  test_api(valuables_data_manager()).NotifyObservers();

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(
          AccessoryTabType::CREDIT_CARDS,
          /*user_info_title=*/
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_EMPTY_MESSAGE),
          /*plus_address_title=*/std::u16string())
          .AddLoyaltyCardInfo(
              loyalty_card.merchant_name(), loyalty_card.program_logo(),
              base::UTF8ToUTF16(loyalty_card.loyalty_card_number()))
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
              AccessoryAction::MANAGE_CREDIT_CARDS)
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_LOYALTY_CARDS_LINK),
              AccessoryAction::MANAGE_LOYALTY_CARDS)
          .Build());
}

TEST_F(PaymentMethodAccessoryControllerTest, FillLoyaltyCardNumber) {
  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  ASSERT_TRUE(rfh);
  FieldGlobalId field_id{.frame_token = LocalFrameToken(*rfh->GetFrameToken()),
                         .renderer_id = FieldRendererId(123)};

  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  LoyaltyCardInfo loyalty_card_info(
      loyalty_card.merchant_name(), loyalty_card.program_logo(),
      base::UTF8ToUTF16(loyalty_card.loyalty_card_number()));
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(mojom::FieldActionType::kReplaceAll,
                               mojom::ActionPersistence::kFill, field_id,
                               loyalty_card_info.value().text_to_fill()));

  controller()->OnFillingTriggered(field_id, loyalty_card_info.value());
}

TEST_F(PaymentMethodAccessoryControllerTest,
       RefreshSuggestionsWithCachedBnplCard) {
  CreditCard bnpl_card = test::GetVirtualCard();
  bnpl_card.set_issuer_id(kBnplAffirmIssuerId);
  bnpl_card.SetNickname(BnplIssuerIdToDisplayName(
      ConvertToBnplIssuerIdEnum(bnpl_card.issuer_id())));
  bnpl_card.set_is_bnpl_card(true);
  bnpl_card.set_guid("bnpl-card-guid");
  bnpl_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnspecified);

  std::u16string cvc = u"123";
  autofill_manager().GetCreditCardAccessManager()->CacheUnmaskedCardInfo(
      bnpl_card, cvc);
  paydm().AddCreditCard(bnpl_card);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  ASSERT_TRUE(controller());
  controller()->RefreshSuggestions();

  std::u16string card_number_for_display = bnpl_card.FullDigitsForDisplay();
  std::u16string card_number_for_fill =
      bnpl_card.GetRawInfo(CREDIT_CARD_NUMBER);
  std::u16string unmasked_a11y = card_number_for_fill + u" " + u"Affirm";

  EXPECT_EQ(
      controller()->GetSheetData(),
      PaymentMethodAccessorySheetDataBuilder()
          .AddUserInfo(bnpl_card.issuer_id())
          .AppendField(
              /*suggestion_type=*/AccessorySuggestionType::kCreditCardNumber,
              /*display_text=*/card_number_for_display,
              /*text_to_fill=*/card_number_for_fill,
              /*a11y_description=*/unmasked_a11y,
              /*id=*/std::string(),
              /*is_obfuscated=*/false,
              /*selectable=*/true)
          .AppendSimpleField(
              AccessorySuggestionType::kCreditCardExpirationMonth,
              bnpl_card.Expiration2DigitMonthAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardExpirationYear,
                             bnpl_card.Expiration4DigitYearAsString())
          .AppendSimpleField(AccessorySuggestionType::kCreditCardNameFull,
                             bnpl_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
          .AppendSimpleField(AccessorySuggestionType::kCreditCardCvc, cvc)
          .Build());
}

}  // namespace autofill
