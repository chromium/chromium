// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_cookies_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kActiveTabHost[] = "active-tab.example";

// CSP header to be applied to the extension and the child frames.
constexpr char kCspHeader[] =
    "script-src 'self' https://a.example:* https://sub.a.example:* "
    "https://notallowedsub.a.example:* https://b.example:* "
    "https://c.example:* https://d.example:* https://e.example; object-src "
    "'self'";

using testing::UnorderedElementsAreArray;

// Base class for special handling of cookies for extensions.
class ExtensionCookiesTest : public ExtensionBrowserTest {
 public:
  ExtensionCookiesTest() : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~ExtensionCookiesTest() override = default;
  ExtensionCookiesTest(const ExtensionCookiesTest&) = delete;
  ExtensionCookiesTest& operator=(const ExtensionCookiesTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    extension_dir_ = std::make_unique<TestExtensionDir>();
    extension_ = MakeExtension();
    helper_ = std::make_unique<ExtensionCookiesTestHelper>(
        *test_server(), *profile(), kCspHeader);
    host_resolver()->AddRule("*", "127.0.0.1");
    net::test_server::RegisterDefaultHandlers(test_server());
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_server()->ServeFilesFromDirectory(test_data_dir);
    test_server()->SetCertHostnames(
        {ExtensionCookiesTestHelper::kPermittedHost,
         ExtensionCookiesTestHelper::kOtherPermittedHost,
         ExtensionCookiesTestHelper::kNotPermittedHost,
         ExtensionCookiesTestHelper::kPermittedSubdomain,
         ExtensionCookiesTestHelper::kNotPermittedSubdomain, kActiveTabHost,
         ExtensionCookiesTestHelper::kCrossOriginHost});
    ASSERT_TRUE(test_server()->Start());
  }

  // The helper holds raw references to the test server and `Profile`. Reset
  // it here so those references are released before the `Profile` is torn
  // down.
  void TearDownOnMainThread() override {
    helper_.reset();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Navigates to the extension page in the main frame. Returns a pointer to the
  // RenderFrameHost of the main frame.
  content::RenderFrameHost* NavigateMainFrameToExtensionPage() {
    auto* web_contents = GetActiveWebContents();
    EXPECT_TRUE(content::NavigateToURL(
        web_contents, extension_->GetResourceURL("empty.html")));
    return web_contents->GetPrimaryMainFrame();
  }

  virtual const Extension* MakeExtension() = 0;

  const Extension* MakeExtension(
      const std::vector<std::string>& host_patterns) {
    ChromeTestExtensionLoader loader(profile());
    base::ListValue permissions;
    for (const auto& host_pattern : host_patterns) {
      permissions.Append(host_pattern);
    }
    auto manifest =
        base::DictValue()
            .Set("name", "Cookies test extension")
            .Set("version", "1")
            .Set("manifest_version", 2)
            .Set("web_accessible_resources", base::ListValue().Append("*.html"))
            .Set("content_security_policy", kCspHeader)
            .Set("permissions", std::move(permissions));
    extension_dir_->WriteFile(FILE_PATH_LITERAL("empty.html"), "");
    extension_dir_->WriteFile(FILE_PATH_LITERAL("script.js"), "");
    extension_dir_->WriteManifest(manifest);

    const Extension* extension =
        loader.LoadExtension(extension_dir_->UnpackedPath()).get();

    DCHECK(extension);
    return extension;
  }

  // The test server needs to be HTTPS because a SameSite=None cookie must be
  // Secure.
  net::EmbeddedTestServer* test_server() { return &test_server_; }

  ExtensionCookiesTestHelper& helper() { return *helper_; }

  std::unique_ptr<ExtensionCookiesTestHelper> helper_;

  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestExtensionDir> extension_dir_;
  raw_ptr<const Extension, DanglingUntriaged> extension_ = nullptr;
};

// Tests for special handling of SameSite cookies for extensions:
// A request should be treated as same-site for the purposes of SameSite
// cookies if either
//  1) the request initiator is an extension with access to the requested URL,
//  2) the site_for_cookies is an extension with access to the requested URL,
//     and the request initiator (if it exists) is same-site to the requested
//     URL and also the extension has access to it.
// See url_loader_util::ShouldForceIgnoreSiteForCookies().
//
// The test fixture param is whether or not legacy SameSite semantics are
// enabled (i.e, whether SameSite-by-default cookies and SameSite=None
// requires Secure are disabled).
class ExtensionSameSiteCookiesTest
    : public ExtensionCookiesTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ExtensionSameSiteCookiesTest() = default;
  ~ExtensionSameSiteCookiesTest() override = default;
  ExtensionSameSiteCookiesTest(const ExtensionSameSiteCookiesTest&) = delete;
  ExtensionSameSiteCookiesTest& operator=(const ExtensionSameSiteCookiesTest&) =
      delete;

