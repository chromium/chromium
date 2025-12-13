// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Local network access browser tests related to workers
// (dedicated/shared/service).

namespace local_network_access {

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

constexpr char kWorkerHtmlPath[] =
    "/local_network_access/request-from-worker-as-public-address.html";

constexpr char kSharedWorkerHtmlPath[] =
    "/local_network_access/fetch-from-shared-worker-as-public-address.html";

constexpr char kServiceWorkerHtmlPath[] =
    "/local_network_access/fetch-from-service-worker-as-public-address.html";

class LocalNetworkAccessWorkersBrowserTest
    : public LocalNetworkAccessBrowserTestBase {};

class LocalNetworkAccessWorkersWebTransportBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);
    server_.SetUpCommandLine(command_line);
    server_.Start();
  }

  int webtransport_port() const { return server_.server_address().port(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      network::features::kLocalNetworkAccessChecksWebTransport};
  content::WebTransportSimpleTestServer server_;
};

// Tests that a script tag that is included in the main page HTML (and thus
// load blocking) correctly triggers the LNA permission prompt.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersWebTransportBrowserTest,
                       DedicatedWorkerDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  // Enable auto-deny of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  std::string_view script_template =
      "webtransport_open_from_worker('https://localhost:$1/echo');";
  EXPECT_EQ(
      "WebTransportError: Opening handshake failed.",
      content::EvalJs(web_contents(), content::JsReplace(script_template,
                                                         webtransport_port())));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kLocalNetworkAccessWithinDedicatedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersWebTransportBrowserTest,
                       DedicatedWorkerAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_server().GetURL("a.com", kWorkerHtmlPath)));

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  std::string_view script_template =
      "webtransport_open_from_worker('https://localhost:$1/echo');";
  EXPECT_EQ(
      "webtransport opened",
      content::EvalJs(web_contents(), content::JsReplace(script_template,
                                                         webtransport_port())));
  EXPECT_EQ(
      "webtransport closed",
      content::EvalJs(web_contents(),
                      content::JsReplace("webtransport_close_from_worker()")));

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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
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

}  // namespace local_network_access
