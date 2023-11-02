// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {

class AutofillSaveCardInfoBarDelegateMobileTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSaveCardInfoBarDelegateMobileTest();

  AutofillSaveCardInfoBarDelegateMobileTest(
      const AutofillSaveCardInfoBarDelegateMobileTest&) = delete;
  AutofillSaveCardInfoBarDelegateMobileTest& operator=(
      const AutofillSaveCardInfoBarDelegateMobileTest&) = delete;

  ~AutofillSaveCardInfoBarDelegateMobileTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> CreateDelegate(
      bool is_uploading,
      CreditCard credit_card = CreditCard());
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateDelegateWithLegalMessage(
      bool is_uploading,
      std::string legal_message_string,
      CreditCard credit_card = CreditCard());
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateDelegateWithLegalMessageAndOptions(
      bool is_uploading,
      std::string legal_message_string,
      AutofillClient::SaveCreditCardOptions options,
      CreditCard credit_card = CreditCard());

  std::unique_ptr<TestPersonalDataManager> personal_data_;

 private:
  void LocalSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision) {
    personal_data_.get()->SaveImportedCreditCard(credit_card_to_save_);
  }

  void UploadSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      const AutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {
    personal_data_.get()->SaveImportedCreditCard(credit_card_to_save_);
  }

  CreditCard credit_card_to_save_;
};

AutofillSaveCardInfoBarDelegateMobileTest::
    AutofillSaveCardInfoBarDelegateMobileTest() {}

AutofillSaveCardInfoBarDelegateMobileTest::
    ~AutofillSaveCardInfoBarDelegateMobileTest() {}

void AutofillSaveCardInfoBarDelegateMobileTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  personal_data_ = std::make_unique<TestPersonalDataManager>();
  personal_data_->SetPrefService(profile()->GetPrefs());
}

void AutofillSaveCardInfoBarDelegateMobileTest::TearDown() {
  personal_data_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegate(
    bool is_uploading,
    CreditCard credit_card) {
  return CreateDelegateWithLegalMessage(is_uploading, "", credit_card);
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegateWithLegalMessage(
    bool is_uploading,
    std::string legal_message_string,
    CreditCard credit_card) {
  return CreateDelegateWithLegalMessageAndOptions(
      is_uploading, legal_message_string,
      AutofillClient::SaveCreditCardOptions(), credit_card);
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::
    CreateDelegateWithLegalMessageAndOptions(
        bool is_uploading,
        std::string legal_message_string,
        AutofillClient::SaveCreditCardOptions options,
        CreditCard credit_card) {
  LegalMessageLines legal_message_lines;
  if (!legal_message_string.empty()) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::ReadDeprecated(legal_message_string));
    EXPECT_TRUE(value);
    LegalMessageLine::Parse(*value, &legal_message_lines,
                            /*escape_apostrophes=*/true);
  }
  if (is_uploading) {
    // Upload save infobar delegate:
    credit_card_to_save_ = credit_card;
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate(
        new AutofillSaveCardInfoBarDelegateMobile(
            is_uploading, options, credit_card, legal_message_lines,
            /*upload_save_card_callback=*/
            base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                               UploadSaveCardPromptCallback,
                           base::Unretained(this)),
            /*local_save_card_callback=*/{}, AccountInfo()));
    return delegate;
  }
  // Local save infobar delegate:
  credit_card_to_save_ = credit_card;
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate(
      new AutofillSaveCardInfoBarDelegateMobile(
          is_uploading, options, credit_card, legal_message_lines,
          /*upload_save_card_callback=*/{},
          /*local_save_card_callback=*/
          base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                             LocalSaveCardPromptCallback,
                         base::Unretained(this)),
          AccountInfo()));
  return delegate;
}

// Test that local credit card save infobar metrics are logged correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Local_Main) {
  ::testing::InSequence dummy;

  // Infobar is shown.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Accept the infobar.
  {
    personal_data_->ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Local",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Local",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Local",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
  }
}

// Test that server credit card save infobar metrics are logged correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Server_Main) {
  ::testing::InSequence dummy;

  // Infobar is shown.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Infobar is still shown when the legal message is successfully parsed.
  {
    base::HistogramTester histogram_tester;
    std::string good_legal_message =
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"This is the entire message.\""
        "  } ]"
        "}";
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessage(
            /* is_uploading= */ true, std::move(good_legal_message)));

    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Accept the infobar.
  {
    personal_data_->ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 1);
  }

  // Accept the infobar which should request an expiration date.
  {
    personal_data_->ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_expiration_date_from_user(true)));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingExpirationDate",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    // kAccept of "Autofill.CreditCardSaveFlowResult.Server" should only be
    // recorded when all data is collected.
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 0);
  }

  // Accept the infobar which should request a cardholder name.
  {
    personal_data_->ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_name_from_user(true)));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingCardholderName",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    // kAccept of "Autofill.CreditCardSaveFlowResult.Server" should only be
    // recorded when all data is collected.
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 0);
  }

  // Dismiss the infobar which doesn't request any data from user.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
  }

  // Dismiss the infobar which should request an expiration date.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_expiration_date_from_user(true)));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingExpirationDate",
        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
  }

  // Dismiss the infobar which should request a cardholder name.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_name_from_user(true)));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingCardholderName",
        AutofillMetrics::INFOBAR_DENIED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
        autofill_metrics::SaveCreditCardPromptResult::kDenied, 1);
  }

  // Ignore the infobar which doesn't request any data from user.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
  }

  // Ignore the infobar which should request an expiration date.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_expiration_date_from_user(true)));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingExpirationDate",
        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
  }

  // Ignore the infobar which should request a cardholder name.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            AutofillClient::SaveCreditCardOptions()
                .with_should_request_name_from_user(true)));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server.RequestingCardholderName",
        AutofillMetrics::INFOBAR_IGNORED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, 1);
  }
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, LocalCardHasNickname) {
  CreditCard card = test::GetCreditCard();
  card.SetNickname(u"Nickname");
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate(/*is_uploading=*/true, card);
  EXPECT_EQ(delegate->card_label(), card.NicknameAndLastFourDigitsForTesting());
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, LocalCardHasNoNickname) {
  CreditCard card = test::GetCreditCard();
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate(/*is_uploading=*/true, card);
  EXPECT_EQ(delegate->card_label(), card.NetworkAndLastFourDigits());
}

}  // namespace autofill