  void SetUpOnMainThread() override {
    ExtensionCookiesTest::SetUpOnMainThread();

    // If SameSite access semantics is "legacy", add content settings to allow
    // legacy access for all sites.
    if (HasLegacySameSiteAccessSemantics()) {
      GetProfile()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext()
          ->GetCookieManager(
              cookie_manager_remote_.BindNewPipeAndPassReceiver());
      cookie_manager_remote_->SetContentSettings(
          ContentSettingsType::LEGACY_COOKIE_ACCESS,
          {ContentSettingPatternSource(
              ContentSettingsPattern::Wildcard(),
              ContentSettingsPattern::Wildcard(),
              base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
              content_settings::ProviderType::kNone, /*incognito=*/false)},
          base::NullCallback());
      cookie_manager_remote_.FlushForTesting();
    }
  }

 protected:
  // Sets an array of cookies with various SameSite values.
  void SetCookies(const std::string& host) {
    helper().SetCookies(
        host,
        {
            base::StrCat({ExtensionCookiesTestHelper::kNoneCookie,
                          ExtensionCookiesTestHelper::kSameSiteNoneAttribute}),
            base::StrCat({ExtensionCookiesTestHelper::kLaxCookie,
                          ExtensionCookiesTestHelper::kSameSiteLaxAttribute}),
            base::StrCat(
                {ExtensionCookiesTestHelper::kStrictCookie,
                 ExtensionCookiesTestHelper::kSameSiteStrictAttribute}),
            ExtensionCookiesTestHelper::kUnspecifiedCookie,
        });
  }

  // Expect that all cookies, including SameSite cookies, are present.
  void ExpectSameSiteCookies(const std::string& cookie_header) {
    EXPECT_THAT(ExtensionCookiesTestHelper::AsCookies(cookie_header),
                testing::UnorderedElementsAre(
                    ExtensionCookiesTestHelper::kNoneCookie,
                    ExtensionCookiesTestHelper::kLaxCookie,
                    ExtensionCookiesTestHelper::kStrictCookie,
                    ExtensionCookiesTestHelper::kUnspecifiedCookie));
  }

  // Expect that only cookies without SameSite are present.
  void ExpectNoSameSiteCookies(const std::string& cookie_header) {
    std::vector<std::string> expected = {
        ExtensionCookiesTestHelper::kNoneCookie};
    if (HasLegacySameSiteAccessSemantics()) {
      expected.push_back(ExtensionCookiesTestHelper::kUnspecifiedCookie);
    }
    EXPECT_THAT(ExtensionCookiesTestHelper::AsCookies(cookie_header),
                testing::UnorderedElementsAreArray(expected));
  }

  const Extension* MakeExtension() override {
    return ExtensionCookiesTest::MakeExtension(
        {ExtensionCookiesTestHelper::kPermissionPattern1,
         ExtensionCookiesTestHelper::kPermissionPattern1Sub,
         ExtensionCookiesTestHelper::kPermissionPattern2});
  }

  bool HasLegacySameSiteAccessSemantics() { return GetParam(); }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
};

// Tests where the extension page initiates the request.

