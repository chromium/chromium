// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/test/run_until.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/test_autofill_external_delegate.h"
#include "components/autofill/core/common/signatures.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// This implementation is for testing. It lets us manually control and simulate
// the moment an SMS is received for one-time passwords (OTP).
class FakeSmsOtpBackend : public one_time_tokens::SmsOtpBackend {
 public:
  using CallbackType =
      base::OnceCallback<void(const one_time_tokens::OtpFetchReply&)>;

  FakeSmsOtpBackend() = default;
  ~FakeSmsOtpBackend() override = default;

  // one_time_tokens::SmsOtpBackend:
  void RetrieveSmsOtp(CallbackType callback) override;

  // Simulates the reception of an SMS.
  void NotifyCallbacks(const one_time_tokens::OtpFetchReply& reply);

  size_t num_callbacks() const { return callbacks_.size(); }

 private:
  std::vector<CallbackType> callbacks_;
};

void FakeSmsOtpBackend::RetrieveSmsOtp(
    FakeSmsOtpBackend::CallbackType callback) {
  callbacks_.push_back(std::move(callback));
}

void FakeSmsOtpBackend::NotifyCallbacks(
    const one_time_tokens::OtpFetchReply& reply) {
  for (auto& callback : callbacks_) {
    std::move(callback).Run(reply);
  }
  callbacks_.clear();
}

// AutofillCrowdsourcingManager that classifies every field as a ONE_TIME_CODE.
class FakeAutofillCrowdsourcingManager : public AutofillCrowdsourcingManager {
 public:
  FakeAutofillCrowdsourcingManager(AutofillClient* autofill_client,
                                   version_info::Channel channel);
  ~FakeAutofillCrowdsourcingManager() override = default;

  bool StartQueryRequest(
      const std::vector<raw_ptr<const FormStructure, VectorExperimental>>&
          forms,
      std::optional<net::IsolationInfo> isolation_info,
      base::OnceCallback<void(std::optional<QueryResponse>)> callback) override;

  bool StartUploadRequest(std::vector<AutofillUploadContents> upload_contents,
                          mojom::SubmissionSource form_submission_source,
                          bool is_password_manager_upload) override;
};

FakeAutofillCrowdsourcingManager::FakeAutofillCrowdsourcingManager(
    AutofillClient* autofill_client,
    version_info::Channel channel)
    : AutofillCrowdsourcingManager(autofill_client, channel) {}

bool FakeAutofillCrowdsourcingManager::StartQueryRequest(
    const std::vector<raw_ptr<const FormStructure, VectorExperimental>>& forms,
    std::optional<net::IsolationInfo> isolation_info,
    base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
  // Generate a response that classifies each field as a ONE_TIME_CODE field.
  std::vector<FormSignature> queried_form_signatures;
  AutofillQueryResponse response;
  for (const FormStructure* form : forms) {
    queried_form_signatures.push_back(form->form_signature());
    auto* form_suggestion = response.add_form_suggestions();
    for (const auto& field : form->fields()) {
      auto* field_suggestion = form_suggestion->add_field_suggestions();
      field_suggestion->set_field_signature(
          CalculateFieldSignatureForField(*field).value());
      *field_suggestion->add_predictions() =
          test::CreateFieldPrediction(ONE_TIME_CODE, /*is_override=*/false);
    }
  }

  std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse{
      base::Base64Encode(response.SerializeAsString()),
      queried_form_signatures});
  return true;
}

bool FakeAutofillCrowdsourcingManager::StartUploadRequest(
    std::vector<AutofillUploadContents> upload_contents,
    mojom::SubmissionSource form_submission_source,
    bool is_password_manager_upload) {
  // Overridden to prevent network communication from tests.
  return true;
}

class OtpTestAutofillManager : public BrowserAutofillManager {
 public:
  explicit OtpTestAutofillManager(ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver) {
    test_api(*this).SetExternalDelegate(
        std::make_unique<TestAutofillExternalDelegate>(this));
  }
  ~OtpTestAutofillManager() override = default;

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

  [[nodiscard]] testing::AssertionResult WaitForSuggestionsShown(
      int min_num_awaited_calls) {
    return suggestions_shown_waiter_.Wait(min_num_awaited_calls);
  }

  TestAutofillExternalDelegate& external_delegate() {
    return *static_cast<TestAutofillExternalDelegate*>(
        test_api(*this).external_delegate());
  }

 private:
  TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {AutofillManagerEvent::kFormsSeen}};

  TestAutofillManagerWaiter suggestions_shown_waiter_{
      *this,
      {AutofillManagerEvent::kAskForValuesToFill}};
};

