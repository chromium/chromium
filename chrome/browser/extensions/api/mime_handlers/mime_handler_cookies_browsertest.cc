// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_cookies_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// MIME handler-specific: the cross-site embedder hosting the PDF that
// triggers the MIME handler. Cross-site to the extension AND to every
// permitted host, so the renderer-computed `site_for_cookies` is null
// without the browser-side override.
constexpr char kEmbedderHost[] = "pdf-embedder.test";

// CSP header applied to child iframes so script execution is allowed across
// the various test hosts.
constexpr char kCspHeader[] =
    "script-src 'self' https://a.example:* https://sub.a.example:* "
    "https://notallowedsub.a.example:* https://b.example:* "
    "https://c.example:* 'unsafe-inline'; object-src 'self'";

}  // namespace

// Mirror of chrome/browser/extensions/extension_cookies_browsertest.cc:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_cookies_browsertest.cc;l=77;drc=c0edd277235be3e52907c24cae01027c72493291
//
// The extension is loaded as a MIME handler subframe inside a cross-site
// embedder. In that arrangement the renderer reports `site_for_cookies = null`
// for the extension subframe and its descendants because the top-level is
// cross-site. The browser-side `IsolationInfo` restores the extension's
// `site_for_cookies` on the wire, so SameSite cookie delivery follows the
// extension's host permissions just as it does when the extension is the
// top-level.
//
// Base class - mirrors `ExtensionCookiesTest` but loads the test extension
// as the MIME handler for application/pdf and triggers it from a cross-site
// embedder.
class MimeHandlerCookiesTest : public ExtensionApiTest {
 public:
  MimeHandlerCookiesTest()
      : test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiMimeHandler);
  }

  MimeHandlerCookiesTest(const MimeHandlerCookiesTest&) = delete;
  MimeHandlerCookiesTest& operator=(const MimeHandlerCookiesTest&) = delete;
  ~MimeHandlerCookiesTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    extension_dir_ = std::make_unique<TestExtensionDir>();
    extension_ = MakeExtension();
    ASSERT_TRUE(extension_);
    helper_ = std::make_unique<ExtensionCookiesTestHelper>(
        *test_server(), *profile(), kCspHeader);
    host_resolver()->AddRule("*", "127.0.0.1");
    net::test_server::RegisterDefaultHandlers(test_server());
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_server()->ServeFilesFromDirectory(test_data_dir);
    // Lets the embedder URL `/test.pdf` resolve so the MIME handler fires.
    test_server()->ServeFilesFromSourceDirectory("chrome/test/data/pdf");
    test_server()->SetCertHostnames(
        {ExtensionCookiesTestHelper::kPermittedHost,
         ExtensionCookiesTestHelper::kOtherPermittedHost,
         ExtensionCookiesTestHelper::kNotPermittedHost,
         ExtensionCookiesTestHelper::kPermittedSubdomain,
         ExtensionCookiesTestHelper::kNotPermittedSubdomain,
         ExtensionCookiesTestHelper::kCrossOriginHost, kEmbedderHost});
    ASSERT_TRUE(test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // The helper holds raw references to the test server and `Profile`.
    // Reset it here so those references are released before the `Profile`
    // is torn down.
    helper_.reset();
    // `extension_` is owned by `ExtensionRegistry`, which `ProfileImpl`
    // destroys during browser shutdown — before this fixture's destructor
    // runs. Clear the `raw_ptr` here so it doesn't outlive the pointee.
    extension_ = nullptr;
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  // Navigates the active tab to https://pdf-embedder.test/test.pdf so the
  // MIME handler activates, then returns the chrome-extension:// frame. The
  // extension subframe commits asynchronously after the top-level navigation,
  // so wait specifically for its commit before walking the frame tree.
  content::RenderFrameHost* LoadMimeHandlerInCrossSiteEmbedder() {
    GURL pdf_url = test_server()->GetURL(kEmbedderHost, "/test.pdf");
    content::TestNavigationObserver handler_observer(
        extension_->GetResourceURL("handler.html"));
    handler_observer.WatchExistingWebContents();
    if (!content::NavigateToURL(GetActiveWebContents(), pdf_url)) {
      return nullptr;
    }
    handler_observer.Wait();
    content::RenderFrameHost* result = nullptr;
    GetActiveWebContents()->ForEachRenderFrameHost(
        [&](content::RenderFrameHost* rfh) {
          if (rfh->GetLastCommittedOrigin().scheme() == kExtensionScheme) {
            result = rfh;
          }
        });
    return result;
  }

  virtual const Extension* MakeExtension() = 0;

  // Constructs the dynamic MIME handler extension with `host_patterns` as its
  // `host_permissions`. The MIME handler API itself does not require
  // `host_permissions`; these tests use them solely to exercise cookie
  // semantics for permitted vs non-permitted hosts (mirroring
  // `ExtensionCookiesTest::MakeExtension()`).
  const Extension* MakeExtension(
      const std::vector<std::string>& host_patterns) {
    ChromeTestExtensionLoader loader(profile());
    base::ListValue host_permissions;
    for (const std::string& pattern : host_patterns) {
      host_permissions.Append(pattern);
    }
    base::DictValue pdf_handler;
    pdf_handler.Set("handler_url", "handler.html");
    pdf_handler.Set("can_embed", false);
    base::DictValue mime_types_handler;
    mime_types_handler.Set("application/pdf", std::move(pdf_handler));
    auto manifest =
        base::DictValue()
            .Set("name", "MIME handler cookies test extension")
            .Set("version", "1")
            .Set("manifest_version", 3)
            .Set("host_permissions", std::move(host_permissions))
            .Set("mime_types_handler", std::move(mime_types_handler));
    extension_dir_->WriteManifest(manifest);
    extension_dir_->WriteFile(FILE_PATH_LITERAL("handler.html"),
                              "<!doctype html>");

    const Extension* extension =
        loader.LoadExtension(extension_dir_->UnpackedPath()).get();
    DCHECK(extension);
    return extension;
  }

  const net::EmbeddedTestServer* test_server() const {
    return test_server_.get();
  }
  net::EmbeddedTestServer* test_server() { return test_server_.get(); }

  ExtensionCookiesTestHelper& helper() { return *helper_; }

 private:
  std::unique_ptr<ExtensionCookiesTestHelper> helper_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
  std::unique_ptr<TestExtensionDir> extension_dir_;
  raw_ptr<const Extension> extension_ = nullptr;
};

