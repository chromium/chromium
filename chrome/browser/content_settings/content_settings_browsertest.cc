// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "components/pdf/browser/pdf_frame_util.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

using content::BrowserThread;
using content_settings::PageSpecificContentSettings;
using net::URLRequestMockHTTPJob;

namespace {

BrowsingDataModel* GetSiteSettingsAllowedBrowsingDataModel(Browser* browser) {
  PageSpecificContentSettings* settings =
      PageSpecificContentSettings::GetForFrame(browser->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetPrimaryMainFrame());
  return settings->allowed_browsing_data_model();
}

BrowsingDataModel* GetSiteSettingsBlockedBrowsingDataModel(Browser* browser) {
  PageSpecificContentSettings* settings =
      PageSpecificContentSettings::GetForFrame(browser->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetPrimaryMainFrame());
  return settings->blocked_browsing_data_model();
}

net::CookieList ExtractCookiesFromModel(BrowsingDataModel* model) {
  net::CookieList result;
  for (const auto& [owner, key, details] : *model) {
    if (const net::CanonicalCookie* cookie =
            absl::get_if<net::CanonicalCookie>(&key.get())) {
      result.push_back(*cookie);
    }
  }
  return result;
}

size_t GetRenderFrameHostCount(content::RenderFrameHost* starting_frame) {
  size_t count = 0;
  starting_frame->ForEachRenderFrameHost(
      [&](content::RenderFrameHost*) { ++count; });
  return count;
}

class MockWebContentsLoadFailObserver : public content::WebContentsObserver {
 public:
  explicit MockWebContentsLoadFailObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~MockWebContentsLoadFailObserver() override = default;

  MOCK_METHOD1(DidFinishNavigation,
               void(content::NavigationHandle* navigation_handle));
};

MATCHER(IsErrorTooManyRedirects, "") {
  return arg->GetNetErrorCode() == net::ERR_TOO_MANY_REDIRECTS;
}

size_t GetModelCookieCount(const BrowsingDataModel* model) {
  size_t cookie_count = 0;
  for (const auto& [owner, key, details] : *model) {
    cookie_count += details->cookie_count;
  }
  return cookie_count;
}

}  // namespace

class ContentSettingsTest : public InProcessBrowserTest {
 public:
  ContentSettingsTest() {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Test the combination of different ways of accessing cookies --- JS, HTML,
// or the new async cookie-store API.
enum class CookieMode {
  kDocumentCookieJS,
  kHttp,
  kCookieStoreJS,
};

class CookieSettingsTest
    : public ContentSettingsTest,
      public testing::WithParamInterface<std::pair<CookieMode, CookieMode>> {
 public:
  CookieMode ReadMode() const { return GetParam().first; }
  CookieMode WriteMode() const { return GetParam().second; }

  void SetUpOnMainThread() override {
    RegisterDefaultHandlers(embedded_test_server());
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &CookieSettingsTest::MonitorRequest, base::Unretained(this)));
    RegisterDefaultHandlers(&https_server_);
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &CookieSettingsTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    // CookieStore API is for https only.
    if (ReadMode() == CookieMode::kCookieStoreJS ||
        WriteMode() == CookieMode::kCookieStoreJS)
      set_secure_scheme();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentSettingsTest::SetUpCommandLine(command_line);
  }

  void set_secure_scheme() { secure_scheme_ = true; }

  std::string ReadCookie(Browser* browser) {
    switch (ReadMode()) {
      case CookieMode::kDocumentCookieJS:
        return JSReadCookie(browser);
      case CookieMode::kHttp:
        return HttpReadCookie(browser);
      case CookieMode::kCookieStoreJS:
        return JSAsyncReadCookie(browser);
    }
  }

  void WriteCookie(Browser* browser) {
    switch (WriteMode()) {
      case CookieMode::kDocumentCookieJS:
        return JSWriteCookie(browser);
      case CookieMode::kHttp:
        return HttpWriteCookie(browser);
      case CookieMode::kCookieStoreJS:
        return JSAsyncWriteCookie(browser);
    }
  }

