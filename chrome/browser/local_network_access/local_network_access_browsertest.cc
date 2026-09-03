// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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
#include "services/network/public/cpp/ip_address_space_overrides_test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Local Network Access browser tests that don't fit into the other files.

namespace local_network_access {

namespace {
// We use a custom page that explicitly disables its own favicon (by providing
// an invalid data: URL for it) so as to prevent the browser from making an
// automatic request to /favicon.ico.
constexpr char kNoFaviconPath[] = "/local_network_access/no-favicon.html";

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

class LocalNetworkAccessBrowserTest : public LocalNetworkAccessBrowserTestBase {
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, FetchDenyPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_public_server().GetURL("a.com", kNoFaviconPath)));

  // Enable auto-denial of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // LNA fetch should fail.
  EXPECT_FALSE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1).then(response => response.ok)",
                         https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, FetchAcceptPermission) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_public_server().GetURL("a.com", kNoFaviconPath)));

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

class LocalNetworkAccessHtmlScriptBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  LocalNetworkAccessHtmlScriptBrowserTest() = default;
  ~LocalNetworkAccessHtmlScriptBrowserTest() override = default;

  void SetUp() override {
    public_server_.SetCertHostnames({"public.test"});
    public_server_.AddDefaultHandlers(GetChromeTestDataDir());
    public_server_.RegisterRequestHandler(base::BindRepeating(
        &LocalNetworkAccessHtmlScriptBrowserTest::HandlePublicHtmlRequest,
        base::Unretained(this)));

    local_server_.SetCertHostnames({"local.test"});
    local_server_.AddDefaultHandlers(GetChromeTestDataDir());
    local_server_.RegisterRequestHandler(base::BindRepeating(
        &LocalNetworkAccessHtmlScriptBrowserTest::HandleLocalScriptRequest,
        base::Unretained(this)));

    LocalNetworkAccessBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);

    ASSERT_TRUE(public_server_.Start());
    ASSERT_TRUE(local_server_.Start());
    network::AddIpAddressSpaceOverridesToCommandLine(
        {network::GenerateIpAddressSpaceOverride(
             local_server(), network::mojom::IPAddressSpace::kLocal),
         network::GenerateIpAddressSpaceOverride(
             public_server(), network::mojom::IPAddressSpace::kPublic)},
        *command_line);
  }

  net::EmbeddedTestServer& public_server() { return public_server_; }
  net::EmbeddedTestServer& local_server() { return local_server_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandlePublicHtmlRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() == "/html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content_type("text/html");
      http_response->set_content(content::JsReplace(
          "<html><head><script src=$1 defer></script></head></html>",
          request.GetURL().GetQuery()));
      return std::move(http_response);
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleLocalScriptRequest(
      const net::test_server::HttpRequest& request) {
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
  }

  net::EmbeddedTestServer public_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer local_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Tests that a script tag that is included in the main page HTML (and thus
// load blocking) correctly triggers the LNA permission prompt.
// Regression test for crbug.com/439876402.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessHtmlScriptBrowserTest,
                       HtmlScriptSrcAllowPermission) {
  // Local script URL
  GURL script_url = local_server().GetURL("local.test", "/script");

  // Enable auto-accept of LNA permission request.
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // Navigate to the public site, which will embed a <script> tag to the local
  // URL. Wait for the expected console.log() call.
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("local-network-access success");
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(),
      public_server().GetURL("public.test", "/html?" + script_url.spec())));
  EXPECT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounter) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_public_server().GetURL("a.com", kNoFaviconPath)));

  // LNA fetch fails due to mismatched targetAddressSpace. Result doesn't matter
  // here though, as we're just checking a use counter that doesn't depend on
  // fetch success.
  EXPECT_FALSE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1, {targetAddressSpace: "
                         "'private'}).then(response => response.ok)",
                         https_server().GetURL("b.com", kLnaPath))));

  CheckCounter(WebFeature::kLocalNetworkAccessPrivateAliasUse, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckPrivateAliasFeatureCounterLocalNotCounted) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), https_public_server().GetURL("a.com", kNoFaviconPath)));

  // LNA fetch fails due to mismatched targetAddressSpace. Result doesn't matter
  // here though, as we're just checking a use counter that doesn't depend on
  // fetch success.
  EXPECT_FALSE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1, {targetAddressSpace: "
                         "'local'}).then(response => response.ok)",
                         https_server().GetURL("b.com", kLnaPath))));

  CheckCounter(WebFeature::kLocalNetworkAccessPrivateAliasUse, 0);
}

