// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/switches.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using content::ExecuteScript;
using content::ExecuteScriptAndExtractString;
using content::NavigationController;
using content::WebContents;

namespace extensions {

namespace {

std::string WrapForJavascriptAndExtract(const char* javascript_expression) {
  return std::string("window.domAutomationController.send(") +
      javascript_expression + ")";
}

std::unique_ptr<net::test_server::HttpResponse> HandleExpectAndSetCookieRequest(
    const net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/expect-and-set-cookie?",
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);

  std::string request_cookies;
  auto it = request.headers.find("Cookie");
  if (it != request.headers.end())
    request_cookies = it->second;

  size_t query_string_pos = request.relative_url.find('?');
  std::string query_string =
      request.relative_url.substr(query_string_pos + 1);
  url::Component query(0, query_string.length()), key_pos, value_pos;
  bool expectations_satisfied = true;
  std::vector<std::string> cookies_to_set;
  while (url::ExtractQueryKeyValue(query_string.c_str(), &query, &key_pos,
                                   &value_pos)) {
    std::string escaped_key(query_string.substr(key_pos.begin, key_pos.len));
    std::string escaped_value(
        query_string.substr(value_pos.begin, value_pos.len));

    std::string key = net::UnescapeBinaryURLComponent(escaped_key);
    std::string value = net::UnescapeBinaryURLComponent(escaped_value);

    if (key == "expect") {
      if (request_cookies.find(value) == std::string::npos)
        expectations_satisfied = false;
    } else if (key == "set") {
      cookies_to_set.push_back(value);
    } else {
      return nullptr;
    }
  }

  if (expectations_satisfied) {
    for (size_t i = 0; i < cookies_to_set.size(); i++)
      http_response->AddCustomHeader("Set-Cookie", cookies_to_set[i]);
  }

  return std::move(http_response);
}

class IsolatedAppTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Returns whether the given tab's current URL has the given cookie.
  bool WARN_UNUSED_RESULT HasCookie(WebContents* contents,
                                    const std::string& cookie) {
    int value_size;
    std::string actual_cookie;
    ui_test_utils::GetCookies(contents->GetURL(), contents, &value_size,
                              &actual_cookie);
    return actual_cookie.find(cookie) != std::string::npos;
  }

  const Extension* GetInstalledApp(WebContents* contents) {
    content::BrowserContext* browser_context = contents->GetBrowserContext();
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
    std::set<std::string> extension_ids =
        ProcessMap::Get(browser_context)
            ->GetExtensionsInProcess(
                contents->GetMainFrame()->GetProcess()->GetID());
    for (auto iter = extension_ids.begin(); iter != extension_ids.end();
         ++iter) {
      const Extension* installed_app =
          registry->enabled_extensions().GetByID(*iter);
      if (installed_app && installed_app->is_app())
        return installed_app;
    }
    return NULL;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedAppTest, CrossProcessClientRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));

  // Redirect to app2.
  GURL redirect_url(embedded_test_server()->GetURL(
      "/extensions/isolated_apps/app2/redirect.html"));
  ui_test_utils::NavigateToURL(browser(), redirect_url);

  // Go back twice.
  // If bug fixed, we cannot go back anymore.
  // If not fixed, we will redirect back to app2 and can go back again.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We also need to test script-initialized navigation (document.location.href)
  // happened after page finishes loading. This one will also triggered the
  // willPerformClientRedirect hook in RenderViewImpl but should not replace
  // the previous history entry.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("non_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  WebContents* tab0 = browser()->tab_strip_model()->GetWebContentsAt(1);

  // Using JavaScript to navigate to app2 page,
  // after the non_app page has finished loading.
  content::WindowedNotificationObserver observer1(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  std::string script = base::StringPrintf(
        "document.location.href=\"%s\";",
        base_url.Resolve("app2/main.html").spec().c_str());
  EXPECT_TRUE(ExecuteScript(tab0, script));
  observer1.Wait();

  // This kind of navigation should not replace previous navigation entry.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(chrome::CanGoBack(browser()));
}

