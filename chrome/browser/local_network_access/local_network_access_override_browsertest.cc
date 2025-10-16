// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/embedder_support/switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

constexpr char kWorkerHtmlPath[] =
    "/local_network_access/fetch-from-worker-as-public-address.html";

constexpr char kSharedWorkerHtmlPath[] =
    "/local_network_access/fetch-from-shared-worker-as-public-address.html";

constexpr char kServiceWorkerHtmlPath[] =
    "/local_network_access/fetch-from-service-worker-as-public-address.html";

// TODO(crbug.com/452389539): Test class is a copy of
// local_network_access_browsertest.cc to help make merging this CL into M142
// easier. This copy-paste should be undone after M142 is fixed.
//
// TODO(crbug.com/406991278): refactor to use LocalNetworkAccessBrowserTestBase
class LocalNetworkAccessOverrideBrowserTest : public policy::PolicyTest {
 public:
  using WebFeature = blink::mojom::WebFeature;

  LocalNetworkAccessOverrideBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Some builders run with field_trial disabled, need to enable this
    // manually.
    base::FieldTrialParams params;
    params["LocalNetworkAccessChecksWarn"] = "false";
    features_.InitAndEnableFeatureWithParameters(
        network::features::kLocalNetworkAccessChecks, params);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  // Fetch the Blink.UseCounter.Features histogram in every renderer process
  // until reaching, but not exceeding, |expected_count|.
  void CheckCounter(WebFeature feature, int expected_count) {
    CheckHistogramCount("Blink.UseCounter.Features", feature, expected_count);
  }

  // Fetch the |histogram|'s |bucket| in every renderer process until reaching,
  // but not exceeding, |expected_count|.
  template <typename T>
  void CheckHistogramCount(std::string_view histogram,
                           T bucket,
                           int expected_count) {
    while (true) {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      int count = histogram_.GetBucketCount(histogram, bucket);
      CHECK_LE(count, expected_count);
      if (count == expected_count) {
        return;
      }

      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
      run_loop.Run();
    }
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  permissions::MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

 protected:
  void SetUpOnMainThread() override {
    permissions::PermissionRequestManager* manager =
        GetPermissionRequestManager();
    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ignore cert errors when connecting to https_server()
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // Clear default from InProcessBrowserTest as test doesn't want 127.0.0.1 in
    // the public address space
    command_line->AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                    "");

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessOverrideBrowserTest,
                       DedicatedWorkerOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_worker($1);";
  // URL fetched, body is just the header that's set.
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinDedicatedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessOverrideBrowserTest,
                       ServiceWorkerOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kServiceWorkerHtmlPath)));

  EXPECT_EQ("ready", content::EvalJs(web_contents(), "setup();"));
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_service_worker($1);";
  // Fetched URL
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessOverrideBrowserTest,
                       SharedWorkerOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kSharedWorkerHtmlPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_shared_worker($1);";
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinSharedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessOverrideBrowserTest,
                       CheckEnterprisePolicyOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Expect LNA fetch to succeed.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessOverrideBrowserTest, IframeNavOptOut) {
  policy::PolicyMap policies;
  SetPolicy(&policies,
            policy::key::kLocalNetworkAccessRestrictionsTemporaryOptOut,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  GURL initial_url =
      https_server().GetURL("a.com", "/local_network_access/no-favicon.html");
  GURL nav_url = https_server().GetURL("c.com", "/defaultresponse");
  GURL iframe_url = https_server().GetURL(
      "b.com",
      "/local_network_access/"
      "client-redirect-treat-as-public-address.html?url=" +
          nav_url.spec());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  content::TestNavigationManager iframe_url_nav_manager(web_contents(),
                                                        iframe_url);
  content::TestNavigationManager nav_url_nav_manager(web_contents(), nav_url);
  std::string_view script_template = R"(
    const child = document.createElement("iframe");
    child.src = $1;
    child.allow = "local-network-access";
    document.body.appendChild(child);
  )";
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(script_template, iframe_url)),
              content::EvalJsResult::IsOk());
  // Check that the child iframe was successfully fetched.
  ASSERT_TRUE(iframe_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(iframe_url_nav_manager.was_successful());

  ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_url_nav_manager.was_successful());
}
