// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using content::BrowserThread;
using net::URLRequestMockHTTPJob;

namespace {

const LocalSharedObjectsContainer* GetSiteSettingsCookieContainer(
    Browser* browser) {
  TabSpecificContentSettings* settings =
      TabSpecificContentSettings::FromWebContents(
          browser->tab_strip_model()->GetWebContentsAt(0));
  return static_cast<const LocalSharedObjectsContainer*>(
      &settings->allowed_local_shared_objects());
}

class MockWebContentsLoadFailObserver : public content::WebContentsObserver {
 public:
  explicit MockWebContentsLoadFailObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~MockWebContentsLoadFailObserver() override {}

  MOCK_METHOD1(DidFinishNavigation,
               void(content::NavigationHandle* navigation_handle));
};

MATCHER(IsErrorTooManyRedirects, "") {
  return arg->GetNetErrorCode() == net::ERR_TOO_MANY_REDIRECTS;
}

}  // namespace

class ContentSettingsTest : public InProcessBrowserTest {
 public:
  ContentSettingsTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
  }

  void SetUpOnMainThread() override {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  net::EmbeddedTestServer https_server_;
};

// Test the combination of different ways of accessing cookies.
// Cookies can be read by JS or http request headers.
// Cookies can be written by JS or http response headers.
enum class CookieMode {
  kJSReadJSWrite,
  kJSReadHttpWrite,
  kHttpReadJSWrite,
  kHttpReadHttpWrite
};

class CookieSettingsTest : public ContentSettingsTest,
                           public testing::WithParamInterface<CookieMode> {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &CookieSettingsTest::MonitorRequest, base::Unretained(this)));
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &CookieSettingsTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(https_server_.Start());
  }

  void set_secure_scheme() { secure_scheme_ = true; }

  std::string ReadCookie(Browser* browser) {
    if (GetParam() == CookieMode::kJSReadJSWrite ||
        GetParam() == CookieMode::kJSReadHttpWrite) {
      return JSReadCookie(browser);
    }

    return HttpReadCookie(browser);
  }

  void WriteCookie(Browser* browser) {
    if (GetParam() == CookieMode::kJSReadJSWrite ||
        GetParam() == CookieMode::kHttpReadJSWrite) {
      return JSWriteCookie(browser);
    }

    return HttpWriteCookie(browser);
  }

  // Check the cookie in an incognito window.
  void CookieCheckIncognitoWindow(bool cookies_enabled) {
    ASSERT_TRUE(ReadCookie(browser()).empty());

    Browser* incognito = CreateIncognitoBrowser();
    ui_test_utils::NavigateToURL(incognito, GetPageURL());
    ASSERT_TRUE(ReadCookie(incognito).empty());
    WriteCookie(incognito);
    ASSERT_EQ(cookies_enabled, !ReadCookie(incognito).empty());

    // Ensure incognito cookies don't leak to regular profile.
    ASSERT_TRUE(ReadCookie(browser()).empty());

    // Ensure cookies get wiped after last incognito window closes.
    CloseBrowserSynchronously(incognito);

    incognito = CreateIncognitoBrowser();
    ui_test_utils::NavigateToURL(incognito, GetPageURL());
    ASSERT_TRUE(ReadCookie(incognito).empty());
    CloseBrowserSynchronously(incognito);
  }

  void PreBasic() {
    ui_test_utils::NavigateToURL(browser(), GetPageURL());
    ASSERT_TRUE(ReadCookie(browser()).empty());

    CookieCheckIncognitoWindow(true);

    WriteCookie(browser());
    ASSERT_FALSE(ReadCookie(browser()).empty());
  }

  void Basic() {
    ui_test_utils::NavigateToURL(browser(), GetPageURL());
    ASSERT_FALSE(ReadCookie(browser()).empty());
  }

  GURL GetPageURL() { return GetServer()->GetURL("/simple.html"); }

  GURL GetSetCookieURL() {
    return GetServer()->GetURL("/set_cookie_header.html");
  }

 private:
  net::EmbeddedTestServer* GetServer() {
    return secure_scheme_ ? &https_server_ : embedded_test_server();
  }

  // Read a cookie via JavaScript.
  std::string JSReadCookie(Browser* browser) {
    std::string cookies;
    bool rv = content::ExecuteScriptAndExtractString(
        browser->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.cookie)", &cookies);
    CHECK(rv);
    return cookies;
  }

  // Read a cookie by fetching a url on the appropriate test server and checking
  // what Cookie header (if any) it saw.
  std::string HttpReadCookie(Browser* browser) {
    {
      base::AutoLock auto_lock(cookies_seen_lock_);
      cookies_seen_.clear();
    }

    auto* network_context =
        content::BrowserContext::GetDefaultStoragePartition(browser->profile())
            ->GetNetworkContext();
    content::LoadBasicRequest(network_context, browser->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetLastCommittedURL());

    {
      base::AutoLock auto_lock(cookies_seen_lock_);
      return cookies_seen_[GetPageURL()];
    }
  }

  // Set a cookie with JavaScript.
  void JSWriteCookie(Browser* browser) {
    bool rv = content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(),
        "document.cookie = 'name=Good;Max-Age=3600'");
    CHECK(rv);
  }

  // Set a cookie by visiting a page that has a Set-Cookie header.
  void HttpWriteCookie(Browser* browser) {
    auto* network_context =
        content::BrowserContext::GetDefaultStoragePartition(browser->profile())
            ->GetNetworkContext();
    content::LoadBasicRequest(network_context, GetSetCookieURL());
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(cookies_seen_lock_);
    auto it = request.headers.find("Cookie");
    if (it != request.headers.end())
      cookies_seen_[request.GetURL()] = it->second;
  }

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
  ui_test_utils::NavigateToURL(browser(), GetPageURL());
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
                ->GetDefaultCookieSetting(NULL));
}

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) when all others are blocked.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, AllowCookiesUsingExceptions) {
  ui_test_utils::NavigateToURL(browser(), GetPageURL());
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  WriteCookie(browser());
  ASSERT_TRUE(ReadCookie(browser()).empty());

  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_ALLOW);

  WriteCookie(browser());
  ASSERT_FALSE(ReadCookie(browser()).empty());
}