// Tests that cookies set within an isolated app are not visible to normal
// pages or other apps.
//
// TODO(ajwong): Also test what happens if an app spans multiple sites in its
// extent.  These origins should also be isolated, but still have origin-based
// separation as you would expect.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, CookieIsolation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app2/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("non_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Ensure first two tabs have installed apps.
  WebContents* tab0 = browser()->tab_strip_model()->GetWebContentsAt(0);
  WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(1);
  WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(2);
  ASSERT_TRUE(GetInstalledApp(tab0));
  ASSERT_TRUE(GetInstalledApp(tab1));
  ASSERT_TRUE(!GetInstalledApp(tab2));

  // Check that tabs see cannot each other's localStorage even though they are
  // in the same origin.
  ASSERT_TRUE(ExecuteScript(
      tab0, "window.localStorage.setItem('testdata', 'ls_app1');"));
  ASSERT_TRUE(ExecuteScript(
      tab1, "window.localStorage.setItem('testdata', 'ls_app2');"));
  ASSERT_TRUE(ExecuteScript(
      tab2, "window.localStorage.setItem('testdata', 'ls_normal');"));

  const std::string& kRetrieveLocalStorage =
      WrapForJavascriptAndExtract(
          "window.localStorage.getItem('testdata') || 'badval'");
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      tab0, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_app1", result);
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      tab1, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_app2", result);
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      tab2, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_normal", result);

  // Check that each tab sees its own cookie.
  EXPECT_TRUE(HasCookie(tab0, "app1=3"));
  EXPECT_TRUE(HasCookie(tab1, "app2=4"));
  EXPECT_TRUE(HasCookie(tab2, "normalPage=5"));

  // Check that app1 tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab0, "app2"));
  EXPECT_FALSE(HasCookie(tab0, "normalPage"));

  // Check that app2 tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab1, "app1"));
  EXPECT_FALSE(HasCookie(tab1, "normalPage"));

  // Check that normal tab cannot see the other cookies.
  EXPECT_FALSE(HasCookie(tab2, "app1"));
  EXPECT_FALSE(HasCookie(tab2, "app2"));

  // Check that the non_app iframe cookie is associated with app1 and not the
  // normal tab.  (For now, iframes are always rendered in their parent
  // process, even if they aren't in the app manifest.)
  EXPECT_TRUE(HasCookie(tab0, "nonAppFrame=6"));
  EXPECT_FALSE(HasCookie(tab2, "nonAppFrame"));

  // Check that isolation persists even if the tab crashes and is reloaded.
  chrome::SelectNumberedTab(browser(), 0);
  content::CrashTab(tab0);
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_TRUE(HasCookie(tab0, "app1=3"));
  EXPECT_FALSE(HasCookie(tab0, "app2"));
  EXPECT_FALSE(HasCookie(tab0, "normalPage"));
}

// This test is disabled due to being flaky. http://crbug.com/145588
// Ensure that cookies are not isolated if the isolated apps are not installed.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, DISABLED_NoCookieIsolationWithoutApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app2/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("non_app/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Check that tabs see each other's cookies.
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(0),
                        "app2=4"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(0),
                        "normalPage=5"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(0),
                        "nonAppFrame=6"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(1),
                        "app1=3"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(1),
                        "normalPage=5"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(1),
                        "nonAppFrame=6"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(2),
                        "app1=3"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(2),
                        "app2=4"));
  EXPECT_TRUE(HasCookie(browser()->tab_strip_model()->GetWebContentsAt(2),
                        "nonAppFrame=6"));

  // Check that all tabs share the same localStorage if they have the same
  // origin.
  WebContents* app1_wc = browser()->tab_strip_model()->GetWebContentsAt(0);
  WebContents* app2_wc = browser()->tab_strip_model()->GetWebContentsAt(1);
  WebContents* non_app_wc = browser()->tab_strip_model()->GetWebContentsAt(2);
  ASSERT_TRUE(ExecuteScript(
      app1_wc, "window.localStorage.setItem('testdata', 'ls_app1');"));
  ASSERT_TRUE(ExecuteScript(
      app2_wc, "window.localStorage.setItem('testdata', 'ls_app2');"));
  ASSERT_TRUE(ExecuteScript(
      non_app_wc, "window.localStorage.setItem('testdata', 'ls_normal');"));

  const std::string& kRetrieveLocalStorage =
      WrapForJavascriptAndExtract("window.localStorage.getItem('testdata')");
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      app1_wc, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_normal", result);
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      app2_wc, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_normal", result);
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      non_app_wc, kRetrieveLocalStorage.c_str(), &result));
  EXPECT_EQ("ls_normal", result);
}