// Tests for special handling of SameSite cookies for extensions, in the
// MIME handler context. Mirror of `ExtensionSameSiteCookiesTest`:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_cookies_browsertest.cc;l=177;drc=c0edd277235be3e52907c24cae01027c72493291
//
// A request should be treated as same-site for the purposes of SameSite
// cookies if either
//  1) the request initiator is an extension with access to the requested
//     URL, or
//  2) the site_for_cookies is an extension with access to the requested
//     URL, and the request initiator (if it exists) is same-site to the
//     requested URL and also the extension has access to it.
// See `network::URLLoader::ShouldForceIgnoreSiteForCookies`.
//
// Test fixture param toggles legacy SameSite access semantics on/off.
class MimeHandlerSameSiteCookiesTest
    : public MimeHandlerCookiesTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MimeHandlerSameSiteCookiesTest() = default;
  MimeHandlerSameSiteCookiesTest(const MimeHandlerSameSiteCookiesTest&) =
      delete;
  MimeHandlerSameSiteCookiesTest& operator=(
      const MimeHandlerSameSiteCookiesTest&) = delete;
  ~MimeHandlerSameSiteCookiesTest() override = default;

  void SetUpOnMainThread() override {
    MimeHandlerCookiesTest::SetUpOnMainThread();
    if (HasLegacySameSiteAccessSemantics()) {
      profile()
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

  void ExpectSameSiteCookies(const std::string& cookie_header) {
    EXPECT_THAT(ExtensionCookiesTestHelper::AsCookies(cookie_header),
                testing::UnorderedElementsAre(
                    ExtensionCookiesTestHelper::kNoneCookie,
                    ExtensionCookiesTestHelper::kLaxCookie,
                    ExtensionCookiesTestHelper::kStrictCookie,
                    ExtensionCookiesTestHelper::kUnspecifiedCookie));
  }

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
    return MimeHandlerCookiesTest::MakeExtension(
        {ExtensionCookiesTestHelper::kPermissionPattern1,
         ExtensionCookiesTestHelper::kPermissionPattern1Sub,
         ExtensionCookiesTestHelper::kPermissionPattern2});
  }

  bool HasLegacySameSiteAccessSemantics() { return GetParam(); }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
};