// Verify that cookies can be blocked for a specific website using exceptions.
IN_PROC_BROWSER_TEST_P(CookieSettingsTest, BlockCookiesUsingExceptions) {
  ui_test_utils::NavigateToURL(browser(), GetPageURL());
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GetPageURL(), CONTENT_SETTING_BLOCK);

  WriteCookie(browser());
  ASSERT_TRUE(ReadCookie(browser()).empty());

  GURL unblocked_url = https_server_.GetURL("/cookie1.html");

  ui_test_utils::NavigateToURL(browser(), unblocked_url);
  ASSERT_FALSE(GetCookies(browser()->profile(), unblocked_url).empty());
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    CookieSettingsTest,
    ::testing::Values(CookieMode::kJSReadJSWrite,
                      CookieMode::kJSReadHttpWrite,
                      CookieMode::kHttpReadJSWrite,
                      CookieMode::kHttpReadHttpWrite));

// This fails on ChromeOS because kRestoreOnStartup is ignored and the startup
// preference is always "continue where I left off.
#if !defined(OS_CHROMEOS)

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) only for a session when all others are blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       PRE_AllowCookiesForASessionUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(url, CONTENT_SETTING_SESSION_ONLY);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       AllowCookiesForASessionUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  // Cookies are shared between ports, so this will get cookies set in PRE.
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
}

#endif // !CHROME_OS

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

  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&observer));

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