  // Check the cookie in an incognito window.
  void CookieCheckIncognitoWindow(bool cookies_enabled) {
    ASSERT_TRUE(ReadCookie(browser()).empty());

    Browser* incognito = CreateIncognitoBrowser();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, GetPageURL()));
    ASSERT_TRUE(ReadCookie(incognito).empty());
    WriteCookie(incognito);
    ASSERT_EQ(cookies_enabled, !ReadCookie(incognito).empty());

    // Ensure incognito cookies don't leak to regular profile.
    ASSERT_TRUE(ReadCookie(browser()).empty());

    // Ensure cookies get wiped after last incognito window closes.
    CloseBrowserSynchronously(incognito);

    incognito = CreateIncognitoBrowser();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, GetPageURL()));
    ASSERT_TRUE(ReadCookie(incognito).empty());
    CloseBrowserSynchronously(incognito);
  }

  void PreBasic() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
    ASSERT_TRUE(ReadCookie(browser()).empty());

    CookieCheckIncognitoWindow(true);

    WriteCookie(browser());
    ASSERT_FALSE(ReadCookie(browser()).empty());
  }

  void Basic() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
    ASSERT_FALSE(ReadCookie(browser()).empty());
  }

  GURL GetPageURL() { return GetServer()->GetURL("/simple.html"); }

  GURL GetSetCookieURL() {
    return GetServer()->GetURL("/set_cookie_header.html");
  }

  net::EmbeddedTestServer* GetOtherServer() {
    return secure_scheme_ ? embedded_test_server() : &https_server_;
  }

  net::EmbeddedTestServer* GetServer() {
    return secure_scheme_ ? &https_server_ : embedded_test_server();
  }

  // Read a cookie by fetching a url and checking what Cookie header (if any) it
  // saw.
  std::string HttpReadCookieWithURL(Browser* browser, const GURL& url) {
    {
      base::AutoLock auto_lock(cookies_seen_lock_);
      cookies_seen_.clear();
    }

    auto* network_context =
        browser->profile()->GetDefaultStoragePartition()->GetNetworkContext();
    content::LoadBasicRequest(network_context, url);

    {
      base::AutoLock auto_lock(cookies_seen_lock_);
      return cookies_seen_[url];
    }
  }

  // Set a cookie by visiting a page that has a Set-Cookie header.
  void HttpWriteCookieWithURL(Browser* browser, const GURL& url) {
    auto* frame = browser->tab_strip_model()
                      ->GetActiveWebContents()
                      ->GetPrimaryMainFrame();
    // Need to load via |frame| for the accessed/blocked cookies lists to be
    // updated properly.
    content::LoadBasicRequest(frame, url);
  }

 private:
  // Read a cookie via JavaScript.
  std::string JSReadCookie(Browser* browser) {
    return content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           "document.cookie")
        .ExtractString();
  }

  // Read a cookie with JavaScript cookie-store API
  std::string JSAsyncReadCookie(Browser* browser) {
    return content::EvalJs(
               browser->tab_strip_model()->GetActiveWebContents(),
               "async function doGet() {"
               "  const cookies = await window.cookieStore.getAll();"
               "  let cookie_str = '';"
               "  for (const cookie of cookies)"
               "    cookie_str += `${cookie.name}=${cookie.value};`;"
               "  return cookie_str;"
               "}"
               "doGet()")
        .ExtractString();
  }

  // Read a cookie by fetching the page url (which we should have just navigated
  // to) and checking what Cookie header (if any) it saw.
  std::string HttpReadCookie(Browser* browser) {
    GURL url = browser->tab_strip_model()
                   ->GetActiveWebContents()
                   ->GetLastCommittedURL();
    EXPECT_EQ(GetPageURL(), url);
    return HttpReadCookieWithURL(browser, url);
  }

  // Set a cookie with JavaScript.
  void JSWriteCookie(Browser* browser) {
    bool rv =
        content::ExecJs(browser->tab_strip_model()->GetActiveWebContents(),
                        "document.cookie = 'name=Good;Max-Age=3600'");
    CHECK(rv);
  }

  // Set a cookie with JavaScript cookie-store api.
  void JSAsyncWriteCookie(Browser* browser) {
    content::EvalJsResult result =
        content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                        "async function doSet() {"
                        "  await window.cookieStore.set("
                        "       { name: 'name',"
                        "         value: 'Good',"
                        "         expires: Date.now() + 3600*1000,"
                        "         sameSite: 'none' });"
                        "  return true;"
                        "}"
                        "doSet()");
    // Failure ignored here since some tests purposefully try to set disallowed
    // cookies.
  }

  // Set a cookie by visiting a page that has a Set-Cookie header.
  void HttpWriteCookie(Browser* browser) {
    HttpWriteCookieWithURL(browser, GetSetCookieURL());
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(cookies_seen_lock_);
    auto it = request.headers.find("Cookie");
    if (it != request.headers.end())
      cookies_seen_[request.GetURL()] = it->second;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  bool secure_scheme_ = false;
  base::Lock cookies_seen_lock_;
  std::map<GURL, std::string> cookies_seen_;
};

// Sanity check on cookies before we do other tests. While these can be written
// in content_browsertests, we want to verify Chrome's cookie storage and how it
// handles incognito windows.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, PRE_BasicCookies) {
  PreBasic();
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BasicCookies) {
  Basic();
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, PRE_BasicCookiesHttps) {
  set_secure_scheme();
  PreBasic();
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BasicCookiesHttps) {
  set_secure_scheme();
  Basic();
}

// Verify that cookies are being blocked.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, PRE_BlockCookies) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  WriteCookie(browser());
  ASSERT_TRUE(ReadCookie(browser()).empty());
  CookieCheckIncognitoWindow(false);
}

// Ensure that the setting persists.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BlockCookies) {
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            CookieSettingsFactory::GetForProfile(browser()->profile())
                ->GetDefaultCookieSetting());
}

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) when all others are blocked.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, AllowCookiesUsingExceptions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  content::CookieChangeObserver observer1(
      browser()->tab_strip_model()->GetActiveWebContents());

  WriteCookie(browser());
  ASSERT_TRUE(ReadCookie(browser()).empty());

  observer1.Wait();
  auto* allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  auto* blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
  net::CookieList blocked_cookies;
  ASSERT_EQ(0u, GetModelCookieCount(allowed_model));
  ASSERT_EQ(1u, GetModelCookieCount(blocked_model));
  blocked_cookies = ExtractCookiesFromModel(blocked_model);

  EXPECT_THAT(blocked_cookies, net::MatchesCookieLine("name=Good"));

  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_ALLOW);

  content::CookieChangeObserver observer2(
      browser()->tab_strip_model()->GetActiveWebContents());

  WriteCookie(browser());
  ASSERT_FALSE(ReadCookie(browser()).empty());

  observer2.Wait();
  net::CookieList accepted_cookies;
  allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
  ASSERT_EQ(GetModelCookieCount(allowed_model), 1u);
  // No navigation, so there should still be one blocked cookie.
  ASSERT_EQ(GetModelCookieCount(blocked_model), 1u);
  accepted_cookies = ExtractCookiesFromModel(allowed_model);

  EXPECT_THAT(accepted_cookies, net::MatchesCookieLine("name=Good"));
}