class OtpTestAutofillClient : public TestContentAutofillClient {
 public:
  explicit OtpTestAutofillClient(content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    set_crowdsourcing_manager(
        std::make_unique<FakeAutofillCrowdsourcingManager>(
            this, version_info::Channel::STABLE));
    set_sms_otp_backend(std::make_unique<FakeSmsOtpBackend>());
    set_one_time_token_service(
        std::make_unique<one_time_tokens::OneTimeTokenServiceImpl>(
            GetSmsOtpBackend()));
  }
  ~OtpTestAutofillClient() override = default;

  FakeSmsOtpBackend& sms_otp_backend() {
    return *static_cast<FakeSmsOtpBackend*>(GetSmsOtpBackend());
  }
};

}  // namespace

class OtpManagerBrowserTest : public PlatformBrowserTest {
  void SetUpOnMainThread() override;

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  OtpTestAutofillManager& autofill_manager() {
    return *autofill_manager_injector_[web_contents()];
  }

  OtpTestAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  // The server communication is mocked out, so we don't need to disable it
  // in the test.
  test::AutofillBrowserTestEnvironment autofill_test_environment_{
      {.disable_server_communication = false}};

  TestAutofillClientInjector<OtpTestAutofillClient> autofill_client_injector_;
  TestAutofillManagerInjector<OtpTestAutofillManager>
      autofill_manager_injector_;
};

void OtpManagerBrowserTest::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

// Test fixture for the interaction between the WebOTP API and Autofill for
// OTPs. If the parameter is true, the WebOTP API should be invoked in the test.
class OtpManagerWithWebOtpApiBrowserTest
    : public OtpManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool is_webotp_api_used() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    OtpManagerWithWebOtpApiBrowserTest,
    ::testing::Bool());

// This integration test ensures that an OTP is correctly suggested by Autofill
// by default (when the WebOTP API is not used) but that an OTP does not get
// autofilled if a website calls the WebOTP API before.
IN_PROC_BROWSER_TEST_P(OtpManagerWithWebOtpApiBrowserTest,
                       SmsOtpAutofillIntegrationTest) {
  GURL url = embedded_test_server()->GetURL("/autofill/sms_otp_form.html");

  // Navigate to page and wait for form to be classified
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(autofill_manager().WaitForFormsSeen(1));

  // Immediately when a form field is classified as an OTP field, a subscription
  // for OTPs should be started if an SMS OTP backend exists.
  ASSERT_EQ(autofill_client().sms_otp_backend().num_callbacks(), 1u);

  // Using the WebOTP API should block the Autofill OTP feature.
  if (is_webotp_api_used()) {
    // Request an OTP via the WebOTP API. The JavaScript code returns true so
    // ExecJs does not wait for the resolution of the pending Promise.
    ASSERT_TRUE(ExecJs(web_contents(),
                       R"(navigator.credentials.get({
                              otp: {transport:['sms']}
                          });
                          true;)",
                       content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                       /*world_id=*/1));
    // As the WebOTP is asynchronous, wait until the use has been propagated.
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return autofill_client().DocumentUsedWebOTP(); }));
    // From this point on, Autofill should not suggest OTPs anymore.
  }

  // Simulate an OTP arriving.
  autofill_client().sms_otp_backend().NotifyCallbacks(
      one_time_tokens::OtpFetchReply(
          one_time_tokens::OneTimeToken(
              one_time_tokens::OneTimeTokenType::kSmsOtp, "123456",
              base::Time::Now()),
          /*request_complete=*/true));

  // Simulate click on field.
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms =
      autofill_manager().form_structures();
  ASSERT_EQ(forms.size(), 1u);
  const std::unique_ptr<FormStructure>& form = forms.begin()->second;
  const std::unique_ptr<AutofillField>& first_field = *form->fields().begin();
  autofill_manager().OnAskForValuesToFill(
      form->ToFormData(), first_field->global_id(), gfx::Rect(),
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*password_request=*/std::nullopt);
  ASSERT_TRUE(autofill_manager().WaitForSuggestionsShown(1));

  // Verify expectations: The OTP should be suggested by autofill unless the
  // WebOTP API was called.
  const TestAutofillExternalDelegate& external_delegate =
      autofill_manager().external_delegate();
  const std::vector<Suggestion>& suggestions = external_delegate.suggestions();
  if (is_webotp_api_used()) {
    EXPECT_EQ(suggestions.size(), 0u);
  } else {
    ASSERT_EQ(suggestions.size(), 1u);
    EXPECT_EQ(suggestions[0].main_text.value, u"123456");
  }
}

}  // namespace autofill