// Tests where the MIME handler extension page initiates the request.

// Extension initiates request to permitted host => SameSite cookies are
// sent: the initiator is the extension and the URL is in its host
// permissions.
IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       ExtensionInitiatedPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  std::string cookies = helper().FetchCookies(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension initiates request to disallowed host => SameSite cookies are
// not sent.
IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       ExtensionInitiatedNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  std::string cookies = helper().FetchCookies(
      extension_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests with one frame on the MIME handler extension page that makes the
// request. The descendant is the initiator and the extension is the
// `site_for_cookies`; SameSite delivery hinges on whether the
// extension's host permissions cover the initiator and the request URL.

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       OnePermittedSameSiteFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Mirror of OnePermittedSameSiteFrame but uses a same-frame navigation
// instead of a fetch.
// See https://crbug.com/40158945 for the upstream flake history.
IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       OnePermittedSameSiteFrame_Navigation) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().NavigateChildAndGetCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       OnePermittedSubdomainFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedSubdomain);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       OnePermittedSuperdomainFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedSubdomain);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedSubdomain);
  ExpectSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       OnePermittedCrossSiteFrame) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kOtherPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       CrossSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       SameSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  ExpectNoSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       SameSiteInitiatorNotPermittedRequestPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       SameSiteInitiatorAndRequestNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  std::string cookies = helper().FetchCookies(
      child_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests where the initiator is a nested frame. Here it doesn't actually
// matter what the initiator is nested in, because the SameSite check
// only considers the initiator and the request URL against the
// extension's `site_for_cookies` and host permissions; it does not walk
// the ancestor chain. Mirrors:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_cookies_browsertest.cc;l=407;drc=c0edd277235be3e52907c24cae01027c72493291

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       NestedSameSitePermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       NestedCrossSitePermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kOtherPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// The following two tests are correct for current behavior, but should
// probably change in the future. We should be walking up the whole
// frame tree instead of only checking permissions and same-siteness for
// the initiator and request. See https://crbug.com/40108668. Mirrors:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_cookies_browsertest.cc;l=449;drc=c0edd277235be3e52907c24cae01027c72493291

// Initiator is allowed nested inside a cross-site disallowed frame, request
// is to the same site => SameSite cookies are attached (but ideally
// shouldn't be).
IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       NestedCrossSiteNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kNotPermittedHost);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Initiator is allowed nested inside a same-site disallowed frame, request
// is to the same site => SameSite cookies are attached (but ideally
// shouldn't be).
IN_PROC_BROWSER_TEST_P(MimeHandlerSameSiteCookiesTest,
                       NestedSameSiteNotPermitted) {
  SetCookies(ExtensionCookiesTestHelper::kPermittedHost);
  content::RenderFrameHost* extension_frame =
      LoadMimeHandlerInCrossSiteEmbedder();
  ASSERT_TRUE(extension_frame);
  content::RenderFrameHost* child_frame = helper().MakeChildFrame(
      extension_frame, ExtensionCookiesTestHelper::kNotPermittedSubdomain);
  content::RenderFrameHost* nested_frame = helper().MakeChildFrame(
      child_frame, ExtensionCookiesTestHelper::kPermittedHost);
  std::string cookies = helper().FetchCookies(
      nested_frame, ExtensionCookiesTestHelper::kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MimeHandlerSameSiteCookiesTest,
                         ::testing::Bool());

}  // namespace extensions
