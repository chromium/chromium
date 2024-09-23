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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
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

namespace extensions {

namespace {

const char* kPermittedHost = "a.example";
const char* kOtherPermittedHost = "b.example";
const char* kNotPermittedHost = "c.example";
const char* kPermittedSubdomain = "sub.a.example";
const char* kNotPermittedSubdomain = "notallowedsub.a.example";
const char* kPermissionPattern1 = "https://a.example/*";
const char* kPermissionPattern1Sub = "https://sub.a.example/*";
const char* kPermissionPattern2 = "https://b.example/*";

// Path for URL of custom ControllableHttpResponse
const char* kFetchCookiesPath = "/respondwithcookies";
// CSP header to be applied to the extension and the child frames
const char* kCspHeader =
    "script-src 'self' https://a.example:* https://sub.a.example:* "
    "https://notallowedsub.a.example:* https://b.example:* "
    "https://c.example:* https://d.example:* https://e.example; object-src "
    "'self'";

const char* kNoneCookie = "none=1";
const char* kLaxCookie = "lax=1";
const char* kStrictCookie = "strict=1";
const char* kUnspecifiedCookie = "unspecified=1";
const char* kSameSiteNoneAttribute = "; SameSite=None; Secure";
const char* kSameSiteLaxAttribute = "; SameSite=Lax";
const char* kSameSiteStrictAttribute = "; SameSite=Strict";

using testing::UnorderedElementsAreArray;

std::vector<std::string> AsCookies(const std::string& cookie_line) {
  return base::SplitString(cookie_line, ";", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

// Base class for special handling of cookies for extensions.
class ExtensionCookiesTest : public ExtensionBrowserTest {
 public:
  ExtensionCookiesTest() : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~ExtensionCookiesTest() override = default;
  ExtensionCookiesTest(const ExtensionCookiesTest&) = delete;
  ExtensionCookiesTest& operator=(const ExtensionCookiesTest&) = delete;

  void SetUpOnMainThread() override {
    constexpr int kMaxNumberOfCookieRequestsFromSingleTest = 15;

    ExtensionBrowserTest::SetUpOnMainThread();
    extension_dir_ = std::make_unique<TestExtensionDir>();
    extension_ = MakeExtension();
    for (int i = 0; i < kMaxNumberOfCookieRequestsFromSingleTest; i++) {
      http_responses_.push_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              test_server(), kFetchCookiesPath));
    }
    host_resolver()->AddRule("*", "127.0.0.1");
    net::test_server::RegisterDefaultHandlers(test_server());
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(test_server()->Start());
  }

  // Ignore cert errors for HTTPS test server, in order to use hostnames other
  // than localhost or 127.0.0.1.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 protected:
  // Navigates to the extension page in the main frame. Returns a pointer to the
  // RenderFrameHost of the main frame.
  content::RenderFrameHost* NavigateMainFrameToExtensionPage() {
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(), extension_->GetResourceURL("/empty.html")));
    return web_contents()->GetPrimaryMainFrame();
  }

