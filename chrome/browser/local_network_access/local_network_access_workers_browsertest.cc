// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_feature_histogram_tester.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/policy/policy_constants.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS)
#include "base/path_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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
    "/local_network_access/request-from-service-worker-as-public-address.html";

class LocalNetworkAccessWorkersBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kServiceWorkerWindowClientInitiator};
};

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
      network::features::kLocalNetworkAccessChecksWebTransport,
  };
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
  base::ListValue blocklist;
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
  base::ListValue allowlist;
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

// Regression tests for crbug.com/454162508
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
                       ServiceWorkerWindowClientNavigateFail) {
  // Because the navigate happens with the window client as the initiator, a
  // permission prompt is triggered. Have the permission prompt deny the
  // permission.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  GURL initial_url = https_public_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  GURL nav_url = https_server().GetURL("c.com", kLnaPath);
  GURL iframe_url = https_public_server().GetURL(
      "b.com", std::string(kServiceWorkerHtmlPath) + "?url=" + nav_url.spec() +
                   "&method=navigate");

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

  // Fail navigation through windowclient.navigate
  ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_url_nav_manager.was_successful());
}

// Regression tests for crbug.com/454162508
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
                       ServiceWorkerWindowClientNavigateSuccess) {
  // Because the navigate happens with the window client as the initiator, a
  // permission prompt is triggered. Have the permission prompt accept the
  // permission.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  GURL initial_url = https_public_server().GetURL(
      "a.com", "/local_network_access/no-favicon-treat-as-public-address.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  GURL nav_url = https_server().GetURL("c.com", kLnaPath);
  GURL iframe_url = https_public_server().GetURL(
      "b.com", std::string(kServiceWorkerHtmlPath) + "?url=" + nav_url.spec() +
                   "&method=navigate");

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

  // Navigation through windowclient.navigate should succeed.
  ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_url_nav_manager.was_successful());
}

// Regression tests for crbug.com/454162508
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessWorkersBrowserTest,
                       ServiceWorkerWindowClientNavigateMainFrame) {
  // Permission prompt shouldn't be triggered since this is a main frame
  // navigation. Reject all permissions in case we do get a permission prompt so
  // test fails quickly.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  GURL nav_url = https_server().GetURL("c.com", kLnaPath);
  GURL initial_url = https_public_server().GetURL(
      "b.com", std::string(kServiceWorkerHtmlPath) + "?url=" + nav_url.spec() +
                   "&method=navigate");
  content::TestNavigationManager nav_url_nav_manager(web_contents(), nav_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  // Main frame navigation through windowclient.navigate should succeed.
  ASSERT_TRUE(nav_url_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_url_nav_manager.was_successful());
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
  base::ListValue allowlist;
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

// ChromeApps are only enabled on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
class ChromeAppLocalNetworkAccessWorkersBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  void LaunchPlatformApp(const extensions::Extension* extension) {
    apps::AppServiceProxyFactory::GetForProfile(GetProfile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
            extension->id(), apps::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));
  }

  // Install and launch ChromeApp with main html page taken from
  // local_network_access test sources specified by `app_html_name` which will
  // launch worker with `worker_js_name` from the same dir.
  content::WebContents* InstallAndLaunchChromeApp(std::string app_js_name,
                                                  std::string worker_js_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ExtensionTestMessageListener listener_launched("Launched");

    extensions::ChromeTestExtensionLoader extension_loader(GetProfile());
    extension_loader.set_pack_extension(false);
    base::FilePath test_data_path;
    CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
    test_data_path = test_data_path.Append("local_network_access");

    extension_dir_.WriteManifest(R"({
                  "name": "Fetch in worker LNA test",
                  "version": "1.0",
                  "manifest_version": 2,
                  "app": {
                    "background": {
                      "scripts": ["background.js"]
                    }
                  }
                })");

    extension_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
                  chrome.app.runtime.onLaunched.addListener(function() {
                    chrome.app.window.create('app.html', {});
                  });
                )");

    extension_dir_.WriteFile(FILE_PATH_LITERAL("app.html"), R"(
              <!doctype html>
              <script src="app.js"></script>
              <script src="chrome_app.js"></script>
            )");

    extension_dir_.WriteFile(FILE_PATH_LITERAL("chrome_app.js"), R"(
          onload = function() {
            chrome.test.sendMessage('Launched');
          }
        )");

    std::string contents;
    base::ReadFileToString(test_data_path.Append(app_js_name), &contents);
    extension_dir_.WriteFile(FILE_PATH_LITERAL("app.js"), std::move(contents));

    base::FilePath worker_file_path = test_data_path.Append(worker_js_name);
    base::ReadFileToString(worker_file_path, &contents);
    extension_dir_.WriteFile(worker_file_path.BaseName().value(),
                             std::move(contents));

    scoped_refptr<const extensions::Extension> extension =
        extension_loader.LoadExtension(extension_dir_.UnpackedPath());
    CHECK(extension);

    apps::chrome_app_deprecation::ScopedAddAppToAllowlistForTesting allowlist(
        extension->id());

    LaunchPlatformApp(extension.get());
    CHECK(listener_launched.WaitUntilSatisfied());

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    extensions::AppWindowRegistry* app_registry =
        extensions::AppWindowRegistry::Get(browser()->profile());

    extensions::AppWindow* window =
        app_registry->GetCurrentAppWindowForApp(extension->id());
    CHECK(window);

    return window->web_contents();
  }

 private:
  extensions::TestExtensionDir extension_dir_;
};

// Test that Dedicated Worker inside ChromeApp can successfully do fetch
// to a private network without a permission prompt, because
// ChromeApp is considered loopback which is more restrictive then private.
IN_PROC_BROWSER_TEST_F(ChromeAppLocalNetworkAccessWorkersBrowserTest,
                       DedicatedWorkerFetchWorks) {
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);

  content::WebContents* web_contents = InstallAndLaunchChromeApp(
      "request-from-worker-as-public-address-page.js",
      "request-from-worker-as-public-address-worker.js");

  std::string_view script_template = "fetch_from_worker($1);";
  ASSERT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents,
                            content::JsReplace(script_template, fetch_url)));
}

// Test that Shared Worker inside ChromeApp can successfully do fetch
// to a private network without a permission prompt, because
// ChromeApp is considered loopback which is more restrictive then private.
IN_PROC_BROWSER_TEST_F(ChromeAppLocalNetworkAccessWorkersBrowserTest,
                       SharedWorkerFetchWorks) {
  GURL fetch_url = https_server().GetURL("b.com", kLnaPath);

  content::WebContents* web_contents = InstallAndLaunchChromeApp(
      "fetch-from-shared-worker-as-public-address-page.js",
      "fetch-from-shared-worker-as-public-address-worker.js");

  std::string_view script_template = "fetch_from_shared_worker($1);";
  ASSERT_EQ("Access-Control-Allow-Origin: *",
            content::EvalJs(web_contents,
                            content::JsReplace(script_template, fetch_url)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace local_network_access