// Extension initiates request to permitted host => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies =
      helper().FetchCookies(frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension initiates request to disallowed host => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies = helper().FetchCookies(
      frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests with one frame on an extension page which makes the request.

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSameSiteFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site => SameSite cookies are sent.
// crbug.com/40158945: flaky on linux, win, and mac
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSameSiteFrame_Navigation) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().NavigateChildAndGetCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a subdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSubdomainFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedSubdomain);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a superdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSuperdomainFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedSubdomain);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are cross-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedCrossSiteFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kOtherPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       CrossSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not, even though they are same-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is not permitted but requested URL
// is permitted, even though they are same-site => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorNotPermittedRequestPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are same-site but
// not permitted => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorAndRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests where the initiator is a nested frame. Here it doesn't actually matter
// what the initiator is nested in, because we don't check.

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site allowed frame, request is to the same site => SameSite cookies are
// attached.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedSameSitePermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// cross-site allowed frame, request is to the same site => SameSite cookies are
// attached.
// This is kind of an interesting case. Should we attach SameSite cookies here?
// If we only check first-partyness between each frame ancestor and the main
// frame, then we consider all of these frames first-party to the extension, so
// we should attach SameSite cookies here. (This is the current algorithm in the
// spec, which says to check each ancestor against the top frame:
// https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis-03#section-5.2.1)
// If we also want to ensure first-partyness between each frame and its
// immediate parent, then we should not send SameSite cookies here. See
// crbug.com/40108668.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedCrossSitePermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kOtherPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// The following tests are correct for current behavior, but should probably
// change in the future. We should be walking up the whole frame tree instead of
// only checking permissions and same-siteness for the initiator and request.

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// cross-site disallowed frame, request is to the same site => SameSite cookies
// are attached (but ideally shouldn't be).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       NestedCrossSiteNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site disallowed frame, request is to the same site => SameSite cookies
// are attached (but ideally shouldn't be).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       NestedSameSiteNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      main_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// TODO(crbug.com/509639786): Flaky on desktop Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ActiveTabPermissions_BackgroundPage \
  DISABLED_ActiveTabPermissions_BackgroundPage
#else
#define MAYBE_ActiveTabPermissions_BackgroundPage \
  ActiveTabPermissions_BackgroundPage
