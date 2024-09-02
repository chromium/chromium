// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {
namespace {

class TestDeviceLockBridge : public DeviceLockBridge {
 public:
  TestDeviceLockBridge() = default;
  TestDeviceLockBridge(const TestDeviceLockBridge&) = delete;
  TestDeviceLockBridge& operator=(const TestDeviceLockBridge&) = delete;

  bool ShouldShowDeviceLockUi() override { return false; }
};


using CardSaveType = payments::PaymentsAutofillClient::CardSaveType;
using SaveCreditCardOptions =
    payments::PaymentsAutofillClient::SaveCreditCardOptions;

class AutofillSaveCardInfoBarDelegateMobileTest
    : public ChromeRenderViewHostTestHarness {
 public:
  struct CreateDelegateOptions {
    int logo_icon_id = -1;
    std::u16string title_text;
    std::u16string confirm_text;
    std::u16string cancel_text;
    std::u16string description_text;
    bool is_google_pay_branding_enabled = false;
  };

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
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> CreateDelegate(
      CreateDelegateOptions options);
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateDelegateWithLegalMessage(
      bool is_uploading,
      std::string legal_message_string,
      CreditCard credit_card = CreditCard());
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateDelegateWithOptions(bool is_uploading,
                            SaveCreditCardOptions options,
                            CreditCard credit_card = CreditCard());
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateDelegateWithLegalMessageAndOptions(
      bool is_uploading,
      std::string legal_message_string,
      SaveCreditCardOptions options,
      CreditCard credit_card = CreditCard());
  void CheckInfobarAcceptReturnValue(ConfirmInfoBarDelegate* infobar_delegate);

  std::unique_ptr<TestPersonalDataManager> personal_data_;

 private:
  void LocalSaveCardPromptCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision
          user_decision) {
    personal_data_->test_payments_data_manager().SaveImportedCreditCard(
        credit_card_to_save_);
  }

  void UploadSaveCardPromptCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {
    personal_data_->test_payments_data_manager().SaveImportedCreditCard(
        credit_card_to_save_);
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
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegate(
    CreateDelegateOptions options) {
  AutofillSaveCardUiInfo ui_info;
  ui_info.logo_icon_id = options.logo_icon_id;
  ui_info.title_text = options.title_text;
  ui_info.confirm_text = options.confirm_text;
  ui_info.cancel_text = options.cancel_text;
  ui_info.description_text = options.description_text;
  ui_info.is_google_pay_branding_enabled =
      options.is_google_pay_branding_enabled;
#if BUILDFLAG(IS_ANDROID)
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegateAndroid>(
      (payments::PaymentsAutofillClient::LocalSaveCardPromptCallback)
          base::DoNothing(),
      SaveCreditCardOptions(), web_contents());
  save_card_delegate->SetDeviceLockBridgeForTesting(
      std::make_unique<TestDeviceLockBridge>());
#else
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegate>(
      (payments::PaymentsAutofillClient::LocalSaveCardPromptCallback)
          base::DoNothing(),
      SaveCreditCardOptions());
#endif
  return std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
      std::move(ui_info), std::move(save_card_delegate));
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegateWithOptions(
    bool is_uploading,
    SaveCreditCardOptions options,
    CreditCard credit_card) {
  return CreateDelegateWithLegalMessageAndOptions(
      is_uploading, /* legal_message_string= */ "", options, credit_card);
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegateWithLegalMessage(
    bool is_uploading,
    std::string legal_message_string,
    CreditCard credit_card) {
  return CreateDelegateWithLegalMessageAndOptions(
      is_uploading, legal_message_string, SaveCreditCardOptions(), credit_card);
}

std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
AutofillSaveCardInfoBarDelegateMobileTest::
    CreateDelegateWithLegalMessageAndOptions(bool is_uploading,
                                             std::string legal_message_string,
                                             SaveCreditCardOptions options,
                                             CreditCard credit_card) {
  LegalMessageLines legal_message_lines;
  if (!legal_message_string.empty()) {
    std::optional<base::Value> value =
        base::JSONReader::Read(legal_message_string);
    EXPECT_TRUE(value);
    LegalMessageLine::Parse(value->GetDict(), &legal_message_lines,
                            /*escape_apostrophes=*/true);
  }

  credit_card_to_save_ = credit_card;
  absl::variant<payments::PaymentsAutofillClient::LocalSaveCardPromptCallback,
                payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>
      save_card_callback;
  AutofillSaveCardUiInfo ui_info;
  if (is_uploading) {
    ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
        options, credit_card, legal_message_lines, AccountInfo());
    save_card_callback =
        base::BindOnce(&AutofillSaveCardInfoBarDelegateMobileTest::
                           UploadSaveCardPromptCallback,
                       base::Unretained(this));
  } else {
    ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(options, credit_card);
    save_card_callback = base::BindOnce(
        &AutofillSaveCardInfoBarDelegateMobileTest::LocalSaveCardPromptCallback,
        base::Unretained(this));
  }

#if BUILDFLAG(IS_ANDROID)
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegateAndroid>(
      std::move(save_card_callback), options, web_contents());
  save_card_delegate->SetDeviceLockBridgeForTesting(
      std::make_unique<TestDeviceLockBridge>());
#else
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegate>(
      std::move(save_card_callback), options);
#endif
  return std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
      std::move(ui_info), std::move(save_card_delegate));
}

void AutofillSaveCardInfoBarDelegateMobileTest::CheckInfobarAcceptReturnValue(
    ConfirmInfoBarDelegate* infobar_delegate) {
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(infobar_delegate->Accept());
#else
  EXPECT_TRUE(infobar_delegate->Accept());
#endif
}

// Test that local credit card save infobar metrics are logged correctly.
// TODO(crbug.com/40286922) Split metrics tests into smaller test.
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
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ false));

    base::HistogramTester histogram_tester;

    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
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
// TODO(crbug.com/40286922) Split metrics tests into smaller test.
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
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(
        /* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCardSaveFlowResult.Server",
        autofill_metrics::SaveCreditCardPromptResult::kAccepted, 1);
  }

  // Accept the infobar which should request an expiration date.
  {
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            SaveCreditCardOptions()
                .with_should_request_expiration_date_from_user(true)));

    base::HistogramTester histogram_tester;
    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
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
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegateWithLegalMessageAndOptions(
            /* is_uploading= */ true, /* legal_message_string= */ "",
            SaveCreditCardOptions().with_should_request_name_from_user(true)));

    base::HistogramTester histogram_tester;
    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
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
            SaveCreditCardOptions()
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
            SaveCreditCardOptions().with_should_request_name_from_user(true)));

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
            SaveCreditCardOptions()
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
            SaveCreditCardOptions().with_should_request_name_from_user(true)));

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