  // Appends a child iframe via JS and waits for it to load. Returns a pointer
  // to the RenderFrameHost of the child frame. (Requests a page that responds
  // with the proper CSP header to allow scripts from the relevant origins.)
  content::RenderFrameHost* MakeChildFrame(content::RenderFrameHost* frame,
                                           const std::string& host) {
    EXPECT_FALSE(content::ChildFrameAt(frame, 0));
    GURL url = test_server()->GetURL(
        host,
        base::StrCat({"/set-header?Content-Security-Policy: ", kCspHeader}));
    const char kAppendFrameScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.src = $1;
        new Promise(resolve => {
          f.onload = function(e) {
              resolve(true);
              f.onload = undefined;
          }
          document.body.appendChild(f);
        });
        )";
    std::string append_frame_script =
        content::JsReplace(kAppendFrameScriptTemplate, url.spec());
    EXPECT_EQ(true, content::EvalJs(frame, append_frame_script));
    content::RenderFrameHost* child_frame = content::ChildFrameAt(frame, 0);
    EXPECT_EQ(url, child_frame->GetLastCommittedURL());
    return child_frame;
  }

  // Sets a vector of cookies directly into the cookie store, simulating being
  // set from a "strictly same-site" request context.
  void SetCookies(const std::string& host,
                  const std::vector<std::string>& cookies) {
    GURL url = test_server()->GetURL(host, "/");
    for (const std::string& cookie : cookies) {
      content::SetCookie(browser()->profile(), url, cookie);
    }
  }

  // Makes a request to |host| from the context of |frame|, then returns the
  // cookies that were sent on that request.
  std::string FetchCookies(content::RenderFrameHost* frame,
                           const std::string& host) {
    GURL cookie_url = test_server()->GetURL(host, kFetchCookiesPath);
    const char kFetchCookiesScriptTemplate[] = R"(
        fetch($1, {method: 'GET', credentials: 'include'})
          .then((resp) => resp.text())
          .then((data) => window.domAutomationController.send(data));)";
    std::string fetch_cookies_script =
        content::JsReplace(kFetchCookiesScriptTemplate, cookie_url.spec());
    content::DOMMessageQueue messages(frame);
    ExecuteScriptAsync(frame, fetch_cookies_script);

    url::Origin initiator = frame->GetLastCommittedOrigin();
    WaitForRequestAndRespondWithCookies(initiator);

    std::string result;
    if (!messages.PopMessage(&result)) {
      EXPECT_TRUE(messages.WaitForMessage(&result));
    }
    base::TrimString(result, "\"", &result);
    return result;
  }

  // Triggers a `frame`-initiated navigation of `frame` to `host`, then returns
  // the cookies that were sent on that navigation request.
  std::string NavigateChildAndGetCookies(content::RenderFrameHost* frame,
                                         const std::string& host) {
    GURL cookie_url = test_server()->GetURL(host, kFetchCookiesPath);
    url::Origin initiator = frame->GetLastCommittedOrigin();
    content::TestNavigationObserver nav_observer(web_contents());
    // We cache the parent here, and use it to get the RenderFrameHost again
    // later, in order to allow cross-site navigations. Cross-site navigations
    // cause `frame` to be freed (and use a new RFHI for the new document), so
    // it is not safe to use `frame` after the call to `ExecuteScriptAsync`.
    content::RenderFrameHost* parent = frame->GetParent();
    // We assume there's only one child.
    DCHECK_EQ(frame, content::ChildFrameAt(parent, 0));
    ExecuteScriptAsync(frame, content::JsReplace("location = $1", cookie_url));
    WaitForRequestAndRespondWithCookies(initiator);
    nav_observer.Wait();

    return content::EvalJs(content::ChildFrameAt(parent, 0),
                           "document.body.innerText")
        .ExtractString();
  }

  // Responds to a request with the cookies that were sent with the request.
  // We can't simply use the default handler /echoheader?Cookie here, because it
  // doesn't send the appropriate Access-Control-Allow-Origin and
  // Access-Control-Allow-Credentials headers (which are required for this to
  // work since we are making cross-origin requests in these tests).
  void WaitForRequestAndRespondWithCookies(const url::Origin& initiator) {
    net::test_server::ControllableHttpResponse& http_response =
        GetNextCookieResponse();
    http_response.WaitForRequest();

    // Remove the trailing slash from the URL.
    std::string origin = initiator.GetURL().spec();
    base::TrimString(origin, "/", &origin);

    // Get the 'Cookie' header that was sent in the request.
    std::string cookie_header;
    auto cookie_header_it = http_response.http_request()->headers.find(
        net::HttpRequestHeaders::kCookie);
    if (cookie_header_it == http_response.http_request()->headers.end()) {
      cookie_header = "";
    } else {
      cookie_header = cookie_header_it->second;
    }
    std::string content_length = base::NumberToString(cookie_header.length());

    // clang-format off
    http_response.Send(
        base::StrCat({
        "HTTP/1.1 200 OK\r\n",
        "Content-Type: text/plain; charset=utf-8\r\n",
        "Content-Length: ", content_length, "\r\n",
        "Access-Control-Allow-Origin: ", origin, "\r\n",
        "Access-Control-Allow-Credentials: true\r\n",
        "\r\n",
        cookie_header}));
    // clang-format on

    http_response.Done();
  }

  virtual const Extension* MakeExtension() = 0;

  const Extension* MakeExtension(
      const std::vector<std::string>& host_patterns) {
    ChromeTestExtensionLoader loader(profile());
    base::Value::List permissions;
    for (const auto& host_pattern : host_patterns) {
      permissions.Append(host_pattern);
    }
    auto manifest = base::Value::Dict()
                        .Set("name", "Cookies test extension")
                        .Set("version", "1")
                        .Set("manifest_version", 2)
                        .Set("web_accessible_resources",
                             base::Value::List().Append("*.html"))
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

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::test_server::ControllableHttpResponse& GetNextCookieResponse() {
    // If the DCHECK below fails, consider increasing the value of the
    // kMaxNumberOfCookieRequestsFromSingleTest constant above.
    DCHECK_LT(index_of_active_http_response_, http_responses_.size());

    return *http_responses_[index_of_active_http_response_++];
  }

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      http_responses_;
  size_t index_of_active_http_response_ = 0;

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
// See URLLoader::ShouldForceIgnoreSiteForCookies().
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
      browser()
          ->profile()
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
    ExtensionCookiesTest::SetCookies(
        host, {
                  base::StrCat({kNoneCookie, kSameSiteNoneAttribute}),
                  base::StrCat({kLaxCookie, kSameSiteLaxAttribute}),
                  base::StrCat({kStrictCookie, kSameSiteStrictAttribute}),
                  kUnspecifiedCookie,
              });
  }

  // Expect that all cookies, including SameSite cookies, are present.
  void ExpectSameSiteCookies(const std::string& cookie_header) {
    EXPECT_THAT(
        AsCookies(cookie_header),
        testing::UnorderedElementsAre(kNoneCookie, kLaxCookie, kStrictCookie,
                                      kUnspecifiedCookie));
  }

  // Expect that only cookies without SameSite are present.
  void ExpectNoSameSiteCookies(const std::string& cookie_header) {
    std::vector<std::string> expected = {kNoneCookie};
    if (HasLegacySameSiteAccessSemantics()) {
      expected.push_back(kUnspecifiedCookie);
    }
    EXPECT_THAT(AsCookies(cookie_header),
                testing::UnorderedElementsAreArray(expected));
  }

  const Extension* MakeExtension() override {
    return ExtensionCookiesTest::MakeExtension(
        {kPermissionPattern1, kPermissionPattern1Sub, kPermissionPattern2});
  }

  bool HasLegacySameSiteAccessSemantics() { return GetParam(); }

 private:
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
};

