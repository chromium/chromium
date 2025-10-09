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
#include "content/public/test/local_network_access_util.h"
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

// TODO(crbug.com/406991278): refactor to use LocalNetworkAccessBrowserTestBase
class LocalNetworkAccessBrowserTest : public policy::PolicyTest {
 public:
  using WebFeature = blink::mojom::WebFeature;

  LocalNetworkAccessBrowserTest()
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
    // The public key used to verify test trial tokens that are used in
    // content::DeprecationTrialURLLoaderInterceptor. See
    // docs/origin_trials_integration.md
    constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);

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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, FetchDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, FetchAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should succeed.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

// Tests that a script tag that is included in the main page HTML (and thus
// load blocking) correctly triggers the LNA permission prompt.
// Regression test for crbug.com/439876402.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       HtmlScriptSrcAllowPermission) {
  auto https_server = net::test_server::EmbeddedTestServer(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"public.test", "local.test"});

  // Set up repsonses for the public HTML (using CSP to force the document to be
  // treated as public) and the local script resource.
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().GetPath() == "/html") {
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("text/html");
          http_response->AddCustomHeader("Content-Security-Policy",
                                         "treat-as-public-address");
          http_response->set_content(content::JsReplace(
              "<html><head><script src=$1 defer></script></head></html>",
              request.GetURL().GetQuery()));
          return std::move(http_response);
        }
        if (request.GetURL().GetPath() == "/script") {
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("text/javascript");
          http_response->set_content(
              "console.log('local-network-access success');");
          return std::move(http_response);
        }
        return nullptr;
      }));
  ASSERT_TRUE(https_server.Start());

  // Local script URL
  GURL script_url = https_server.GetURL("local.test", "/script");

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Navigate to the public site, which will embed a <script> tag to the local
  // URL. Wait for the expected console.log() call.
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("local-network-access success");
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server.GetURL("public.test", "/html?" + script_url.spec())));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       DedicatedWorkerDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_worker($1);";
  // Failure to fetch URL
  EXPECT_EQ("TypeError: Failed to fetch",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinDedicatedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       DedicatedWorkerAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_worker($1);";
  // URL fetched, body is just the header that's set.
  EXPECT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinDedicatedWorker, 1);
}

// TODO(crbug.com/406991278): Adding counters for LNA accesses within workers in
// third_party/blink/renderer/core/loader/resource_load_observer_for_worker.cc
// works for shared and dedicated workers, but operates oddly for service
// workers:
//
// * It counts the initial load of the service worker JS file
// * It doesn't count LNA requests without permission
// * It does count LNA request with permission (the AllowPermission test below)
// * Trying to check the count via CheckCounter() or WebFeatureHistogramTester
//   does not work.
//
// Figure out how to add use counters for service worker fetches.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ServiceWorkerNoPermissionSet) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kServiceWorkerHtmlPath)));

  // Enable auto-accept of LNA permission requests (which shouldn't be checked).
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  EXPECT_EQ("ready", content::EvalJs(web_contents(), "setup();"));
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_service_worker($1);";
  // Failure to fetch URL, as for service workers the permission is only
  // checked; if its not present we don't pop up a permission prompt.
  //
  // See the comment in
  // StoragePartitionImpl::OnLocalNetworkAccessPermissionRequired for
  // Context::kServiceWorker for more context.
  EXPECT_EQ("TypeError: Failed to fetch",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ServiceWorkerDenyPermission) {
  // Use enterprise policy to block LNA requests
  policy::PolicyMap policies;
  base::Value::List blocklist;
  blocklist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessBlockedForUrls,
            base::Value(std::move(blocklist)));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kServiceWorkerHtmlPath)));

  EXPECT_EQ("ready", content::EvalJs(web_contents(), "setup();"));
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_service_worker($1);";
  // Failure to fetch URL.
  EXPECT_EQ("TypeError: Failed to fetch",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       ServiceWorkerAllowPermission) {
  // Use enterprise policy to allow LNA requests
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       SharedWorkerDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kSharedWorkerHtmlPath)));

  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);
  std::string_view script_template = "fetch_from_shared_worker($1);";
  // Failure to fetch URL
  EXPECT_EQ("TypeError: Failed to fetch",
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template, fetch_url)));
  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinSharedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       SharedWorkerAcceptPermission) {
  // Use enterprise policy to allow LNA requests
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckEnterprisePolicyEnableLNA) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kLocalNetworkAccessRestrictionsEnabled,
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

  // Expect LNA fetch to fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounter) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

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
          "/local_network_access/no-favicon-treat-as-public-address.html")));

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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkAccessAllowedForUrlsPolicy) {
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       LocalNetworkAccessBlockedForUrlsPolicy) {
  // Set both policies. Block should override Allow
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  base::Value::List blocklist;
  blocklist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessBlockedForUrls,
            base::Value(std::move(blocklist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // Enable auto-accept of LNA permission request, although it should not be
  // checked.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}

// Test that using the LNA allow policy override on an HTTP url works in
// conjuction with setting the kUnsafelyTreatInsecureOriginAsSecure command line
// switch.
class LocalNetworkAccessBrowserHttpCommandLineOverrideTest
    : public LocalNetworkAccessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) final {
    LocalNetworkAccessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        embedded_test_server()->GetURL("a.com", "/").spec());
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserHttpCommandLineOverrideTest,
                       LocalNetworkAccessAllowedForHttpUrlsPolicy) {
  policy::PolicyMap policies;
  base::Value::List allowlist;
  allowlist.Append(base::Value("*"));
  SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

// Test that using the LNA allow policy override on an HTTP url works in
// conjunction with setting the kOverrideSecurityRestrictionsOnInsecureOrigin
// enterprise policy.
class LocalNetworkAccessBrowserHttpPolicyOverrideTest
    : public LocalNetworkAccessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    LocalNetworkAccessBrowserTest::SetUpInProcessBrowserTestFixture();

    policy::PolicyMap policies;
    base::Value::List secureList;
    secureList.Append(
        base::Value(embedded_test_server()->GetURL("a.com", "/").spec()));
    SetPolicy(&policies,
              policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
              base::Value(std::move(secureList)));
    base::Value::List allowlist;
    allowlist.Append(base::Value("*"));
    SetPolicy(&policies, policy::key::kLocalNetworkAccessAllowedForUrls,
              base::Value(std::move(allowlist)));
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserHttpPolicyOverrideTest,
                       LocalNetworkAccessAllowedForHttpUrlsPolicy) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass.
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

// *****************************
// * Deprecation trial testing *
// *****************************

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       DeprecationTrialAllowsForLNAOnNonSecureSite) {
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, DeprecationTrialIframe) {
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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