// Verify that cookies can be blocked for a specific website using exceptions.
//
// TODO(crbug.com/41440775): Re-enable test once flakiness is fixed.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest,
                       DISABLED_BlockCookiesUsingExceptions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  WriteCookie(browser());
  ASSERT_TRUE(ReadCookie(browser()).empty());

  auto* allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  auto* blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
  net::CookieList blocked_cookies;
  EXPECT_EQ(GetModelCookieCount(allowed_model), 0u);
  EXPECT_EQ(GetModelCookieCount(blocked_model), 1u);
  blocked_cookies = ExtractCookiesFromModel(blocked_model);

  EXPECT_THAT(blocked_cookies, net::MatchesCookieLine("name=Good"));

  GURL unblocked_url = GetOtherServer()->GetURL("/cookie1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), unblocked_url));
  ASSERT_FALSE(GetCookies(browser()->profile(), unblocked_url).empty());
  net::CookieList accepted_cookies;

  EXPECT_EQ(GetModelCookieCount(blocked_model), 0u);
  EXPECT_EQ(GetModelCookieCount(allowed_model), 1u);
  accepted_cookies = ExtractCookiesFromModel(allowed_model);

  EXPECT_THAT(accepted_cookies, net::MatchesCookieLine("foo=baz"));
}

// Test that cookies that are considered "blocked" are excluded only due to the
// content settings blocking (i.e. not for other reasons like domain or path not
// matching). See https://crbug.com/1104451.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest,
                       BlockedCookiesOnlyExcludedDueToBlocking) {
  // This test only runs in HTTP mode, not with the full parameterized test
  // suite.
  if (ReadMode() != CookieMode::kHttp || WriteMode() != CookieMode::kHttp)
    return;

  // URLs to get cookies from:
  GURL cookies_present_url(GetServer()->GetURL("a.test", "/simple.html"));
  GURL cookies_blocked_url(
      GetServer()->GetURL("sub.a.test", "/echoheader?Cookie"));

  // URLs to set cookies on:
  GURL set_host_cookie_url(
      GetServer()->GetURL("a.test", "/set-cookie?host_cookie=1"));
  GURL set_path_cookie_url(GetServer()->GetURL(
      "sub.a.test",
      "/set-cookie?path_cookie=1;domain=a.test;path=/simple.html"));
  GURL set_included_cookie_url(GetServer()->GetURL(
      "sub.a.test", "/set-cookie?included_cookie=1;domain=a.test"));

  // No cookies are present prior to setting them.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_present_url));
  ASSERT_TRUE(HttpReadCookieWithURL(browser(), cookies_present_url).empty());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_blocked_url));
  ASSERT_TRUE(HttpReadCookieWithURL(browser(), cookies_blocked_url).empty());

  // Set the cookies.
  HttpWriteCookieWithURL(browser(), set_host_cookie_url);
  HttpWriteCookieWithURL(browser(), set_path_cookie_url);
  HttpWriteCookieWithURL(browser(), set_included_cookie_url);

  // Verify all cookies are present on |cookies_present_url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_present_url));
  HttpReadCookieWithURL(browser(), cookies_present_url);
  auto* allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  auto* blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
  net::CookieList accepted_cookies;

  EXPECT_EQ(GetModelCookieCount(allowed_model), 3u);
  EXPECT_EQ(GetModelCookieCount(blocked_model), 0u);
  accepted_cookies = ExtractCookiesFromModel(allowed_model);

  EXPECT_THAT(accepted_cookies,
              net::MatchesCookieLine(
                  "host_cookie=1; included_cookie=1; path_cookie=1"));

  // Verify there is only one included cookie for |cookies_blocked_url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_blocked_url));
  HttpReadCookieWithURL(browser(), cookies_blocked_url);
  allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());

  EXPECT_EQ(GetModelCookieCount(allowed_model), 1u);
  EXPECT_EQ(GetModelCookieCount(blocked_model), 0u);
  accepted_cookies = ExtractCookiesFromModel(allowed_model);

  EXPECT_THAT(accepted_cookies, net::MatchesCookieLine("included_cookie=1"));

  // Set content settings to block cookies for |cookies_blocked_url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_blocked_url));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(cookies_blocked_url, CONTENT_SETTING_BLOCK);

  // Verify all cookies are still present on |cookies_present_url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_present_url));
  HttpReadCookieWithURL(browser(), cookies_present_url);
  allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());

  EXPECT_EQ(GetModelCookieCount(allowed_model), 3u);
  EXPECT_EQ(GetModelCookieCount(blocked_model), 0u);
  accepted_cookies = ExtractCookiesFromModel(allowed_model);

  EXPECT_THAT(accepted_cookies,
              net::MatchesCookieLine(
                  "host_cookie=1; included_cookie=1; path_cookie=1"));

  // Verify there is only one blocked cookie on |cookies_blocked_url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookies_blocked_url));
  allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
  blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
  net::CookieList blocked_cookies;
  EXPECT_EQ(GetModelCookieCount(allowed_model), 0u);
  EXPECT_EQ(GetModelCookieCount(blocked_model), 1u);
  blocked_cookies = ExtractCookiesFromModel(blocked_model);

  EXPECT_THAT(blocked_cookies, net::MatchesCookieLine("included_cookie=1"));
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BlockCookiesAlsoBlocksCacheStorage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  const char kBaseExpected[] =
      "%s - SecurityError: Failed to execute '%s' on 'CacheStorage': An "
      "attempt was made to break through the security "
      "policy of the user agent.";

  const char kBaseScript[] =
      "(async function() {"
      "  const name = `%s`;"
      "  try {"
      "    await %s;"
      "  } catch(e) {"
      "    return `${name} - ${e.toString()}`;"
      "  }"
      "  return `${name} - success`;"
      "}())";

  struct TestOp {
    const char* cmd;
    const char* name;
  };

  const TestOp kTestOps[] = {
      {.cmd = "caches.open('foo')", .name = "open"},
      {.cmd = "caches.has('foo')", .name = "has"},
      {.cmd = "caches.keys()", .name = "keys"},
      {.cmd = "caches.delete('foo')", .name = "delete"},
      {.cmd = "caches.match('/')", .name = "match"},
  };

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (auto& op : kTestOps) {
    EXPECT_EQ(EvalJs(tab, base::StringPrintf(kBaseScript, op.cmd, op.cmd)),
              base::StringPrintf(kBaseExpected, op.cmd, op.name));
  }
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BlockCookiesAlsoBlocksIndexedDB) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char kBaseScript[] =
      "(async function() {"
      "  const name = `%s`;"
      "  function wrap(req) {"
      "    return new Promise((resolve, reject) => {"
      "      req.onerror = function() { reject(req.error); };"
      "      req.onsuccess = function() { resolve(); };"
      "    });"
      "  }"
      "  try {"
      "    await wrap(indexedDB.%s%s);"
      "  } catch(e) {"
      "    return `${name} - ${e.toString()}`;"
      "  }"
      "  return `${name} - success`;"
      "}())";

  struct TestOp {
    const char* cmd;
    const char* args;
  };

  const TestOp kTestOps[] = {
      {.cmd = "open", .args = "('foo', 1)"},
      {.cmd = "deleteDatabase", .args = "('foo')"},
  };

  const char kBaseExpected[] =
      "%s - UnknownError: The user denied permission to access the database.";

  for (auto& op : kTestOps) {
    EXPECT_EQ(
        EvalJs(tab, base::StringPrintf(kBaseScript, op.cmd, op.cmd, op.args)),
        base::StringPrintf(kBaseExpected, op.cmd));
  }
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest,
                       BlockCookiesAlsoBlocksIndexedDBPromiseBased) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char kPromiseBaseScript[] =
      "(async function() {"
      "  const name = `%s`;"
      "  try {"
      "    await indexedDB.%s%s;"
      "  } catch(e) {"
      "    return `${name} - ${e.toString()}`;"
      "  }"
      "  return `${name} - success`;"
      "}())";

  struct TestOp {
    const char* cmd;
    const char* args;
  };

  const TestOp kPromiseTestOps[] = {
      {.cmd = "databases", .args = "()"},
  };

  const char kBaseExpected[] =
      "%s - UnknownError: The user denied permission to access the database.";

  for (auto& op : kPromiseTestOps) {
    EXPECT_EQ(EvalJs(tab, base::StringPrintf(kPromiseBaseScript, op.cmd, op.cmd,
                                             op.args)),
              base::StringPrintf(kBaseExpected, op.cmd));
  }
}

IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BlockCookiesAlsoBlocksFileSystem) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetPageURL()));
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  const char kBaseExpected[] = "%s - %s";

  const char kBaseScript[] = R"(
      (async function() {
        const name = `%s`;
        try {
          await %s;
        } catch(e) {
          return `${name} - ${e.toString()}`;
        }
        return `${name} - success`;
      }())
  )";

  struct TestOp {
    const char* name;
    const char* code;
    const char* error;
  };

  const TestOp kTestOps[] = {
      {.name = "navigator.storage.getDirectory()",
       .code = "navigator.storage.getDirectory()",
       .error = "SecurityError: Storage directory access is denied."},
      {.name = "window.webkitRequestFileSystem()",
       .code = "new Promise((resolve, reject) => { "
               " window.webkitRequestFileSystem(window.TEMPORARY,"
               " 5*1024, () => resolve(),"
               " (e) => reject(e));"
               "});",
       .error = "AbortError: An ongoing operation was aborted, typically with "
                "a call to abort()."},
  };

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (auto& op : kTestOps) {
    EXPECT_EQ(EvalJs(tab, base::StringPrintf(kBaseScript, op.name, op.code)),
              base::StringPrintf(kBaseExpected, op.name, op.error));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieSettingsTest,
    ::testing::Values(
        std::make_pair(CookieMode::kDocumentCookieJS,
                       CookieMode::kDocumentCookieJS),
        std::make_pair(CookieMode::kDocumentCookieJS, CookieMode::kHttp),
        std::make_pair(CookieMode::kHttp, CookieMode::kDocumentCookieJS),
        std::make_pair(CookieMode::kHttp, CookieMode::kHttp),
        std::make_pair(CookieMode::kHttp, CookieMode::kCookieStoreJS),
        std::make_pair(CookieMode::kCookieStoreJS,
                       CookieMode::kDocumentCookieJS)));

// This fails on ChromeOS because kRestoreOnStartup is ignored and the startup
// preference is always "continue where I left off.
#if !BUILDFLAG(IS_CHROMEOS)

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) only for a session when all others are blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       PRE_AllowCookiesForASessionUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(url, CONTENT_SETTING_SESSION_ONLY);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       AllowCookiesForASessionUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  // Cookies are shared between ports, so this will get cookies set in PRE.
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
}

#endif  // !CHROME_OS

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RedirectLoopCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/redirect-loop.html");

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MockWebContentsLoadFailObserver observer(web_contents);
  EXPECT_CALL(observer, DidFinishNavigation(IsErrorTooManyRedirects()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&observer));

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::COOKIES));
}

// Any cookie access during a navigation does not end up in a new document (e.g.
// due to the request returning HTTP 204) should not be tracked by the
// PageSpecificContentSettings.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, CookiesIgnoredFor204) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url =
      embedded_test_server()->GetURL("/server-redirect-with-cookie?/nocontent");

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  EXPECT_FALSE(PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::COOKIES));
}

class ContentSettingsBackForwardCacheBrowserTest : public ContentSettingsTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentSettingsTest ::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    ContentSettingsTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContentSettingsBackForwardCacheBrowserTest,
                       StateRestoredWhenNavigatingBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("a.com", "/setcookie.html");
  GURL other_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  content::CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  observer.Wait();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::COOKIES));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));
  EXPECT_EQ(main_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_FALSE(PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::COOKIES));

  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame(), main_frame);
  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::COOKIES));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsBackForwardCacheBrowserTest,
                       SettingsUpdateWhileInCacheShouldBeProcessed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("a.com", "/setcookie.html");
  GURL other_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  content::CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  observer.Wait();

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::COOKIES));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));
  EXPECT_FALSE(PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::COOKIES));

  // This triggers a OnContentSettingChanged notification that should be
  // processed by the page in the cache.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);

  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_FALSE(PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::COOKIES));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(u"Data URL", web_contents->GetTitle());

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
}