// Tests where the extension page initiates the request.

// Extension initiates request to permitted host => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies = FetchCookies(frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension initiates request to disallowed host => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies = FetchCookies(frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests with one frame on an extension page which makes the request.

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSameSiteFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site => SameSite cookies are sent.
// crbug.com/1153083: flaky on linux, win, and mac
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSameSiteFrame_Navigation) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = NavigateChildAndGetCookies(child_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a subdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSubdomainFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedSubdomain);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a superdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSuperdomainFrame) {
  SetCookies(kPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedSubdomain);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are cross-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedCrossSiteFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kOtherPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       CrossSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not, even though they are same-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(kNotPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedSubdomain);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is not permitted but requested URL
// is permitted, even though they are same-site => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorNotPermittedRequestPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedSubdomain);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are same-site but
// not permitted => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorAndRequestNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests where the initiator is a nested frame. Here it doesn't actually matter
// what the initiator is nested in, because we don't check.

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site allowed frame, request is to the same site => SameSite cookies are
// attached.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedSameSitePermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
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
// crbug.com/1027258.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedCrossSitePermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kOtherPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
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
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site disallowed frame, request is to the same site => SameSite cookies
// are attached (but ideally shouldn't be).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       NestedSameSiteNotPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedSubdomain);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// SameSite-cookies-flavoured copy of the ExtensionActiveTabTest.ActiveTab test.
// In this test, the effective extension permissions are changing at runtime
// - the test verifies that the changing permissions are correctly propagated
// into the SameSite cookie decisions (e.g. in
// network::URLLoader::ShouldForceIgnoreSiteForCookies).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ActiveTabPermissions_BackgroundPage) {
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
  constexpr char kActiveTabHost[] = "active-tab.example";
  GURL original_document_url =
      test_server()->GetURL(kActiveTabHost, "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_document_url));
  SetCookies(kActiveTabHost);

  // Based on activeTab, the extension shouldn't be initially granted access to
  // `kActiveTabHost`.
  {
    SCOPED_TRACE("TEST STEP 1: Initial fetch.");
    std::string cookies = FetchCookies(background_page, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension still shouldn't have access to `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents())
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies = FetchCookies(background_page, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents())
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
    std::string cookies = FetchCookies(background_page, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different, same-origin document should retain
  // extension's access to the origin.
  GURL another_document_url =
      test_server()->GetURL(kActiveTabHost, "/title2.html");
  EXPECT_NE(another_document_url, original_document_url);
  EXPECT_EQ(url::Origin::Create(another_document_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), another_document_url));
  {
    SCOPED_TRACE(
        "TEST STEP 4: After navigating the tab cross-document, "
        "but still same-origin.");
    std::string cookies = FetchCookies(background_page, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }

  // Navigating the tab to a different origin should revoke extension's access
  // to the tab.
  GURL cross_origin_url = test_server()->GetURL("other.com", "/title1.html");
  EXPECT_NE(url::Origin::Create(cross_origin_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_url));
  {
    SCOPED_TRACE("TEST STEP 5: After navigating the tab cross-origin.");
    std::string cookies = FetchCookies(background_page, kActiveTabHost);
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
  constexpr char kActiveTabHost[] = "active-tab.example";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL(kActiveTabHost, "/title1.html")));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(test_server()->GetURL(kActiveTabHost, "/"),
                         CONTENT_SETTING_ALLOW);
  SetCookies(kActiveTabHost);
  content::RenderFrameHost* extension_subframe = nullptr;
  {
    content::TestNavigationObserver subframe_nav_observer(web_contents());
    constexpr char kSubframeInjectionScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
    )";
    ASSERT_TRUE(content::ExecJs(
        web_contents(),
        content::JsReplace(kSubframeInjectionScriptTemplate,
                           extension->GetResourceURL("subframe.html"))));
    subframe_nav_observer.Wait();
    extension_subframe = ChildFrameAt(web_contents(), 0);
    ASSERT_TRUE(extension_subframe);
    ASSERT_EQ(extension->origin(),
              extension_subframe->GetLastCommittedOrigin());
  }

  // Based on activeTab, the extension shouldn't be initially granted access to
  // `kActiveTabHost`.
  {
    SCOPED_TRACE("TEST STEP 1: Initial fetch.");
    std::string cookies = FetchCookies(extension_subframe, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Do one pass of BrowserAction without granting activeTab permission,
  // extension still shouldn't have access to `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents())
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies = FetchCookies(extension_subframe, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents())
      ->RunAction(extension, true);
  {
    // ActiveTab should grant access to SameSite cookies to the
    // `extension_subframe`.
    SCOPED_TRACE("TEST STEP 3: After granting ActiveTab access.");
    std::string cookies = FetchCookies(extension_subframe, kActiveTabHost);
    ExpectSameSiteCookies(cookies);
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ActiveTabPermissions_ExtensionServiceWorker) {
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
    GURL cookie_url = this->test_server()->GetURL(host, kFetchCookiesPath);
    std::string fetch_script = content::JsReplace(kFetchTemplate, cookie_url);

    // Use `fetch_script` to ask the service worker to perform a `fetch` and
    // reply with the response.
    content::DOMMessageQueue queue(extension_frame);
    content::ExecuteScriptAsync(extension_frame, fetch_script);

    // Provide the HTTP response.
    url::Origin initiator = extension_frame->GetLastCommittedOrigin();
    WaitForRequestAndRespondWithCookies(initiator);

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
  constexpr char kActiveTabHost[] = "active-tab.example";
  GURL original_document_url =
      test_server()->GetURL(kActiveTabHost, "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_document_url));
  EXPECT_EQ(
      kActiveTabHost,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL().host());
  SetCookies(kActiveTabHost);
  GURL extension_frame_url = extension->GetResourceURL("frame.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_frame_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::RenderFrameHost* extension_frame =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame();
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
  ExtensionActionRunner::GetForWebContents(web_contents())
      ->RunAction(extension, false);
  {
    SCOPED_TRACE("TEST STEP 2: After BrowserAction without granting access.");
    std::string cookies =
        fetch_via_extension_service_worker(extension_frame, kActiveTabHost);
    ExpectNoSameSiteCookies(cookies);
  }

  // Granting activeTab permission to the extension should give it access to
  // `kActiveTabHost`.
  ExtensionActionRunner::GetForWebContents(web_contents())
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), another_document_url));
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
  GURL cross_origin_url = test_server()->GetURL("other.com", "/title1.html");
  EXPECT_NE(url::Origin::Create(cross_origin_url),
            url::Origin::Create(original_document_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cross_origin_url));
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