// ================
// 0.0.0.0 TESTS
// ================

class LocalNetworkAccessNullIPBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  net::EmbeddedTestServer& public_server() { return public_server_; }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    public_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(public_server_.Start());
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);
    network::AddIpAddressSpaceOverridesToCommandLine(
        {network::GenerateIpAddressSpaceOverride(
             https_local_server(), network::mojom::IPAddressSpace::kLocal),
         network::GenerateIpAddressSpaceOverride(
             https_public_server(), network::mojom::IPAddressSpace::kPublic),
         network::GenerateIpAddressSpaceOverride(
             public_server_, network::mojom::IPAddressSpace::kPublic)},
        *command_line);
  }

 private:
  net::EmbeddedTestServer public_server_{net::EmbeddedTestServer::TYPE_HTTP};
};

// This test verifies that a 0.0.0.0 subresource is blocked on a nonsecure
// public URL.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessNullIPBrowserTest,
                       NullIPBlockedOnNonsecure) {
  if constexpr (BUILDFLAG(IS_WIN)) {
    GTEST_SKIP() << "0.0.0.0 behavior varies across platforms and is "
                    "unreachable on Windows.";
  }

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), public_server().GetURL("a.com", kNoFaviconPath)));
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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest, SpecialSchemeDevtools) {
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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
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
  extensions::ExtensionRegistrar::Get(browser()->GetProfile())
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
//
// These tests need to change IP address spaces using command line overrides,
// which require a browser restart. Port numbers are kept the same across
// the restarts because the cache is partitioned by top-level origin.

class LocalNetworkAccessCachedResourceBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  LocalNetworkAccessCachedResourceBrowserTest() = default;
  ~LocalNetworkAccessCachedResourceBrowserTest() override = default;

  void SetUp() override {
    cached_resource_public_server_.SetCertHostnames({"a.com"});
    cached_resource_public_server_.AddDefaultHandlers(GetChromeTestDataDir());

    cached_resource_loopback_server_.SetCertHostnames({"b.com"});
    cached_resource_loopback_server_.AddDefaultHandlers(GetChromeTestDataDir());
    cached_resource_loopback_server_.RegisterRequestHandler(base::BindRepeating(
        &LocalNetworkAccessCachedResourceBrowserTest::HandleCacheableRequest,
        base::Unretained(this)));

    LocalNetworkAccessBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LocalNetworkAccessBrowserTestBase::SetUpCommandLine(command_line);

    base::FilePath user_data_dir =
        command_line->GetSwitchValuePath("user-data-dir");
    ASSERT_FALSE(user_data_dir.empty());
    base::FilePath public_port_file =
        user_data_dir.AppendASCII("cached_resource_public_server_port.txt");
    base::FilePath loopback_port_file =
        user_data_dir.AppendASCII("cached_resource_loopback_server_port.txt");

    int public_port = 0;
    int loopback_port = 0;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    bool is_pre_test = base::StartsWith(test_info->name(), "PRE_");

    if (!is_pre_test) {
      std::string port_str;
      if (base::ReadFileToString(public_port_file, &port_str)) {
        base::StringToInt(port_str, &public_port);
      }
      if (base::ReadFileToString(loopback_port_file, &port_str)) {
        base::StringToInt(port_str, &loopback_port);
      }
    }

    ASSERT_TRUE(
        cached_resource_public_server_.InitializeAndListen(public_port));
    ASSERT_TRUE(
        cached_resource_loopback_server_.InitializeAndListen(loopback_port));

    if (is_pre_test) {
      std::string public_port_str =
          base::NumberToString(cached_resource_public_server_.port());
      ASSERT_TRUE(base::WriteFile(public_port_file, public_port_str));

      std::string loopback_port_str =
          base::NumberToString(cached_resource_loopback_server_.port());
      ASSERT_TRUE(base::WriteFile(loopback_port_file, loopback_port_str));
    }

    if (!is_pre_test) {
      network::AddPublicIpAddressSpaceOverrideToCommandLine(
          cached_resource_public_server_, *command_line);
    }

    cached_resource_public_server_.StartAcceptingConnections();
    cached_resource_loopback_server_.StartAcceptingConnections();
  }

  int request_count() const { return request_count_; }
  net::EmbeddedTestServer& cached_resource_public_server() {
    return cached_resource_public_server_;
  }
  net::EmbeddedTestServer& cached_resource_loopback_server() {
    return cached_resource_loopback_server_;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleCacheableRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() == "/cacheable") {
      request_count_++;
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
  }

  net::EmbeddedTestServer cached_resource_public_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer cached_resource_loopback_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  int request_count_ = 0;
};

