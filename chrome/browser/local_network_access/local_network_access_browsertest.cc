// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Local Network Access browser tests that don't fit into the other files.

namespace local_network_access {

namespace {
// We use a custom page that explicitly disables its own favicon (by providing
// an invalid data: URL for it) so as to prevent the browser from making an
// automatic request to /favicon.ico.
//
// It also carries a header that makes the browser consider it came from the
// `public` address space, irrespective of the fact that we loaded the web page
// from localhost.
constexpr char kTreatAsPublicAddressPath[] =
    "/local_network_access/no-favicon-treat-as-public-address.html";

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

// The returned script evaluates to a boolean indicating whether the fetch
// succeeded or not.
std::string FetchScript(const GURL& url) {
  return content::JsReplace(
      "fetch($1).then(response => true).catch(error => false)", url);
}
}  // namespace

class LocalNetworkAccessBrowserTest : public LocalNetworkAccessBrowserTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  LocalNetworkAccessBrowserTest() : LocalNetworkAccessBrowserTestBase() {
    if (SplitPermissionsEnabled()) {
      feature_list_.InitAndEnableFeature(
          network::features::kLocalNetworkAccessChecksSplitPermissions);
    } else {
      feature_list_.InitAndDisableFeature(
          network::features::kLocalNetworkAccessChecksSplitPermissions);
    }
  }

  bool SplitPermissionsEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest, FetchDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

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

IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest, FetchAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

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
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
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

IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounter) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

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

IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounterLocalNotCounted) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

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

// ================
// 0.0.0.0 TESTS
// ================

// This test verifies that a 0.0.0.0 subresource is blocked on a nonsecure
// public URL.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       NullIPBlockedOnNonsecure) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("a.com", kTreatAsPublicAddressPath)));
  GURL subresource_url =
      embedded_test_server()->GetURL("0.0.0.0", "/cors-ok.txt");
  EXPECT_EQ(false,
            content::EvalJs(web_contents(), FetchScript(subresource_url)));
}

// ====================
// SPECIAL SCHEME TESTS
// ====================
//
// These tests verify the IP address space assigned to documents loaded from a
// variety of special URL schemes. Since these are not loaded over the network,
// an IP address space must be made up for them.

// This test verifies that the devtools:// scheme is considered loopback for the
// purpose of Local Network Access.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest, SpecialSchemeDevtools) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("devtools://devtools/bundled/devtools_app.html")));
  EXPECT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          content::kChromeDevToolsScheme));

  // DevTools has strict CSP which doesn't allow fetching from local addresses,
  // so we're using an iframe, since frame-src allows wildcards.
  GURL iframe_url = https_server().GetURL("/cors-ok.txt");
  content::TestNavigationManager nav_manager(web_contents(), iframe_url);

  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace(
                          "const iframe = document.createElement('iframe');"
                          "iframe.src = $1;"
                          "document.body.appendChild(iframe);",
                          iframe_url)));

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(nav_manager.was_successful());
}

// This test verifies that the chrome-search:// scheme is considered loopback
// for the purpose of Local Network Access.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       SpecialSchemeChromeSearch) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("chrome-search://most-visited/title.html")));
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          chrome::kChromeSearchScheme));

  GURL fetch_url = https_server().GetURL("/cors-ok.txt");

  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the chrome-extension:// scheme is considered local
// for the purpose of Local Network Access.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       SpecialSchemeChromeExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";
  constexpr char kContents[] = R"(
  <html>
    <head>
      <title>IPAddressSpace of chrome-extension:// schemes.</title>
    </head>
    <body>
    </body>
  </html>
  )";
  base::WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents);
  static constexpr char kWebAccessibleResources[] =
      R"([{
            "resources": ["page.html"],
            "matches": ["*://*/*"]
         }])";

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources",
                      base::test::ParseJson(kWebAccessibleResources));

  scoped_refptr<const extensions::Extension> extension = builder.Build();
  extensions::ExtensionRegistrar::Get(browser()->profile())
      ->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  const GURL url = extension->GetResourceURL(kPageFile);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().SchemeIs(
          extensions::kExtensionScheme));

  GURL fetch_url = https_server().GetURL("/cors-ok.txt");

  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// ====================
// CACHE RETRY TESTS
// ====================
//
// These tests verify the behavior of LNA when resources are loaded from cache.
// The remote IP address of the resource is stored in cache, causing LNA checks
// to trigger when network state changes.