// Tests that if redirect across origins occurs, the new process still gets the
// content settings before the resource headers.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RedirectCrossOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  net::HostPortPair host_port = embedded_test_server()->host_port_pair();
  DCHECK_EQ(host_port.host(), std::string("127.0.0.1"));

  std::string redirect(base::StringPrintf(
      "http://localhost:%d/redirect-cross-origin.html", host_port.port()));
  GURL test_url =
      embedded_test_server()->GetURL("/server-redirect?" + redirect);

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::COOKIES));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, SendRendererContentRules) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url_2 =
      embedded_test_server()->GetURL("b.com", "/javaScriptTitle.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_1));
  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  EXPECT_NE(map, nullptr);
  EXPECT_FALSE(PageSpecificContentSettings::GetForFrame(
                   web_contents->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  map->SetContentSettingDefaultScope(url_2, url_2,
                                     ContentSettingsType::JAVASCRIPT,
                                     ContentSetting::CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       SpareRenderProcessHostRulesAreUpdated) {
  // Make sure a spare RenderProcessHost exists during the test.
  content::SpareRenderProcessHostManager::Get().WarmupSpare(
      browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  // URL to a page that loads a cross-site iframe which creates another iframe
  // via JavaScript. We will count iframes to test if JavaScript is blocked or
  // not.
  const GURL url = embedded_test_server()->GetURL(
      "a.test", "/iframe_cross_site_with_script.html");

  // Disable JavaScript. A warmed-up spare renderer should get ContentSettings
  // updates and disable JavaScript.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  // Navigate to the page.
  content::RenderFrameHost* main_frame =
      ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(main_frame);
  // Ensure 2 frames exist after the load (main frame and 'b.test' frame).
  EXPECT_EQ(GetRenderFrameHostCount(main_frame), 2u);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, NonMainFrameRulesAreUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // URL to a page that loads a cross-site iframe which creates another iframe
  // via JavaScript. We will count iframes to test if JavaScript is blocked or
  // not.
  const GURL url = embedded_test_server()->GetURL(
      "a.test", "/iframe_cross_site_with_script.html");

  // Disable JavaScript.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  // Navigate to the page.
  content::RenderFrameHost* main_frame =
      ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(main_frame);
  // Ensure 2 frames exist after the load (main frame and 'b.test' frame).
  EXPECT_EQ(GetRenderFrameHostCount(main_frame), 2u);

  // Enable JavaScript and load the same page.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_DEFAULT);
  main_frame = ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(main_frame);
  // Ensure 3 frames exist after the load (main frame, 'b.test' frame and
  // JavaScript-created 'b.test' nested frame).
  EXPECT_EQ(GetRenderFrameHostCount(main_frame), 3u);

  // Disable JavaScript and reload the iframe which contains JavaScript.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  content::RenderFrameHost* iframe_to_reload =
      content::ChildFrameAt(main_frame, 0);
  content::TestFrameNavigationObserver iframe_nav_observer(iframe_to_reload);
  iframe_to_reload->Reload();
  iframe_nav_observer.Wait();

  // Ensure 2 frames exist after iframe reload (main frame and 'b.test' frame).
  EXPECT_EQ(GetRenderFrameHostCount(main_frame), 2u);
}

// Simulates script being blocked in the renderer and notifying the browser
// before DidCommitNavigation is sent to the browser (i.e. while the RFH is
// still pending commit).
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RendererUpdateWhilePendingCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  const GURL second_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::CommitMessageDelayer delayer(
      web_contents, second_url,
      base::BindOnce([](content::RenderFrameHost* rfh) {
        auto global_frame_token = rfh->GetGlobalFrameToken();
        // Call ContentBlocked while the RFH is pending commit.
        PageSpecificContentSettings::ContentBlocked(
            global_frame_token, ContentSettingsType::JAVASCRIPT);
      }));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), second_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  delayer.Wait();

  EXPECT_TRUE(PageSpecificContentSettings::GetForFrame(
                  web_contents->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
}

// This test verifies that the site settings accurately reflect that an attempt
// to create a secure cookie by an insecure origin fails.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, SecureCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL http_url =
      embedded_test_server()->GetURL("a.test", "/setsecurecookie.html");
  GURL https_url = https_server.GetURL("a.test", "/setsecurecookie.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  EXPECT_EQ(
      GetModelCookieCount(GetSiteSettingsAllowedBrowsingDataModel(browser())),
      0u);

  content::CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));
  observer.Wait();

  EXPECT_EQ(
      GetModelCookieCount(GetSiteSettingsAllowedBrowsingDataModel(browser())),
      1u);
}

class ContentSettingsWorkerModulesBrowserTest : public ContentSettingsTest {
 public:
  ContentSettingsWorkerModulesBrowserTest() = default;

  ContentSettingsWorkerModulesBrowserTest(
      const ContentSettingsWorkerModulesBrowserTest&) = delete;
  ContentSettingsWorkerModulesBrowserTest& operator=(
      const ContentSettingsWorkerModulesBrowserTest&) = delete;

  ~ContentSettingsWorkerModulesBrowserTest() override = default;

 protected:
  void RegisterStaticFile(net::EmbeddedTestServer* server,
                          const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type) {
    server->RegisterRequestHandler(base::BindRepeating(
        &ContentSettingsWorkerModulesBrowserTest::StaticRequestHandler,
        base::Unretained(this), relative_url, content, content_type));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> StaticRequestHandler(
      const std::string& relative_url,
      const std::string& content,
      const std::string& content_type,
      const net::test_server::HttpRequest& request) const {
    if (request.relative_url != relative_url)
      return nullptr;
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        std::make_unique<net::test_server::BasicHttpResponse>());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(content);
    http_response->set_content_type(content_type);
    return std::move(http_response);
  }
};

IN_PROC_BROWSER_TEST_F(ContentSettingsWorkerModulesBrowserTest,
                       WorkerImportModule) {
  // This test uses 2 servers, |https_server_| and |embedded_test_server|.
  // These 3 files are served from them:
  //   - "worker_import_module.html" from |embedded_test_server|.
  //   - "worker_import_module_worker.js" from |embedded_test_server|.
  //   - "worker_import_module_imported.js" from |https_server_|.
  // 1. worker_import_module.html starts a dedicated worker which type is
  //    'module' using worker_import_module_worker.js.
  //      new Worker('worker_import_module_worker.js', { type: 'module' })
  // 2. worker_import_module_worker.js imports worker_import_module_imported.js.
  //    - If succeeded to import, calls postMessage() with the exported |msg|
  //      constant value which is 'Imported'.
  //    - If failed, calls postMessage('Failed').
  // 3. When the page receives the message from the worker, change the title
  //    to the message string.
  //      worker.onmessage = (event) => { document.title = event.data; };
  ASSERT_TRUE(https_server_.Start());
  GURL module_url = https_server_.GetURL("/worker_import_module_imported.js");

  const std::string script = base::StringPrintf(
      "import('%s')\n"
      "  .then(module => postMessage(module.msg), _ => postMessage('Failed'));",
      module_url.spec().c_str());
  RegisterStaticFile(embedded_test_server(), "/worker_import_module_worker.js",
                     script, "text/javascript");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_url = embedded_test_server()->GetURL("/worker_import_module.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::u16string expected_title(u"Imported");
  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(u"Failed");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));

  // The import must be executed successfully.
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsWorkerModulesBrowserTest, CookieStore) {
  const char kWorkerScript[] = R"(
      async function cookieHandler(e) {
        try {
          await cookieStore.set(
              { name: e.data,
                value: 'value',
                expires: Date.now() + 3600*1000,
                sameSite: 'none' });
        } finally {
          e.source.postMessage('set executed for ' + e.data);
        }
      }
      self.addEventListener('message', cookieHandler);)";

  RegisterStaticFile(&https_server_, "/sw.js", kWorkerScript,
                     "text/javascript");

  ASSERT_TRUE(https_server_.Start());

  // Install service worker and wait for it to be activated.
  GURL setup_url =
      https_server_.GetURL("/service_worker/create_service_worker.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), setup_url));
  content::EvalJsResult result =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "register('/sw.js')");
  ASSERT_EQ("DONE", result);

  // Navigate again, this time it should be active. Also add some JS helpers to
  // message the service worker asking it to set cookies.
  GURL page_url = https_server_.GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  const char kClientScript[] = R"(
      function requestCookieSet(name) {
        return new Promise(resolve => {
          navigator.serviceWorker.onmessage = e => {
            resolve(e.data);
          };
          window.sw.postMessage(name);
        });
      }
      async function lookupSw() {
        const reg = await navigator.serviceWorker.ready;
        window.sw = reg.active;
        return !!window.sw;
      }
      lookupSw();)";

  content::EvalJsResult result2 = content::EvalJs(
      browser()->tab_strip_model()->GetActiveWebContents(), kClientScript);
  EXPECT_EQ(result2, true);

  {
    content::CookieChangeObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    // Set a cookie, see that it's reported.
    content::EvalJsResult result3 =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "requestCookieSet('first')");
    EXPECT_EQ(result3, "set executed for first");
    observer.Wait();

    auto* allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
    auto* blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());

    net::CookieList accepted_cookies;
    EXPECT_EQ(GetModelCookieCount(allowed_model), 1u);
    EXPECT_EQ(GetModelCookieCount(blocked_model), 0u);
    accepted_cookies = ExtractCookiesFromModel(allowed_model);

    EXPECT_THAT(accepted_cookies, net::MatchesCookieLine("first=value"));
  }

  {
    content::CookieChangeObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    // Now set with cookies blocked.
    content_settings::CookieSettings* settings =
        CookieSettingsFactory::GetForProfile(browser()->profile()).get();
    settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
    content::EvalJsResult result4 =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "requestCookieSet('second')");
    EXPECT_EQ(result4, "set executed for second");
    observer.Wait();

    auto* allowed_model = GetSiteSettingsAllowedBrowsingDataModel(browser());
    auto* blocked_model = GetSiteSettingsBlockedBrowsingDataModel(browser());
    net::CookieList blocked_cookies;
    EXPECT_EQ(GetModelCookieCount(allowed_model), 1u);
    EXPECT_EQ(GetModelCookieCount(blocked_model), 1u);
    blocked_cookies = ExtractCookiesFromModel(blocked_model);

    EXPECT_THAT(blocked_cookies, net::MatchesCookieLine("second=value"));
  }
}