// Tests that resources:
//   - which were cached from loopback addresses,
//   - then loaded from cache in contexts where LNA would apply,
//   - but the requesting origin does not have the LNA permission granted yet.
// get retried over the network.
//
// See also the test `CachedResourceIsLoadedFromCache` below.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCachedResourceBrowserTest,
                       PRE_CachedResourceIsLoadedFromNetwork) {
  GURL target_url =
      cached_resource_loopback_server().GetURL("b.com", "/cacheable");

  // First, navigate to a loopback page on a.com and fetch loopback resource
  // from b.com to get it in the cache.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      cached_resource_loopback_server().GetURL("a.com", kNoFaviconPath)));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // No permission prompt should be shown as this was a loopback -> loopback
  // connection.
  EXPECT_EQ(1, request_count());
  EXPECT_EQ(0, bubble_factory()->show_count());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCachedResourceBrowserTest,
                       CachedResourceIsLoadedFromNetwork) {
  GURL target_url =
      cached_resource_loopback_server().GetURL("b.com", "/cacheable");

  // Now, navigate to the same page but it's now considered public and try to
  // fetch the same resource.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      cached_resource_public_server().GetURL("a.com", kNoFaviconPath)));

  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // The resource should have been re-fetched, rather than loaded from cache.
  // The prompt will trigger because the subresource is in the `loopback`
  // address space and the top-level page is now in `public`.
  EXPECT_EQ(1, request_count());
  EXPECT_EQ(1, bubble_factory()->request_count());
}

// Tests that resources:
//   - which were cached from loopback addresses,
//   - then loaded from cache in contexts where LNA would apply,
//   - but the requesting origin has the LNA permission granted
// *don't* get retried over the network and are loaded from cache.
//
// This is a counterpart to the test `CachedResourceIsLoadedFromNetwork` above.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCachedResourceBrowserTest,
                       PRE_CachedResourceIsLoadedFromCache) {
  GURL target_url =
      cached_resource_loopback_server().GetURL("b.com", "/cacheable");

  // Set the LNA permission for a.com to "Allowed".
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          chrome_test_utils::GetProfile(this));
  host_content_settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(
          cached_resource_public_server().GetURL("a.com", "/")),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::LOOPBACK_NETWORK,
      CONTENT_SETTING_ALLOW);

  // First, navigate to a loopback page on a.com and fetch resource from b.com
  // to get it in the cache.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      cached_resource_loopback_server().GetURL("a.com", kNoFaviconPath)));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // No permission prompt should be shown as this was a loopback -> loopback
  // connection.
  EXPECT_EQ(1, request_count());
  EXPECT_EQ(0, bubble_factory()->show_count());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessCachedResourceBrowserTest,
                       CachedResourceIsLoadedFromCache) {
  GURL target_url =
      cached_resource_loopback_server().GetURL("b.com", "/cacheable");

  // Now, navigate to the same page but it's now considered public and try to
  // fetch the same resource. Check that the resource was loaded from cache
  // because a.com has been granted the LNA permission.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      cached_resource_public_server().GetURL("a.com", kNoFaviconPath)));

  ASSERT_EQ(true, content::EvalJs(web_contents(), FetchScript(target_url)));

  // The resource should have been loaded from cache, and no request should be
  // seen by the test server. No prompt will trigger because a.com is already
  // granted the LNA permission.
  EXPECT_EQ(0, request_count());
  EXPECT_EQ(0, bubble_factory()->request_count());
}

}  // namespace local_network_access