// Test that CVC-only local save infobar metrics are logged correctly.
// TODO(crbug.com/40286922) Split metrics tests into smaller test.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Cvc_Local_Main) {
  ::testing::InSequence dummy;

  // Infobar is shown.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ false, SaveCreditCardOptions().with_card_save_type(
                                       CardSaveType::kCvcSaveOnly)));

    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Local",
                                        AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Accept the infobar.
  {
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ false, SaveCreditCardOptions().with_card_save_type(
                                       CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;

    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Local",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ false, SaveCreditCardOptions().with_card_save_type(
                                       CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Local",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ false, SaveCreditCardOptions().with_card_save_type(
                                       CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Local",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

// Test that CVC-only upload save infobar metrics are logged correctly.
// TODO(crbug.com/40286922) Split metrics tests into smaller test.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Cvc_Server_Main) {
  ::testing::InSequence dummy;

  // Infobar is shown.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ true, SaveCreditCardOptions().with_card_save_type(
                                      CardSaveType::kCvcSaveOnly)));

    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Upload",
                                        AutofillMetrics::INFOBAR_SHOWN, 1);
  }

  // Accept the infobar.
  {
    personal_data_->test_payments_data_manager().ClearCreditCards();
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ true, SaveCreditCardOptions().with_card_save_type(
                                      CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;
    CheckInfobarAcceptReturnValue(infobar.get());
    ASSERT_EQ(1U,
              personal_data_->payments_data_manager().GetCreditCards().size());
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Upload",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ true, SaveCreditCardOptions().with_card_save_type(
                                      CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Upload",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegateWithOptions(
        /* is_uploading= */ true, SaveCreditCardOptions().with_card_save_type(
                                      CardSaveType::kCvcSaveOnly)));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CvcInfoBar.Upload",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
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

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, IsGooglePayBrandingEnabled) {
  for (bool param : {true, false}) {
    auto delegate = CreateDelegate({
        .is_google_pay_branding_enabled = param,
    });

    EXPECT_EQ(delegate->IsGooglePayBrandingEnabled(), param);
  }
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, GetDescriptionText) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate({
          .description_text = u"Mock Description Text",
      });

  EXPECT_EQ(delegate->GetDescriptionText(), u"Mock Description Text");
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, GetIconId) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate({
          .logo_icon_id = 123456,
      });

  EXPECT_EQ(delegate->GetIconId(), 123456);
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, GetMessageText) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate({
          .title_text = u"Mock Title Text",
      });

  EXPECT_EQ(delegate->GetMessageText(), u"Mock Title Text");
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, GetButtonLabelForOkButton) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate({
          .confirm_text = u"Mock Confirm Text",
      });

  EXPECT_EQ(
      delegate->GetButtonLabel(
          AutofillSaveCardInfoBarDelegateMobile::InfoBarButton::BUTTON_OK),
      u"Mock Confirm Text");
}

TEST_F(AutofillSaveCardInfoBarDelegateMobileTest,
       GetButtonLabelForCancelButton) {
  std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate =
      CreateDelegate({
          .cancel_text = u"Mock Cancel Text",
      });

  EXPECT_EQ(
      delegate->GetButtonLabel(
          AutofillSaveCardInfoBarDelegateMobile::InfoBarButton::BUTTON_CANCEL),
      u"Mock Cancel Text");
}

}  // namespace
}  // namespace autofill