class ContentSettingsWithPrerenderingBrowserTest : public ContentSettingsTest {
 public:
  ContentSettingsWithPrerenderingBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &ContentSettingsWithPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_test_helper().RegisterServerRequestMonitor(
        embedded_test_server());
    ContentSettingsTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentSettingsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

// Used to wait for non-primary pages to set a cookie (eg: prerendering pages or
// fenced frames).
class NonPrimaryPageCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit NonPrimaryPageCookieAccessObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~NonPrimaryPageCookieAccessObserver() override = default;

  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override {
    bool is_in_primary_frame =
        navigation_handle->GetParentFrame()
            ? navigation_handle->GetParentFrame()->GetPage().IsPrimary()
            : navigation_handle->IsInPrimaryMainFrame();
    if (is_in_primary_frame) {
      cookie_accessed_in_primary_page_ = true;
    } else {
      run_loop_.Quit();
    }
  }

  void OnCookiesAccessed(content::RenderFrameHost* rfh,
                         const content::CookieAccessDetails& details) override {
    if (rfh->GetPage().IsPrimary()) {
      cookie_accessed_in_primary_page_ = true;
    } else {
      run_loop_.Quit();
    }
  }

  bool CookieAccessedByPrimaryPage() const {
    return cookie_accessed_in_primary_page_;
  }

