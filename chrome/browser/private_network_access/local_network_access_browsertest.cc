// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

class LocalNetworkAccessBrowserTest : public policy::PolicyTest {
 public:
  using WebFeature = blink::mojom::WebFeature;

  LocalNetworkAccessBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Some builders run with field_trial disabled, need to  enable this
    // manually.
    features_.InitAndEnableFeature(
        network::features::kLocalNetworkAccessChecks);
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

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.Start());
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) final {
    // Ignore cert errors when connecting to https_server()
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_;
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckSecurityStateDefaultPolicy) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass (default is currently in warning mode).
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckSecurityStatePolicySet) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kLocalNetworkAccessRestrictionsEnabled,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-denial of LNA permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Expect LNA fetch to fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounter) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch fails due to mismatched targetAddressSpace. Result doesn't matter
  // here though, as we're just checking a use counter that doesn't depend on
  // fetch success.
  EXPECT_THAT(content::EvalJs(web_contents(),
                              content::JsReplace(
                                  "fetch($1, {targetAddressSpace: "
                                  "'private'}).then(response => response.ok)",
                                  https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());

  CheckCounter(WebFeature::kLocalNetworkAccessPrivateAliasUse, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounterLocalNotCounted) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch fails due to mismatched targetAddressSpace. Result doesn't matter
  // here though, as we're just checking a use counter that doesn't depend on
  // fetch success.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1, {targetAddressSpace: "
                                     "'local'}).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());

  CheckCounter(WebFeature::kLocalNetworkAccessPrivateAliasUse, 0);
}
