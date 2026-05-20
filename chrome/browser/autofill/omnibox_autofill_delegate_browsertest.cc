// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

using autofill_metrics::OmniboxAutofillShowChipDecisionPart1;
using testing::_;
using testing::Eq;
using testing::Return;

class OmniboxAutofillDelegateBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxAutofillDelegateBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }
  ~OmniboxAutofillDelegateBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set up the HTTPS server.
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    embedded_https_test_server().SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&OmniboxAutofillDelegateBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    embedded_https_test_server().StartAcceptingConnections();

    // Add a credit card to the system.
    PersonalDataManager& pdm = autofill_client().GetPersonalDataManager();
    pdm.payments_data_manager().AddCreditCard(test::GetCreditCard());
  }

  void SetUrlContent(const std::string& relative_path,
                     const std::string& content_html) {
    pages_[relative_path] = content_html;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestContentAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  MockAutofillOptimizationGuideDecider& optimization_guide_decider() {
    return *static_cast<MockAutofillOptimizationGuideDecider*>(
        autofill_client().GetAutofillOptimizationGuideDecider());
  }

 protected:
  net::EmbeddedTestServer& embedded_https_test_server() {
    return https_server_;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto it = pages_.find(request.GetURL().GetPath());
    if (it == pages_.end()) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html;charset=utf-8");
    response->set_content(it->second);
    return response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::map<std::string, std::string> pages_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  content::ContentMockCertVerifier cert_verifier_;
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Checks that Omnibox Autofill is allowed when all form fields are on the
// page's main frame (i.e., not in an iframe).
IN_PROC_BROWSER_TEST_F(OmniboxAutofillDelegateBrowserTest,
                       FieldsInMainFrameOnly_Succeeds) {
  base::HistogramTester histogram_tester;

  SetUrlContent("/form.html", R"(
    <form>
      <input autocomplete="cc-name">
      <input autocomplete="cc-number">
      <input autocomplete="cc-exp">
      <input type="submit">
    </form>
  )");

  GURL url = embedded_https_test_server().GetURL("a.com", "/form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* rfh = web_contents()->GetPrimaryMainFrame();
  auto* driver = ContentAutofillDriver::GetForRenderFrameHost(rfh);
  auto& manager = driver->GetAutofillManager();

  const FormStructure* form = WaitForMatchingForm(
      &manager, base::BindRepeating([](const FormStructure& form) {
        return form.field_count() == 3;
      }));
  ASSERT_TRUE(form);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
               OmniboxAutofillShowChipDecisionPart1::kSuccess) == 1;
  }));
}

// Checks that Omnibox Autofill is allowed when form fields are contained within
// an iframe, as long as that iframe is allowlisted.
IN_PROC_BROWSER_TEST_F(OmniboxAutofillDelegateBrowserTest,
                       FieldsInAllowlistedIframe_Succeeds) {
  base::HistogramTester histogram_tester;

  SetUrlContent("/iframe.html", R"(<input autocomplete="cc-exp">)");

  std::string main_content = base::StringPrintf(
      R"(<form>
           <input autocomplete="cc-name">
           <input autocomplete="cc-number">
           <iframe src="%s"></iframe>
         </form>)",
      embedded_https_test_server()
          .GetURL("b.com", "/iframe.html")
          .spec()
          .c_str());

  SetUrlContent("/form.html", main_content);

  // Allowlist b.com.
  GURL iframe_url =
      embedded_https_test_server().GetURL("b.com", "/iframe.html");
  GURL iframe_origin_url = url::Origin::Create(iframe_url).GetURL();
  EXPECT_CALL(optimization_guide_decider(),
              IsUrlEligibleForOmniboxAutofill(Eq(iframe_origin_url)))
      .WillRepeatedly(Return(true));

  GURL url = embedded_https_test_server().GetURL("a.com", "/form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* rfh = web_contents()->GetPrimaryMainFrame();
  auto* driver = ContentAutofillDriver::GetForRenderFrameHost(rfh);
  auto& manager = driver->GetAutofillManager();

  const FormStructure* form = WaitForMatchingForm(
      &manager, base::BindRepeating([](const FormStructure& form) {
        return form.field_count() == 3;
      }));
  ASSERT_TRUE(form);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
               OmniboxAutofillShowChipDecisionPart1::kSuccess) == 1;
  }));
}

// Checks that Omnibox Autofill is *not* allowed when form fields are contained
// within an iframe, and that iframe is *not* allowlisted.
IN_PROC_BROWSER_TEST_F(OmniboxAutofillDelegateBrowserTest,
                       FieldInNonAllowlistedIframe_Aborts) {
  base::HistogramTester histogram_tester;

  SetUrlContent("/iframe_allowlisted.html",
                R"(<input autocomplete="cc-name">)");
  SetUrlContent("/iframe_non_allowlisted.html",
                R"(<input autocomplete="cc-number">)");

  std::string main_content = base::StringPrintf(
      R"(<form>
           <iframe src="%s"></iframe>
           <iframe src="%s"></iframe>
           <input autocomplete="cc-exp">
         </form>)",
      embedded_https_test_server()
          .GetURL("b.com", "/iframe_allowlisted.html")
          .spec()
          .c_str(),
      embedded_https_test_server()
          .GetURL("c.com", "/iframe_non_allowlisted.html")
          .spec()
          .c_str());

  SetUrlContent("/form.html", main_content);

  // Allowlist b.com.
  GURL iframe_allowlisted_url =
      embedded_https_test_server().GetURL("b.com", "/iframe_allowlisted.html");
  GURL iframe_allowlisted_origin_url =
      url::Origin::Create(iframe_allowlisted_url).GetURL();
  EXPECT_CALL(
      optimization_guide_decider(),
      IsUrlEligibleForOmniboxAutofill(Eq(iframe_allowlisted_origin_url)))
      .WillRepeatedly(Return(true));

  // Do NOT allowlist c.com.
  GURL iframe_non_allowlisted_url = embedded_https_test_server().GetURL(
      "c.com", "/iframe_non_allowlisted.html");
  GURL iframe_non_allowlisted_origin_url =
      url::Origin::Create(iframe_non_allowlisted_url).GetURL();
  EXPECT_CALL(
      optimization_guide_decider(),
      IsUrlEligibleForOmniboxAutofill(Eq(iframe_non_allowlisted_origin_url)))
      .WillRepeatedly(Return(false));

  GURL url = embedded_https_test_server().GetURL("a.com", "/form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* rfh = web_contents()->GetPrimaryMainFrame();
  auto* driver = ContentAutofillDriver::GetForRenderFrameHost(rfh);
  auto& manager = driver->GetAutofillManager();

  const FormStructure* form = WaitForMatchingForm(
      &manager, base::BindRepeating([](const FormStructure& form) {
        return form.field_count() == 3;
      }));
  ASSERT_TRUE(form);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
               OmniboxAutofillShowChipDecisionPart1::kNonAllowlistedIframe) ==
           1;
  }));
}

}  // namespace autofill