  // Waits for the prerendering page to set a cookie.
  void Wait() { run_loop_.Run(); }

 private:
  bool cookie_accessed_in_primary_page_ = false;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(ContentSettingsWithPrerenderingBrowserTest,
                       PrerenderingPageSetsCookie) {
  const GURL main_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url =
      embedded_test_server()->GetURL("/set_cookie_header.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_EQ(GetWebContents()->GetLastCommittedURL(), main_url);
  auto* main_pscs = PageSpecificContentSettings::GetForFrame(
      GetWebContents()->GetPrimaryMainFrame());
  ASSERT_FALSE(main_pscs->IsContentAllowed(ContentSettingsType::COOKIES));

  {
    NonPrimaryPageCookieAccessObserver cookie_observer(GetWebContents());
    prerender_test_helper().AddPrerender(prerender_url);
    content::FrameTreeNodeId host_id =
        prerender_test_helper().GetHostForUrl(prerender_url);
    content::RenderFrameHost* prerender_frame =
        prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
    EXPECT_NE(prerender_frame, nullptr);
    // Ensure notification for cookie access by prerendering page has been sent.
    cookie_observer.Wait();

    auto* prerender_pscs =
        PageSpecificContentSettings::GetForFrame(prerender_frame);
    EXPECT_TRUE(prerender_pscs->IsContentAllowed(ContentSettingsType::COOKIES));
    EXPECT_EQ(
        GetModelCookieCount(prerender_pscs->allowed_browsing_data_model()), 1u);
    // Between when the cookie was set by the prerendering page and now, the
    // main page might have accessed the cookie (for instance, when sending a
    // request for a favicon) - check for the appropriate value based on
    // observed behavior.
    EXPECT_EQ(GetModelCookieCount(main_pscs->allowed_browsing_data_model()),
              cookie_observer.CookieAccessedByPrimaryPage() ? 1u : 0u);
  }

  prerender_test_helper().NavigatePrimaryPage(prerender_url);

  main_pscs = PageSpecificContentSettings::GetForFrame(
      GetWebContents()->GetPrimaryMainFrame());
  EXPECT_TRUE(main_pscs->IsContentAllowed(ContentSettingsType::COOKIES));
  EXPECT_EQ(GetModelCookieCount(main_pscs->allowed_browsing_data_model()), 1u);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsWithPrerenderingBrowserTest,
                       PrerenderingPageIframeSetsCookie) {
  const GURL main_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  const GURL iframe_url =
      embedded_test_server()->GetURL("/set_cookie_header.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_EQ(GetWebContents()->GetLastCommittedURL(), main_url);

  auto* main_pscs = PageSpecificContentSettings::GetForFrame(
      GetWebContents()->GetPrimaryMainFrame());
  ASSERT_FALSE(main_pscs->IsContentAllowed(ContentSettingsType::COOKIES));

  prerender_test_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  content::TestNavigationManager navigation_manager(GetWebContents(),
                                                    iframe_url);
  NonPrimaryPageCookieAccessObserver cookie_observer(GetWebContents());
  EXPECT_TRUE(content::ExecJs(
      prerender_frame,
      content::JsReplace("const iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         iframe_url)));
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();
  cookie_observer.Wait();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  auto* prerender_pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  EXPECT_TRUE(prerender_pscs->IsContentAllowed(ContentSettingsType::COOKIES));
  EXPECT_EQ(GetModelCookieCount(prerender_pscs->allowed_browsing_data_model()),
            1u);

  // Between when the cookie was set by the prerendering page and now, the
  // main page might have accessed the cookie (for instance, when sending a
  // request for a favicon) - check for the appropriate value based on
  // observed behavior.
  EXPECT_EQ(GetModelCookieCount(main_pscs->allowed_browsing_data_model()),
            cookie_observer.CookieAccessedByPrimaryPage() ? 1u : 0u);
}

class ContentSettingsWithFencedFrameBrowserTest : public ContentSettingsTest {
 public:
  ContentSettingsWithFencedFrameBrowserTest() = default;
  ~ContentSettingsWithFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentSettingsTest::SetUpOnMainThread();
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    ASSERT_TRUE(https_server_.Start());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(ContentSettingsWithFencedFrameBrowserTest,
                       StorageAccessInFencedFrame) {
  const GURL main_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL fenced_frame_url =
      https_server_.GetURL("b.test", "/browsing_data/site_data.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_FALSE(GetWebContents()->GetPrimaryMainFrame()->IsErrorDocument());
  ASSERT_EQ(GetWebContents()->GetLastCommittedURL(), main_url);
  auto* main_pscs = PageSpecificContentSettings::GetForFrame(
      GetWebContents()->GetPrimaryMainFrame());

  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_NE(fenced_frame, nullptr);
  auto* ff_pscs = PageSpecificContentSettings::GetForFrame(fenced_frame);

  std::vector<std::string> storage_types_to_test{
      "LocalStorage",     "SessionStorage", "CacheStorage", "FileSystem",
      "FileSystemAccess", "IndexedDb",      "SharedWorker", "ServiceWorker"};
  for (auto storage_type : storage_types_to_test) {
    EXPECT_TRUE(content::EvalJs(fenced_frame, "set" + storage_type + "();")
                    .ExtractBool());
  }

  std::vector<PageSpecificContentSettings*> pscs_list;
  pscs_list.push_back(main_pscs);
  pscs_list.push_back(ff_pscs);

  for (auto* pscs : pscs_list) {
    EXPECT_TRUE(pscs->IsContentAllowed(ContentSettingsType::COOKIES));
    EXPECT_EQ(pscs->allowed_browsing_data_model()->size(), 1u);
  }
}

IN_PROC_BROWSER_TEST_F(ContentSettingsWithFencedFrameBrowserTest,
                       RendererContentSettings) {
  const GURL main_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL fenced_frame_url =
      https_server_.GetURL("b.test", "/fenced_frames/page_with_script.html");

  auto NavigatePrimaryPageAndAddFencedFrame =
      [&]() -> content::RenderFrameHost* {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
    EXPECT_FALSE(GetWebContents()->GetPrimaryMainFrame()->IsErrorDocument());
    EXPECT_EQ(GetWebContents()->GetLastCommittedURL(), main_url);
    content::RenderFrameHost* fenced_frame =
        fenced_frame_test_helper().CreateFencedFrame(
            GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
    EXPECT_NE(fenced_frame, nullptr);
    return fenced_frame;
  };

  auto ExpectScriptAllowed = [&](content::RenderFrameHost* fenced_frame) {
    EXPECT_EQ(EvalJs(fenced_frame, "(async () => { return 1; })();"), 1);
    auto* ff_pscs = PageSpecificContentSettings::GetForFrame(fenced_frame);
    EXPECT_FALSE(ff_pscs->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  };

  // There is no test to write for the case where script is blocked in a.test
  // because FencedFrames cannot be loaded without javascript since they do not
  // take a src attribute.
  //
  // Block script in b.test.
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(fenced_frame_url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::JAVASCRIPT, ContentSetting::CONTENT_SETTING_BLOCK);
  content::RenderFrameHost* fenced_frame =
      NavigatePrimaryPageAndAddFencedFrame();

  // Content settings are always determined by the outermost URL, which must be
  // allowed in order to create the fenced frame.
  ExpectScriptAllowed(fenced_frame);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsWithFencedFrameBrowserTest,
                       FencedFrameSetsCookie) {
  const GURL main_url = https_server_.GetURL("a.test", "/empty.html");
  const GURL fenced_frame_url =
      https_server_.GetURL("b.test", "/fenced_frames/set_cookie_header.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_EQ(GetWebContents()->GetLastCommittedURL(), main_url);
  auto* main_pscs = PageSpecificContentSettings::GetForFrame(
      GetWebContents()->GetPrimaryMainFrame());
  ASSERT_FALSE(main_pscs->IsContentAllowed(ContentSettingsType::COOKIES));

  std::unique_ptr<content::RenderFrameHostWrapper> fenced_frame;
  {
    NonPrimaryPageCookieAccessObserver cookie_observer(GetWebContents());
    fenced_frame = std::make_unique<content::RenderFrameHostWrapper>(
        fenced_frame_test_helper().CreateFencedFrame(
            GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
    EXPECT_NE(fenced_frame, nullptr);
    // Ensure notification for cookie access by fenced frame has been sent.
    cookie_observer.Wait();
  }

  auto* ff_pscs = PageSpecificContentSettings::GetForFrame(fenced_frame->get());
  EXPECT_TRUE(ff_pscs->IsContentAllowed(ContentSettingsType::COOKIES));
  EXPECT_TRUE(main_pscs->IsContentAllowed(ContentSettingsType::COOKIES));
  EXPECT_EQ(GetModelCookieCount(main_pscs->allowed_browsing_data_model()), 1u);
  EXPECT_EQ(GetModelCookieCount(ff_pscs->allowed_browsing_data_model()), 1u);

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting((*fenced_frame)->GetLastCommittedURL(),
                         CONTENT_SETTING_BLOCK);
  {
    NonPrimaryPageCookieAccessObserver cookie_observer(GetWebContents());
    ASSERT_TRUE(content::ExecJs(fenced_frame->get(), "document.cookie"));
    cookie_observer.Wait();
  }

  EXPECT_TRUE(ff_pscs->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_TRUE(main_pscs->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_EQ(GetModelCookieCount(main_pscs->blocked_browsing_data_model()), 1u);
  EXPECT_EQ(GetModelCookieCount(ff_pscs->blocked_browsing_data_model()), 1u);

  ASSERT_TRUE(
      content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                      "const ff = document.querySelector('fencedframe'); "
                      "ff.remove();"));
  ASSERT_TRUE(fenced_frame->WaitUntilRenderFrameDeleted());

  EXPECT_TRUE(main_pscs->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_EQ(GetModelCookieCount(main_pscs->allowed_browsing_data_model()), 1u);
  EXPECT_EQ(GetModelCookieCount(main_pscs->blocked_browsing_data_model()), 1u);
}

class ContentSettingsWorkerModulesWithFencedFrameBrowserTest
    : public ContentSettingsWorkerModulesBrowserTest {
 public:
  ContentSettingsWorkerModulesWithFencedFrameBrowserTest() = default;
  ~ContentSettingsWorkerModulesWithFencedFrameBrowserTest() override = default;
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

#if BUILDFLAG(ENABLE_PDF)
class ContentSettingsPdfTest : public PDFExtensionTestBase {
 public:
  bool UseOopif() const override { return true; }

  testing::AssertionResult IsJavaScriptEnabled(content::RenderFrameHost* host) {
    return content::ExecJs(
        host, "",
        content::EvalJsOptions::EXECUTE_SCRIPT_HONOR_JS_CONTENT_SETTINGS);
  }
};

// Test that only PDF frames are allowed to use JavaScript.
IN_PROC_BROWSER_TEST_F(ContentSettingsPdfTest, JavaScriptAllowedForPdfFrames) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  content::RenderFrameHost* extension_host =
      LoadPdfGetExtensionHost(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(extension_host);

  // Arbitrary frames shouldn't be able to execute JavaScript.
  EXPECT_FALSE(
      IsJavaScriptEnabled(GetActiveWebContents()->GetPrimaryMainFrame()));

  // The PDF extension frame should be able to execute JavaScript.
  EXPECT_TRUE(IsJavaScriptEnabled(extension_host));

  content::RenderFrameHost* content_host =
      pdf_frame_util::FindPdfChildFrame(extension_host);
  ASSERT_TRUE(content_host);

  // The PDF content frame should be able to execute JavaScript.
  EXPECT_TRUE(IsJavaScriptEnabled(content_host));
}

#endif  // BUILDFLAG(ENABLE_PDF)