// http://crbug.com/174926
#if (defined(OS_WIN) && !defined(NDEBUG)) || defined(OS_MACOSX)
#define MAYBE_SubresourceCookieIsolation DISABLED_SubresourceCookieIsolation
#else
#define MAYBE_SubresourceCookieIsolation SubresourceCookieIsolation
#endif  // (defined(OS_WIN) && !defined(NDEBUG)) || defined(OS_MACOSX)

// Tests that subresource and media requests use the app's cookie store.
// See http://crbug.com/141172.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, MAYBE_SubresourceCookieIsolation) {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleExpectAndSetCookieRequest, embedded_test_server()));

  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL root_url = embedded_test_server()->GetURL("/");
  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  root_url = root_url.ReplaceComponents(replace_host);
  base_url = base_url.ReplaceComponents(replace_host);

  // First set cookies inside and outside the app.
  ui_test_utils::NavigateToURL(
      browser(), root_url.Resolve("expect-and-set-cookie?set=nonApp%3d1"));
  WebContents* tab0 = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(GetInstalledApp(tab0));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app1/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(GetInstalledApp(tab1));

  // Check that each tab sees its own cookie.
  EXPECT_TRUE(HasCookie(tab0, "nonApp=1"));
  EXPECT_FALSE(HasCookie(tab0, "app1=3"));
  EXPECT_FALSE(HasCookie(tab1, "nonApp=1"));
  EXPECT_TRUE(HasCookie(tab1, "app1=3"));

  // Now visit an app page that loads subresources located outside the app.
  // For both images and video tags, it loads two URLs:
  //  - One will set nonApp{Media,Image}=1 cookies if nonApp=1 is set.
  //  - One will set app1{Media,Image}=1 cookies if app1=3 is set.
  // We expect only the app's cookies to be present.
  // We must wait for the onload event, to allow the subresources to finish.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(
          browser()->tab_strip_model()->GetActiveWebContents()));
  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("app1/app_subresources.html"));
  observer.Wait();
  EXPECT_FALSE(HasCookie(tab1, "nonAppMedia=1"));
  EXPECT_TRUE(HasCookie(tab1, "app1Media=1"));
  EXPECT_FALSE(HasCookie(tab1, "nonAppImage=1"));
  EXPECT_TRUE(HasCookie(tab1, "app1Image=1"));

  // Also create a non-app tab to ensure no new cookies were set in that jar.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), root_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(2);
  EXPECT_FALSE(HasCookie(tab2, "nonAppMedia=1"));
  EXPECT_FALSE(HasCookie(tab2, "app1Media=1"));
  EXPECT_FALSE(HasCookie(tab2, "nonAppImage=1"));
  EXPECT_FALSE(HasCookie(tab2, "app1Image=1"));
}

// Test is flaky on Windows.
// http://crbug.com/247667
#if defined(OS_WIN)
#define MAYBE_IsolatedAppProcessModel DISABLED_IsolatedAppProcessModel
#else
#define MAYBE_IsolatedAppProcessModel IsolatedAppProcessModel
#endif  // defined(OS_WIN)

