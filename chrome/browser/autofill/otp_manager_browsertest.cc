// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/ui/test_autofill_external_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/signatures.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::testing::ElementsAre;

// AutofillCrowdsourcingManager that classifies every field as a ONE_TIME_CODE.
class FakeAutofillCrowdsourcingManager : public AutofillCrowdsourcingManager {
 public:
  FakeAutofillCrowdsourcingManager(AutofillClient* autofill_client,
                                   version_info::Channel channel);
  ~FakeAutofillCrowdsourcingManager() override = default;

  bool StartQueryRequest(
      const std::vector<FormData>& forms,
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
    const std::vector<FormData>& forms,
    std::optional<net::IsolationInfo> isolation_info,
    base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
  // Generate a response that classifies each field as a ONE_TIME_CODE field.
  std::vector<FormSignature> queried_form_signatures;
  AutofillQueryResponse response;
  for (const FormData& form : forms) {
    queried_form_signatures.push_back(CalculateFormSignature(form));
    auto* form_suggestion = response.add_form_suggestions();
    for (const FormFieldData& field : form.fields()) {
      auto* field_suggestion = form_suggestion->add_field_suggestions();
      field_suggestion->set_field_signature(
          CalculateFieldSignatureForField(field).value());
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
    set_sms_otp_service(std::make_unique<one_time_tokens::FakeSmsOtpService>());
    set_gmail_otp_backend(
        std::make_unique<one_time_tokens::FakeGmailOtpBackend>());
    set_affiliation_service(&affiliation_service_);
  }
  ~OtpTestAutofillClient() override { set_affiliation_service(nullptr); }

  // Prevents `PaymentMethodAccessoryControllerImpl` on Android from observing
  // `TestContentAutofillClient`'s member-owned data managers, which have
  // non-deterministic relative destruction order in `SupportsUserData` during
  // `WebContents` teardown.
  bool HasPersonalDataManager() const override { return false; }

  one_time_tokens::FakeSmsOtpService& sms_otp_service() {
    return static_cast<one_time_tokens::FakeSmsOtpService&>(
        *GetSmsOtpService());
  }

  one_time_tokens::FakeGmailOtpBackend& gmail_otp_backend() {
    return static_cast<one_time_tokens::FakeGmailOtpBackend&>(
        *GetGmailOtpBackend());
  }

 private:
  affiliations::FakeAffiliationService affiliation_service_;
};

}  // namespace

class OtpManagerBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
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

void OtpManagerBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
  // Slower test bots (chromeos, debug, etc.) are flaky due to slower loading
  // interacting with deferred commits.
  command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

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
  // for SMS OTPs should be started.
  ASSERT_EQ(autofill_client().sms_otp_service().num_subscribers(), 1u);

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
  autofill_client().sms_otp_service().SimulateOtpArrived(
      one_time_tokens::OneTimeToken(one_time_tokens::OneTimeTokenType::kSmsOtp,
                                    "123456", base::TimeTicks::Now()));

  // Simulate click on field.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost())
      .Wait();
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "otp");
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

class OtpManagerGmailOtpBrowserTest : public OtpManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    OtpManagerBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_ =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            "chrome/test/data", GURL("https://example.test"));
    autofill_client().sms_otp_service().set_platform_supported(false);
    prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                             true);
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    OtpManagerBrowserTest::TearDownOnMainThread();
  }

 protected:
  void NavigateAndRequestGmailOtpSuggestions(std::string_view sender_address) {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(chrome_test_utils::NavigateToURL(
        web_contents(),
        GURL("https://example.test/autofill/sms_otp_form.html")));
    ASSERT_TRUE(autofill_manager().WaitForFormsSeen(1));

    autofill_client().gmail_otp_backend().ProcessCallbacks(
        one_time_tokens::OneTimeToken(one_time_tokens::OneTimeTokenType::kGmail,
                                      "123456", base::TimeTicks::Now(),
                                      std::string(sender_address)));
    // Wait for `GmailOtpRetriever` to finish the asynchronous sender domain
    // check.
    ASSERT_TRUE(base::test::RunUntil([&] {
      return !histogram_tester
                  .GetAllSamples(
                      "OneTimeTokens.GmailOtpRetriever."
                      "SenderDomainMatchAcceptedMatchType.Received")
                  .empty() ||
             !histogram_tester
                  .GetAllSamples(
                      "OneTimeTokens.GmailOtpRetriever."
                      "SenderDomainMatchRejectionReason.Received")
                  .empty();
    }));

    // Accepted tokens proactively trigger `AskForValuesToFill()` via
    // `RendererShouldTriggerSuggestions()`. For rejected tokens, simulate a
    // click on the field to request suggestions and verify none are shown.
    if (histogram_tester
            .GetAllSamples("OneTimeTokens.GmailOtpRetriever."
                           "SenderDomainMatchAcceptedMatchType.Received")
            .empty()) {
      content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
      content::MainThreadFrameObserver(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost())
          .Wait();
      content::SimulateMouseClickOrTapElementWithId(web_contents(), "otp");
    }
    ASSERT_TRUE(autofill_manager().WaitForSuggestionsShown(1));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillShowGmailOtpSuggestions};
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Tests that a Gmail OTP whose sender domain exactly matches the page's origin
// (e.g., `otp@example.test` on `https://example.test`) is accepted and
// suggested by Autofill.
IN_PROC_BROWSER_TEST_F(OtpManagerGmailOtpBrowserTest,
                       GmailOtpSenderDomainMatch_ExactMatch) {
  NavigateAndRequestGmailOtpSuggestions("otp@example.test");

  const std::vector<Suggestion>& suggestions =
      autofill_manager().external_delegate().suggestions();
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_EQ(suggestions[0].main_text.value, u"123456");
  EXPECT_THAT(suggestions[0].labels,
              ElementsAre(ElementsAre(Suggestion::Text(u"otp@example.test"))));
}

// Tests that a Gmail OTP from a sender domain matching the page's origin via
// the Public Suffix List (e.g., `otp@mail.example.test` on
// `https://example.test`) is accepted and suggested by Autofill.
IN_PROC_BROWSER_TEST_F(OtpManagerGmailOtpBrowserTest,
                       GmailOtpSenderDomainMatch_PslMatch) {
  NavigateAndRequestGmailOtpSuggestions("otp@mail.example.test");

  const std::vector<Suggestion>& suggestions =
      autofill_manager().external_delegate().suggestions();
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kGmailOneTimePasswordEntry);
  EXPECT_EQ(suggestions[0].main_text.value, u"123456");
  EXPECT_THAT(
      suggestions[0].labels,
      ElementsAre(ElementsAre(Suggestion::Text(u"otp@mail.example.test"))));
}

// Tests that a Gmail OTP from an unrelated sender domain (e.g.,
// `otp@other.test` on `https://example.test`) is rejected and not suggested by
// Autofill.
IN_PROC_BROWSER_TEST_F(OtpManagerGmailOtpBrowserTest,
                       GmailOtpSenderDomainMatch_NoMatch) {
  NavigateAndRequestGmailOtpSuggestions("otp@other.test");

  const std::vector<Suggestion>& suggestions =
      autofill_manager().external_delegate().suggestions();
  EXPECT_TRUE(suggestions.empty());
}

}  // namespace autofill
