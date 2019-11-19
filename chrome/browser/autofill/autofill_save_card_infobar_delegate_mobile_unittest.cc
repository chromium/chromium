// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
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
  ~AutofillSaveCardInfoBarDelegateMobileTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<ConfirmInfoBarDelegate> CreateDelegate(
      bool is_uploading,
      prefs::PreviousSaveCreditCardPromptUserDecision
          previous_save_credit_card_prompt_user_decision =
              prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  std::unique_ptr<ConfirmInfoBarDelegate> CreateDelegateWithLegalMessage(
      bool is_uploading,
      std::string legal_message_string,
      prefs::PreviousSaveCreditCardPromptUserDecision
          previous_save_credit_card_prompt_user_decision);

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

  DISALLOW_COPY_AND_ASSIGN(AutofillSaveCardInfoBarDelegateMobileTest);
};

AutofillSaveCardInfoBarDelegateMobileTest::
    AutofillSaveCardInfoBarDelegateMobileTest() {}

AutofillSaveCardInfoBarDelegateMobileTest::
    ~AutofillSaveCardInfoBarDelegateMobileTest() {}

void AutofillSaveCardInfoBarDelegateMobileTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  personal_data_.reset(new TestPersonalDataManager());
  personal_data_->SetPrefService(profile()->GetPrefs());

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
}

void AutofillSaveCardInfoBarDelegateMobileTest::TearDown() {
  personal_data_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<ConfirmInfoBarDelegate>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegate(
    bool is_uploading,
    prefs::PreviousSaveCreditCardPromptUserDecision
        previous_save_credit_card_prompt_user_decision) {
  return CreateDelegateWithLegalMessage(
      is_uploading, "", previous_save_credit_card_prompt_user_decision);
}

std::unique_ptr<ConfirmInfoBarDelegate>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegateWithLegalMessage(
    bool is_uploading,
    std::string legal_message_string,
    prefs::PreviousSaveCreditCardPromptUserDecision
        previous_save_credit_card_prompt_user_decision) {
  CreditCard credit_card;
  LegalMessageLines legal_message_lines;
  if (!legal_message_string.empty()) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::ReadDeprecated(legal_message_string));
    EXPECT_TRUE(value);
    LegalMessageLine::Parse(*value, &legal_message_lines,
                            /*escape_apostrophes=*/true);
  }
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      previous_save_credit_card_prompt_user_decision);
  if (is_uploading) {
    // Upload save infobar delegate:
    credit_card_to_save_ = credit_card;
    std::unique_ptr<ConfirmInfoBarDelegate> delegate(
        new AutofillSaveCardInfoBarDelegateMobile(
            is_uploading, AutofillClient::SaveCreditCardOptions(), credit_card,
            legal_message_lines,
            /*upload_save_card_callback=*/
            base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                               UploadSaveCardPromptCallback,
                           base::Unretained(this)),
            /*local_save_card_callback=*/{}, profile()->GetPrefs(),
            /*is_off_the_record=*/false));
    return delegate;
  }
  // Local save infobar delegate:
  credit_card_to_save_ = credit_card;
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new AutofillSaveCardInfoBarDelegateMobile(
          is_uploading, AutofillClient::SaveCreditCardOptions(), credit_card,
          legal_message_lines,
          /*upload_save_card_callback=*/{},
          /*local_save_card_callback=*/
          base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                             LocalSaveCardPromptCallback,
                         base::Unretained(this)),
          profile()->GetPrefs(), /*is_off_the_record=*/false));
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
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
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
            /* is_uploading= */ true, std::move(good_legal_message),
            prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE));

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
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

// Test that local credit card save infobar previous-decision metrics are logged
// correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest,
       Metrics_Local_PreviousDecision) {
  ::testing::InSequence dummy;

  // NoPreviousDecision
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.NoPreviousDecision",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.NoPreviousDecision",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // PreviouslyAccepted
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.PreviouslyAccepted",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.PreviouslyAccepted",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // PreviouslyDenied
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.PreviouslyDenied",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Local.PreviouslyDenied",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }
}

// Test that server credit card save infobar metrics are logged correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest,
       Metrics_Server_PreviousDecision) {
  ::testing::InSequence dummy;

  // NoPreviousDecision
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.NoPreviousDecision",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.NoPreviousDecision",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // PreviouslyAccepted
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.PreviouslyAccepted",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.PreviouslyAccepted",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // PreviouslyDenied
  {
    personal_data_->ClearCreditCards();
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED));

    EXPECT_TRUE(infobar->Accept());
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.PreviouslyDenied",
        AutofillMetrics::INFOBAR_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.CreditCardInfoBar.Server.PreviouslyDenied",
        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }
}

}  // namespace autofill