// TODO(jww): This should be removed after strict secure cookies is enabled for
// all and this test should be moved into ContentSettingsTest above.
class ContentSettingsStrictSecureCookiesBrowserTest
    : public ContentSettingsTest {
 public:
  ContentSettingsStrictSecureCookiesBrowserTest() = default;
  ~ContentSettingsStrictSecureCookiesBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    ContentSettingsTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This test verifies that if strict secure cookies is enabled, the site
// settings accurately reflect that an attempt to create a secure cookie by an
// insecure origin fails.
IN_PROC_BROWSER_TEST_F(ContentSettingsStrictSecureCookiesBrowserTest, Cookies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL http_url = embedded_test_server()->GetURL("/setsecurecookie.html");
  GURL https_url = https_server.GetURL("/setsecurecookie.html");

  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_TRUE(GetSiteSettingsCookieContainer(browser())->cookies()->empty());

  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/setsecurecookie.html"));
  EXPECT_FALSE(GetSiteSettingsCookieContainer(browser())->cookies()->empty());
};

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(base::UTF8ToUTF16("Data URL"), web_contents->GetTitle());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
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

  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

class ContentSettingsWorkerModulesBrowserTest : public ContentSettingsTest {
 public:
  ContentSettingsWorkerModulesBrowserTest() = default;
  ~ContentSettingsWorkerModulesBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Module scripts on Dedicated Worker is still an experimental feature.
    // TODO(crbug/680046): Remove this after shipping.
    cmd->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

 protected:
  void RegisterStaticFile(const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
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
      return std::unique_ptr<net::test_server::HttpResponse>();
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        std::make_unique<net::test_server::BasicHttpResponse>());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(content);
    http_response->set_content_type(content_type);
    return std::move(http_response);
  }

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsWorkerModulesBrowserTest);
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
  RegisterStaticFile("/worker_import_module_worker.js", script,
                     "text/javascript");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_url = embedded_test_server()->GetURL("/worker_import_module.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::string16 expected_title(base::ASCIIToUTF16("Imported"));
  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Failed"));

  ui_test_utils::NavigateToURL(browser(), http_url);

  // The import must be executed successfully.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsWorkerModulesBrowserTest,
                       WorkerImportModuleBlocked) {
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
  RegisterStaticFile("/worker_import_module_worker.js", script,
                     "text/javascript");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_url = embedded_test_server()->GetURL("/worker_import_module.html");

  // Change the settings to blocks the script loading of
  // worker_import_module_imported.js from worker_import_module.html.
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings_map->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(http_url),
      ContentSettingsPattern::FromURLNoWildcard(module_url),
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

  content::WindowedNotificationObserver javascript_content_blocked_observer(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      base::BindRepeating(&TabSpecificContentSettings::IsContentBlocked,
                          base::Unretained(tab_settings),
                          CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  base::string16 expected_title(base::ASCIIToUTF16("Failed"));
  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Imported"));

  ui_test_utils::NavigateToURL(browser(), http_url);

  // The import must be blocked.
  javascript_content_blocked_observer.Wait();
  EXPECT_TRUE(tab_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#if BUILDFLAG(ENABLE_PLUGINS)
class PepperContentSettingsSpecialCasesTest : public ContentSettingsTest {
 protected:
  void SetUpOnMainThread() override {
    ContentSettingsTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    // Plugin throttling is irrelevant to this test and just makes it harder to
    // verify if a test Flash plugin loads successfully.
    command_line->AppendSwitchASCII(
        switches::kOverridePluginPowerSaverForTesting, "never");

#if BUILDFLAG(ENABLE_NACL)
    // Ensure NaCl can run.
    command_line->AppendSwitch(switches::kEnableNaCl);
#endif
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) && BUILDFLAG(ENABLE_WIDEVINE)
  // Since the CDM is bundled and registered through the component updater,
  // we must re-enable the component updater.
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) && BUILDFLAG(ENABLE_WIDEVINE)

  void RunLoadPepperPluginTest(const char* mime_type, bool expect_loaded) {
    const char* expected_result = expect_loaded ? "Loaded" : "Not Loaded";
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::string16 expected_title(base::ASCIIToUTF16(expected_result));
    content::TitleWatcher title_watcher(web_contents, expected_title);

    GURL url(https_server_.GetURL("/load_pepper_plugin.html").spec() +
             base::StringPrintf("?mimetype=%s", mime_type));
    ui_test_utils::NavigateToURL(browser(), url);

    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    EXPECT_EQ(!expect_loaded,
              TabSpecificContentSettings::FromWebContents(web_contents)->
                  IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  }

  void RunJavaScriptBlockedTest(const char* path,
                                bool expect_is_javascript_content_blocked) {
    // Because JavaScript is blocked, <title> will be the only title set.
    // Checking for it ensures that the page loaded, though that is not always
    // sufficient - see below.
    const char* const kExpectedTitle = "Initial Title";
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabSpecificContentSettings* tab_settings =
        TabSpecificContentSettings::FromWebContents(web_contents);
    base::string16 expected_title(base::ASCIIToUTF16(kExpectedTitle));
    content::TitleWatcher title_watcher(web_contents, expected_title);

    // Because JavaScript is blocked, we cannot rely on JavaScript to set a
    // title, telling us the test is complete.
    // As a result, it is possible to reach the IsContentBlocked() checks below
    // before the blocked content can be reported to the browser process.
    // See http://crbug.com/306702.
    // Therefore, when expecting blocked content, we must wait until it has been
    // reported by checking IsContentBlocked() when notified that
    // NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED. (It is not sufficient to wait
    // for just the notification because the same notification is reported for
    // other reasons and the notification contains no indication of what
    // caused it.)
    content::WindowedNotificationObserver javascript_content_blocked_observer(
              chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
              base::Bind(&TabSpecificContentSettings::IsContentBlocked,
                                   base::Unretained(tab_settings),
                                   CONTENT_SETTINGS_TYPE_JAVASCRIPT));

    ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(path));

    // Always wait for the page to load.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    if (expect_is_javascript_content_blocked) {
      javascript_content_blocked_observer.Wait();
    } else {
      // Since there is no notification that content is not blocked and no
      // content is blocked when |expect_is_javascript_content_blocked| is
      // false, javascript_content_blocked_observer would never succeed.
      // There is no way to ensure blocked content would not have been reported
      // after the check below. For coverage of this scenario, we must rely on
      // the TitleWatcher adding sufficient delay most of the time.
    }

    EXPECT_EQ(expect_is_javascript_content_blocked,
              tab_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
    EXPECT_FALSE(tab_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

class PepperContentSettingsSpecialCasesPluginsBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  void SetUpOnMainThread() override {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                   CONTENT_SETTING_BLOCK);
  }
};

class PepperContentSettingsSpecialCasesJavaScriptBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  void SetUpOnMainThread() override {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    GURL server_root = https_server_.GetURL("/");
    HostContentSettingsMap* content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    content_settings_map->SetContentSettingDefaultScope(
        server_root, server_root, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
        CONTENT_SETTING_ALLOW);
    content_settings_map->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  }
};

// A sanity check to verify that the plugin that is used as a baseline below
// can be loaded.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesTest, Flash) {
  GURL server_root = https_server_.GetURL("/");
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(server_root, server_root,
                                      CONTENT_SETTINGS_TYPE_PLUGINS,
                                      std::string(), CONTENT_SETTING_ALLOW);

  RunLoadPepperPluginTest(content::kFlashPluginSwfMimeType, true);
}

// The following tests verify that Pepper plugins that use JavaScript settings
// instead of Plugins settings still work when Plugins are blocked.

#if BUILDFLAG(ENABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       NaCl) {
  RunLoadPepperPluginTest("application/x-nacl", true);
}
#endif  // BUILDFLAG(ENABLE_NACL)

// The following tests verify that those same Pepper plugins do not work when
// JavaScript is blocked.

// Flash is not blocked when JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       Flash) {
  RunJavaScriptBlockedTest("/load_flash_no_js.html", false);
}

#if BUILDFLAG(ENABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       NaCl) {
  RunJavaScriptBlockedTest("/load_nacl_no_js.html", true);
}
#endif  // BUILDFLAG(ENABLE_NACL)

#endif  // BUILDFLAG(ENABLE_PLUGINS)