// This test used to check that isolated apps processes do not render top-level
// non-app pages, and that this is true even in the case of the OAuth
// workaround for hosted apps, where non-app popups may be kept in the hosted
// app process.  After fixing https://crbug.com/828720, this workaround will
// apply to all SiteInstances that utilize effective URLs, which includes
// isolated apps.  Therefore, this test is now checking that when an isolated
// app window.opens a non-app same-site URL, the popup does stay in the
// isolated app process.
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, MAYBE_IsolatedAppProcessModel) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  // Create three tabs in the isolated app in different ways.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("app1/main.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  // For the third tab, use window.open to keep it in process with an opener.
  OpenWindow(browser()->tab_strip_model()->GetWebContentsAt(0),
             base_url.Resolve("app1/main.html"), true, true, nullptr);

  // In a fourth tab, use window.open to a non-app same-site URL.  It should
  // stay in the app process and SiteInstance, due to the workaround in
  // https://crbug.com/828720 which keeps same-site non-app popups in the app
  // process.
  //
  // TODO(alexmos,creis): the workaround for https://crbug.com/828720 could be
  // restricted to hosted apps only, allowing this to go to a separate non-app
  // process.  We would need to consider this if isolated apps are ever
  // shipped.
  OpenWindow(browser()->tab_strip_model()->GetWebContentsAt(0),
             base_url.Resolve("non_app/main.html"),
             true /* newtab_process_should_equal_opener */,
             true /* should_succeed */, nullptr);

  // We should now have four tabs, the first and third sharing a process.
  // The second one is an independent instance in a separate process.
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  int process_id_0 = browser()
                         ->tab_strip_model()
                         ->GetWebContentsAt(0)
                         ->GetMainFrame()
                         ->GetProcess()
                         ->GetID();
  int process_id_1 = browser()
                         ->tab_strip_model()
                         ->GetWebContentsAt(1)
                         ->GetMainFrame()
                         ->GetProcess()
                         ->GetID();
  EXPECT_NE(process_id_0, process_id_1);
  EXPECT_EQ(process_id_0, browser()
                              ->tab_strip_model()
                              ->GetWebContentsAt(2)
                              ->GetMainFrame()
                              ->GetProcess()
                              ->GetID());
  EXPECT_EQ(process_id_0, browser()
                              ->tab_strip_model()
                              ->GetWebContentsAt(3)
                              ->GetMainFrame()
                              ->GetProcess()
                              ->GetID());

  // Navigating the second tab out of the app should cause a process swap.
  const GURL& non_app_url(base_url.Resolve("non_app/main.html"));
  NavigateInRenderer(browser()->tab_strip_model()->GetWebContentsAt(1),
                     non_app_url);
  EXPECT_NE(process_id_1, browser()
                              ->tab_strip_model()
                              ->GetWebContentsAt(1)
                              ->GetMainFrame()
                              ->GetProcess()
                              ->GetID());
}

// This test no longer passes, since we don't properly isolate sessionStorage
// for isolated apps. This was broken as part of the changes for storage
// partition support for webview tags.
// TODO(nasko): If isolated apps is no longer developed, this test should be
// removed. http://crbug.com/159932
IN_PROC_BROWSER_TEST_F(IsolatedAppTest, DISABLED_SessionStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app1")));
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("isolated_apps/app2")));

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL("/extensions/isolated_apps/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  // Enter some state into sessionStorage three times on the same origin, but
  // for three URLs that correspond to app1, app2, and a non-isolated site.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  ASSERT_TRUE(ExecuteScript(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      "window.sessionStorage.setItem('testdata', 'ss_app1');"));

  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app2/main.html"));
  ASSERT_TRUE(ExecuteScript(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      "window.sessionStorage.setItem('testdata', 'ss_app2');"));

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("non_app/main.html"));
  ASSERT_TRUE(ExecuteScript(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      "window.sessionStorage.setItem('testdata', 'ss_normal');"));

  // Now, ensure that the sessionStorage is correctly partitioned, and persists
  // when we navigate around all over the dang place.
  const std::string& kRetrieveSessionStorage =
      WrapForJavascriptAndExtract(
          "window.sessionStorage.getItem('testdata') || 'badval'");
  std::string result;
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app1/main.html"));
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      kRetrieveSessionStorage.c_str(), &result));
  EXPECT_EQ("ss_app1", result);

  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("app2/main.html"));
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      kRetrieveSessionStorage.c_str(), &result));
  EXPECT_EQ("ss_app2", result);

  ui_test_utils::NavigateToURL(
      browser(), base_url.Resolve("non_app/main.html"));
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      kRetrieveSessionStorage.c_str(), &result));
  EXPECT_EQ("ss_normal", result);
}

}  // namespace

}  // namespace extensions
