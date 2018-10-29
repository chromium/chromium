// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"

#include <memory>

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
  void UploadSaveCardCallback(const base::string16& cardholder_name) {
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
  std::unique_ptr<base::DictionaryValue> legal_message;
  if (!legal_message_string.empty()) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::Read(legal_message_string));
    EXPECT_TRUE(value);
    base::DictionaryValue* dictionary;
    EXPECT_TRUE(value->GetAsDictionary(&dictionary));
    legal_message = dictionary->CreateDeepCopy();
  }
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      previous_save_credit_card_prompt_user_decision);
  if (is_uploading) {
    // Upload save infobar delegate:
    credit_card_to_save_ = credit_card;
    std::unique_ptr<ConfirmInfoBarDelegate> delegate(
        new AutofillSaveCardInfoBarDelegateMobile(
            is_uploading, credit_card, std::move(legal_message),
            /*strike_database=*/nullptr,
            /*upload_save_card_callback=*/
            base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                               UploadSaveCardCallback,
                           base::Unretained(this)),
            /*local_save_card_callback=*/base::Closure(),
            profile()->GetPrefs()));
    return delegate;
  }
  // Local save infobar delegate:
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new AutofillSaveCardInfoBarDelegateMobile(
          is_uploading, credit_card, std::move(legal_message),
          /*strike_database=*/nullptr,
          /*upload_save_card_callback=*/
          base::OnceCallback<void(const base::string16&)>(),
          /*local_save_card_callback=*/
          base::Bind(base::IgnoreResult(
                         &TestPersonalDataManager::SaveImportedCreditCard),
                     base::Unretained(personal_data_.get()), credit_card),
          profile()->GetPrefs()));
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

  // Infobar is not shown because the provided legal message is invalid.
  {
    base::HistogramTester histogram_tester;
    // Legal message is invalid because it's missing the url.
    std::string bad_legal_message =
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"Panda {0}.\","
        "     \"template_parameter\": [ {"
        "        \"display_text\": \"bear\""
        "     } ]"
        "  } ]"
        "}";
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessage(
            /* is_uploading= */ true, std::move(bad_legal_message),
            prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE));

    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardInfoBar.Server",
        AutofillMetrics::INFOBAR_NOT_SHOWN_INVALID_LEGAL_MESSAGE, 1);
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