// Tests that resources:
//   - which were cached from loopback addresses,
//   - then loaded from cache in contexts where LNA would apply,
//   - but the requesting origin does not have the LNA permission granted yet.
// get retried over the network.
//
// See also the test `CachedResourceIsLoadedFromCache` below.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       CachedResourceIsLoadedFromNetwork) {
  auto https_server = net::test_server::EmbeddedTestServer(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());

  int request_count = 0;
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().GetPath() == "/cacheable") {
          request_count++;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("text/plain");
          http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          http_response->AddCustomHeader("Cache-Control", "max-age=3600");
          http_response->set_content("hello");
          return std::move(http_response);
        }
        return nullptr;
      }));

  ASSERT_TRUE(https_server.Start());

  GURL target_url = https_server.GetURL("b.com", "/cacheable");

  // First, navigate to a local page on a.com and fetch resource from b.com to
  // get it in the cache.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server.GetURL("a.com", "/local_network_access/no-favicon.html")));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // No permission prompt should be shown as this was a loopback -> loopback
  // connection.
  EXPECT_EQ(1, request_count);
  EXPECT_EQ(0, bubble_factory()->show_count());

  // Now, navigate to the same page but it's now considered public and try to
  // fetch the same resource. The subresource will still be in the `loopback`
  // address space (as dynamically configuring this for subresources is
  // challenging in tests), but we can at least check that it wasn't loaded from
  // cache.
  //
  // TODO(crbug.com/40820219): Once we support enterprise policies modifying
  // the IP address space mappings, we may be able to change this test to
  // dynamically modify the mappings to make 127.0.0.1 be "public", instead of
  // relying on the CSP header, giving us a more complete test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server.GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // The resource should have been re-fetched, rather than loaded from cache.
  // The prompt will trigger because the subresource is still in the `loopback`
  // address space.
  EXPECT_EQ(2, request_count);
  EXPECT_EQ(1, bubble_factory()->request_count());
}

// Tests that resources:
//   - which were cached from loopback addresses,
//   - then loaded from cache in contexts where LNA would apply,
//   - but the requesting origin has the LNA permission granted
// *don't* get retried over the network and are loaded from cache.
//
// This is a counterpart to the test `CachedResourceIsLoadedFromNetwork` above.
IN_PROC_BROWSER_TEST_P(LocalNetworkAccessBrowserTest,
                       CachedResourceIsLoadedFromCache) {
  auto https_server = net::test_server::EmbeddedTestServer(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());

  int request_count = 0;
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().GetPath() == "/cacheable") {
          request_count++;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_OK);
          http_response->set_content_type("text/plain");
          http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          http_response->AddCustomHeader("Cache-Control", "max-age=3600");
          http_response->set_content("hello");
          return std::move(http_response);
        }
        return nullptr;
      }));

  ASSERT_TRUE(https_server.Start());

  GURL target_url = https_server.GetURL("b.com", "/cacheable");

  // Set the LNA permission for a.com to "Allowed".
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          chrome_test_utils::GetProfile(this));
  auto content_setting_type = SplitPermissionsEnabled()
                                  ? ContentSettingsType::LOOPBACK_NETWORK
                                  : ContentSettingsType::LOCAL_NETWORK_ACCESS;
  host_content_settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(https_server.GetURL("a.com", "/")),
      ContentSettingsPattern::Wildcard(), content_setting_type,
      CONTENT_SETTING_ALLOW);

  // First, navigate to a local page on a.com and fetch resource from b.com to
  // get it in the cache.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server.GetURL("a.com", "/local_network_access/no-favicon.html")));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // No permission prompt should be shown as this was a loopback -> loopback
  // connection.
  EXPECT_EQ(1, request_count);
  EXPECT_EQ(0, bubble_factory()->show_count());

  // Now, navigate to the same page but it's now considered public and try to
  // fetch the same resource. The subresource will still be in the `loopback`
  // address space (as dynamically configuring this for subresources is
  // challenging in tests), but we can check that the resource was loaded from
  // cache because a.com has been granted the LNA permission.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server.GetURL(
          "a.com",
          "/local_network_access/no-favicon-treat-as-public-address.html")));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // The resource should have been loaded from cache, and no request should be
  // seen by the test server. No prompt will trigger because a.com is already
  // granted the LNA permission.
  EXPECT_EQ(1, request_count);
  EXPECT_EQ(0, bubble_factory()->request_count());
}

INSTANTIATE_TEST_SUITE_P(All, LocalNetworkAccessBrowserTest, testing::Bool());

}  // namespace local_network_access
