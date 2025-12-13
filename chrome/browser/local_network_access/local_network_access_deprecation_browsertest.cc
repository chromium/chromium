// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/embedder_support/switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/local_network_access_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Browser Tests for the Local Network Access deprecation trial (also known as a
// reverse origin trial).

namespace local_network_access {

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

class LocalNetworkAccessDeprecationBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);
    // The public key used to verify test trial tokens that are used in
    // content::DeprecationTrialURLLoaderInterceptor. See
    // docs/origin_trials_integration.md
    constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       RecordUseCounterForDeprecationTrialEnabled) {
  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  // Deprecation trial allows LNA on non-secure contexts (with permission
  // grant).
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.EnabledHttpUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);

  // Deprecation trial has no impact on secure contexts.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.EnabledHttpsUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       RecordUseCounterForDeprecationTrialDisabled) {
  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.DisabledHttpUrl()));
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.DisabledHttpsUrl()));

  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            0);
}

#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_DeprecationTrialAllowsForLNAOnNonSecureSite \
  DISABLED_DeprecationTrialAllowsForLNAOnNonSecureSite
#else
#define MAYBE_DeprecationTrialAllowsForLNAOnNonSecureSite \
  DeprecationTrialAllowsForLNAOnNonSecureSite
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       MAYBE_DeprecationTrialAllowsForLNAOnNonSecureSite) {
  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  // Deprecation trial allows LNA on non-secure contexts (with permission
  // grant).
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.EnabledHttpUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);

  // Enable auto-accept of LNA permission request
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_DeprecationTrialIframe DISABLED_DeprecationTrialIframe
#else
#define MAYBE_DeprecationTrialIframe DeprecationTrialIframe
#endif
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       MAYBE_DeprecationTrialIframe) {
  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  // Deprecation trial allows LNA on non-secure contexts (with permission
  // grant).
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), interceptor.EnabledHttpUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL iframe_url = https_server().GetURL("b.com", kLnaPath);
  content::TestNavigationManager nav_manager(web_contents(), iframe_url);
  std::string_view script_template = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    document.body.appendChild(child);
  )";
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(script_template, iframe_url)),
              content::EvalJsResult::IsOk());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  // Check that the child iframe was successfully fetched.
  EXPECT_TRUE(nav_manager.was_successful());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialDedicatedWorker) {
  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     interceptor.EnabledHttpWorkerUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_worker($1);";
  // URL fetched, body is just the header that's set.
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialSharedWorker) {
  // Use enterprise policy to allow LNA requests
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  content::DeprecationTrialURLLoaderInterceptor interceptor;
  WebFeatureHistogramTester feature_histogram_tester;

  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     interceptor.EnabledHttpSharedWorkerUrl()));
  EXPECT_EQ(feature_histogram_tester.GetCount(
                WebFeature::
                    kLocalNetworkAccessNonSecureContextAllowedDeprecationTrial),
            1);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_shared_worker($1);";
  // URL fetched, body is just the header that's set.
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinSharedWorker, 1);
}

}  // namespace local_network_access