#endif
// SameSite-cookies-flavoured copy of the ExtensionActiveTabTest.ActiveTab test.
// In this test, the effective extension permissions are changing at runtime
// - the test verifies that the changing permissions are correctly propagated
// into the SameSite cookie decisions (e.g. in
// network::url_loader_util::ShouldForceIgnoreSiteForCookies).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       MAYBE_ActiveTabPermissions_BackgroundPage) {
  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
      {
        "name": "ActiveTab permissions vs SameSite cookies",
        "version": "1.0",
        "manifest_version": 2,
        "browser_action": {
          "default_title": "activeTab"
        },
        "permissions": ["activeTab"],
        "background": {
          "scripts": ["bg_script.js"]
        }
      } )";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("bg_script.js"), "");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  content::RenderFrameHost* background_page =
      ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(extension->id())
          ->host_contents()
          ->GetPrimaryMainFrame();

  // Set up a test scenario:
  // - top-level frame: kActiveTabHost
  GURL original_document_url =
      test_server()->GetURL(kActiveTabHost, "/title1.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, original_document_url));
  SetCookies(kActiveTabHost);

  // Based on activeTab, the extension shouldn't be initially granted access to
  // `kActiveTabHost`.
  {
    SCOPED_TRACE("TEST STEP 1: Initial fetch.");
    std::string cookies =
        helper().FetchCookies(background_page, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension still shouldn't have access to `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies =
        helper().FetchCookies(background_page, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, true);
  {
    // ActiveTab access (just like OOR-CORS access) extends to the background
    // page.  This is desirable, because
    // 1) there is no security boundary between A) extension background pages
    //    and B) extension frames in the tab
    // 2) it seems best to highlight #1 by simplistically granting extra
    //    capabilities to the whole extension (rather than forcing the extension
    //    authors to jump through extra hurdles to utilize the new capability).
    SCOPED_TRACE("TEST STEP 3: After granting ActiveTab access.");
    std::string cookies =
        helper().FetchCookies(background_page, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different, same-origin document should retain
  // extension's access to the origin.
  GURL another_document_url =
      test_server()->GetURL(kActiveTabHost, "/title2.html");
  EXPECT_NE(another_document_url, original_document_url);
  EXPECT_EQ(url::Origin::Create(another_document_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(NavigateToURL(web_contents, another_document_url));
  {
    SCOPED_TRACE(
        "TEST STEP 4: After navigating the tab cross-document, "
        "but still same-origin.");
    std::string cookies =
        helper().FetchCookies(background_page, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different origin should revoke extension's access
  // to the tab.
  GURL cross_origin_url = test_server()->GetURL(
      ExtensionCookiesTestHelper::kCrossOriginHost, "/title1.html");
  EXPECT_NE(url::Origin::Create(cross_origin_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(NavigateToURL(web_contents, cross_origin_url));
  {
    SCOPED_TRACE("TEST STEP 5: After navigating the tab cross-origin.");
    std::string cookies =
        helper().FetchCookies(background_page, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ActiveTabPermissions_ExtensionSubframeInTab) {
  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
      {
        "name": "ActiveTab permissions vs SameSite cookies",
        "version": "1.0",
        "manifest_version": 2,
        "browser_action": {
          "default_title": "activeTab"
        },
        "permissions": ["activeTab"],
        "web_accessible_resources": ["subframe.html"]
      } )";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("subframe.html"),
                          "<p>Extension frame</p>");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Set up a test scenario:
  // - top-level frame: kActiveTabHost
  // - subframe: extension
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents, test_server()->GetURL(kActiveTabHost, "/title1.html")));
  CookieSettingsFactory::GetForProfile(profile())->SetCookieSetting(
      test_server()->GetURL(kActiveTabHost, "/"), CONTENT_SETTING_ALLOW);
  SetCookies(kActiveTabHost);
  content::RenderFrameHost* extension_subframe = nullptr;
  {
    content::TestNavigationObserver subframe_nav_observer(web_contents);
    constexpr char kSubframeInjectionScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
    )";
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        content::JsReplace(kSubframeInjectionScriptTemplate,
                           extension->GetResourceURL("subframe.html"))));
    subframe_nav_observer.Wait();
    extension_subframe = ChildFrameAt(web_contents, 0);
    ASSERT_TRUE(extension_subframe);
    ASSERT_EQ(extension->origin(),
              extension_subframe->GetLastCommittedOrigin());
  }

  // Based on activeTab, the extension shouldn't be initially granted access to
  // `kActiveTabHost`.
  {
    SCOPED_TRACE("TEST STEP 1: Initial fetch.");
    std::string cookies =
        helper().FetchCookies(extension_subframe, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension still shouldn't have access to `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies =
        helper().FetchCookies(extension_subframe, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, true);
  {
    // ActiveTab should grant access to SameSite cookies to the
    // `extension_subframe`.
    SCOPED_TRACE("TEST STEP 3: After granting ActiveTab access.");
    std::string cookies =
        helper().FetchCookies(extension_subframe, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }
}

// TODO(crbug.com/509639786): Flaky on desktop Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ActiveTabPermissions_ExtensionServiceWorker \
  DISABLED_ActiveTabPermissions_ExtensionServiceWorker
#else
#define MAYBE_ActiveTabPermissions_ExtensionServiceWorker \
  ActiveTabPermissions_ExtensionServiceWorker
#endif
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       MAYBE_ActiveTabPermissions_ExtensionServiceWorker) {
  const char kServiceWorker[] = R"(
      chrome.runtime.onMessage.addListener(
          function(request, sender, sendResponse) {
            if (request.url) {
              fetch(request.url, {method: 'GET', credentials: 'include'})
                .then(response => response.text())
                .then(text => sendResponse(text))
                .catch(err => sendResponse('error: ' + err));
              return true;
            }
          });
      chrome.test.sendMessage('WORKER_RUNNING');
  )";
  auto fetch_via_extension_service_worker =
      [this](content::RenderFrameHost* extension_frame,
             const std::string& host) -> std::string {
    // Build a script that will send a message to the extension service worker,
    // asking it to perform a `fetch` and reply with the response.
    const char kFetchTemplate[] = R"(
        chrome.runtime.sendMessage({url: $1}, function(response) {
            domAutomationController.send(response);
        });
    )";
    GURL cookie_url = this->test_server()->GetURL(
        host, ExtensionCookiesTestHelper::kFetchCookiesPath);
    std::string fetch_script = content::JsReplace(kFetchTemplate, cookie_url);

    // Use `fetch_script` to ask the service worker to perform a `fetch` and
    // reply with the response.
    content::DOMMessageQueue queue(extension_frame);
    content::ExecuteScriptAsync(extension_frame, fetch_script);

    // Provide the HTTP response.
    url::Origin initiator = extension_frame->GetLastCommittedOrigin();
    helper().WaitForRequestAndRespondWithCookies(initiator);

    // Read back the response reported by the extension service worker.
    std::string json;
    EXPECT_TRUE(queue.WaitForMessage(&json));
    std::optional<base::Value> value =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
    EXPECT_TRUE(value->is_string());
    return value->GetString();
  };

  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
      {
        "name": "ActiveTab permissions vs SameSite cookies",
        "version": "1.0",
        "manifest_version": 2,
        "browser_action": {
          "default_title": "activeTab"
        },
        "permissions": ["activeTab"],
        "background": {"service_worker": "bg_worker.js"}
      } )";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("bg_worker.js"), kServiceWorker);
  extension_dir.WriteFile(FILE_PATH_LITERAL("frame.html"),
                          "<p>Extension frame</p>");
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  // Set up a test scenario:
  // - tab1: top-level frame: kActiveTabHost
  // - tab2: top-level frame: extension (for triggering fetches in the
  //                                     extension's service worker)
  GURL original_document_url =
      test_server()->GetURL(kActiveTabHost, "/title1.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, original_document_url));
  EXPECT_EQ(
      kActiveTabHost,
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL().GetHost());
  SetCookies(kActiveTabHost);
  GURL extension_frame_url = extension->GetResourceURL("frame.html");
  auto* browser = GetBrowserWindowInterface();
  auto* tab_list = TabListInterface::From(browser);
  ASSERT_TRUE(tab_list);
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tab_list->OpenTab(extension_frame_url, /*index=*/-1, /*foreground=*/false);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  content::WebContents* extension_contents = tab_list->GetTab(1)->GetContents();
  content::WaitForLoadStop(extension_contents);
  content::RenderFrameHost* extension_frame =
      extension_contents->GetPrimaryMainFrame();
  EXPECT_EQ(extension_frame_url, extension_frame->GetLastCommittedURL());

  // Based on activeTab, the extension shouldn't be initially granted access to
  // `kActiveTabHost`.
  {
    SCOPED_TRACE("TEST STEP 1: Initial fetch.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension still shouldn't have access to `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension, true);
  {
    // ActiveTab access (just like OOR-CORS access) extends to the service
    // worker of an extension.  This is desirable, because
    // 1) there is no security boundary between A) extension service worker
    //    and B) extension frames in the tab
    // 2) it seems best to highlight #1 by simplistically granting extra
    //    capabilities to the whole extension (rather than forcing the extension
    //    authors to jump through extra hurdles to utilize the new capability).
    SCOPED_TRACE("TEST STEP 3: After granting ActiveTab access.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different, same-origin document should retain
  // extension's access to the origin.
  GURL another_document_url =
      test_server()->GetURL(kActiveTabHost, "/title2.html");
  EXPECT_NE(another_document_url, original_document_url);
  EXPECT_EQ(url::Origin::Create(another_document_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(NavigateToURL(web_contents, another_document_url));
  {
    SCOPED_TRACE(
        "TEST STEP 4: After navigating the tab cross-document, "
        "but still same-origin.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different origin should revoke extension's access
  // to the tab.
  GURL cross_origin_url = test_server()->GetURL(
      ExtensionCookiesTestHelper::kCrossOriginHost, "/title1.html");
  EXPECT_NE(url::Origin::Create(cross_origin_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(NavigateToURL(web_contents, cross_origin_url));
  {
    SCOPED_TRACE("TEST STEP 5: After navigating the tab cross-origin.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }
}

INSTANTIATE_TEST_SUITE_P(All, ExtensionSameSiteCookiesTest, ::testing::Bool());

}  // namespace

}  // namespace extensions
