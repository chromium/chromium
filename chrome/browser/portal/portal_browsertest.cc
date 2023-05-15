// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using content::WebContents;

class PortalBrowserTest : public InProcessBrowserTest {
 public:
  PortalBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        // Some tests assume that tabs don't share processes. Disable a feature
        // that allows process sharing among tabs.
        /*disabled_features=*/{
            features::kProcessPerSiteUpToMainFrameThreshold});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, PortalActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* contents = tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(1, tab_strip_model->count());

  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  EXPECT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(portal_contents, tab_strip_model->GetActiveWebContents());
}

// Flaky on Linux ASAN. crbug.com/1182702
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_LINUX)
#define MAYBE_DevToolsWindowStaysOpenAfterActivation \
  DISABLED_DevToolsWindowStaysOpenAfterActivation
#else
#define MAYBE_DevToolsWindowStaysOpenAfterActivation \
  DevToolsWindowStaysOpenAfterActivation
#endif
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       MAYBE_DevToolsWindowStaysOpenAfterActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  DevToolsWindow* dev_tools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  WebContents* main_web_contents =
      DevToolsWindowTesting::Get(dev_tools_window)->main_web_contents();
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(contents, nullptr));

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(
                browser()->tab_strip_model()->GetActiveWebContents(), nullptr));
}

IN_PROC_BROWSER_TEST_F(
    PortalBrowserTest,
    DevToolsWindowIsAttachedToOriginalWebContentsWhenActivationFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/portal-no-src.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* dev_tools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  WebContents* main_web_contents =
      DevToolsWindowTesting::Get(dev_tools_window)->main_web_contents();
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(contents, nullptr));

  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(main_web_contents,
            DevToolsWindow::GetInTabWebContents(
                browser()->tab_strip_model()->GetActiveWebContents(), nullptr));
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, HttpBasicAuthenticationInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true,
            content::EvalJs(contents,
                            "new Promise((resolve, reject) => {\n"
                            "  let portal = document.createElement('portal');\n"
                            "  portal.src = '/title2.html';\n"
                            "  portal.onload = () => resolve(true);\n"
                            "  document.body.appendChild(portal);\n"
                            "})"));
  const auto& inner_contents = contents->GetInnerWebContents();
  ASSERT_EQ(inner_contents.size(), 1u);
  WebContents* portal_contents = inner_contents[0];
  content::NavigationController& portal_controller =
      portal_contents->GetController();

  LoginPromptBrowserTestObserver login_observer;
  login_observer.Register(
      content::Source<content::NavigationController>(&portal_controller));
  WindowedAuthNeededObserver auth_needed(&portal_controller);
  ASSERT_TRUE(content::ExecJs(portal_contents,
                              "location.href = '/auth-basic?realm=Aperture'"));
  auth_needed.Wait();

  WindowedAuthSuppliedObserver auth_supplied(&portal_controller);
  LoginHandler* login_handler = login_observer.handlers().front();
  EXPECT_EQ(login_handler->auth_info().realm, "Aperture");
  login_handler->SetAuth(u"basicuser", u"secret");
  auth_supplied.Wait();

  std::u16string expected_title = u"basicuser/secret";
  content::TitleWatcher title_watcher(portal_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// The task manager should show the portal tasks, and update the tasks after
// activation as tab contents become portals and vice versa.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TaskManagerUpdatesAfterActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::u16string expected_tab_title_before_activation =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX, u"activate.html");
  const std::u16string expected_tab_title_after_activation =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                 u"activate-portal.html");
  const std::u16string expected_portal_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_PORTAL_PREFIX, u"http://127.0.0.1/");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/portal/activate.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(tab, "loadPromise"));

  // Check that both tasks appear.
  chrome::ShowTaskManager(browser());
  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_tab_title_before_activation);
  task_manager::browsertest_util::WaitForTaskManagerRows(1,
                                                         expected_portal_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::ElementsAre(expected_tab_title_before_activation,
                                     expected_portal_title));

  // Activate and check that this updates as expected.
  EXPECT_EQ(true, content::EvalJs(tab, "activate()"));
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_tab_title_after_activation);
  task_manager::browsertest_util::WaitForTaskManagerRows(1,
                                                         expected_portal_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::ElementsAre(expected_tab_title_after_activation,
                                     expected_portal_title));
}

// The task manager should show the portal tasks, and by default they should be
// grouped with their respective tabs. This is similar to
// TaskManagerOOPIFBrowserTest.OrderingOfDependentRows, but less exhaustive.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TaskManagerOrderingOfDependentRows) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const unsigned kNumTabs = 3;
  const unsigned kPortalsPerTab = 2;

  const std::u16string expected_tab_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, u"Title Of Awesomeness");
  const std::u16string expected_portal_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_PORTAL_PREFIX, u"http://127.0.0.1/");
  std::vector<std::u16string> expected_titles;
  for (unsigned i = 0; i < kNumTabs; i++) {
    expected_titles.push_back(expected_tab_title);
    for (unsigned j = 0; j < kPortalsPerTab; j++)
      expected_titles.push_back(expected_portal_title);
  }

  // Open a number of new tabs.
  std::vector<WebContents*> tab_contents;
  for (unsigned i = 0; i < kNumTabs; i++) {
    ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
        browser(), embedded_test_server()->GetURL("/title2.html"), 1,
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    tab_contents.push_back(tab);
  }

  // There's an initial tab that's implicitly created.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(static_cast<int>(kNumTabs), browser()->tab_strip_model()->count());

  // Create portals in each tab.
  for (WebContents* tab : tab_contents) {
    EXPECT_EQ(static_cast<int>(kPortalsPerTab),
              content::EvalJs(
                  tab, content::JsReplace(
                           "Promise.all([...Array($1)].map(() =>"
                           "  new Promise((resolve, reject) => {"
                           "    let portal = document.createElement('portal');"
                           "    portal.src = '/title3.html';"
                           "    portal.onload = () => resolve();"
                           "    document.body.appendChild(portal);"
                           "  }))).then(arr => arr.length)",
                           static_cast<int>(kPortalsPerTab))));
  }

  // Check that the tasks are grouped in the UI as expected.
  chrome::ShowTaskManager(browser());
  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());
  task_manager::browsertest_util::WaitForTaskManagerRows(kNumTabs,
                                                         expected_tab_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      kNumTabs * kPortalsPerTab, expected_portal_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(), expected_titles);
}

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, PdfViewerLoadsInPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true,
            content::EvalJs(contents,
                            "new Promise((resolve) => {\n"
                            "  let portal = document.createElement('portal');\n"
                            "  portal.src = '/pdf/test.pdf';\n"
                            "  portal.onload = () => { resolve(true); }\n"
                            "  document.body.appendChild(portal);\n"
                            "});"));

  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(portal_contents));
}
#endif  // BUILDFLAG(ENABLE_PDF)

// Test that we do not show main frame interstitials in portal contents. We
// should treat portals like subframes in terms of how to display the error to
// the user.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ShowSubFrameErrorPage) {
  net::EmbeddedTestServer bad_https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  bad_https_server.AddDefaultHandlers(GetChromeTestDataDir());
  bad_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(bad_https_server.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  GURL bad_cert_url(bad_https_server.GetURL("/title1.html"));
  content::TestNavigationObserver portal_navigation_observer(nullptr, 1);
  portal_navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(true,
            content::EvalJs(
                contents, content::JsReplace(
                              "new Promise((resolve) => {"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.onload = () => { resolve(true); };"
                              "  document.body.appendChild(portal);"
                              "});",
                              bad_cert_url)));
  portal_navigation_observer.StopWatchingNewWebContents();
  portal_navigation_observer.Wait();
  EXPECT_FALSE(portal_navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID,
            portal_navigation_observer.last_net_error_code());

  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  content::NavigationController& controller = portal_contents->GetController();
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());

  // Ensure that this is the net error page and not an interstitial.
  ASSERT_FALSE(
      chrome_browser_interstitials::IsShowingInterstitial(portal_contents));
  // Also ensure that the error page is using its subframe layout.
  ASSERT_EQ(true, content::EvalJs(
                      portal_contents,
                      "document.documentElement.hasAttribute('subframe');"));
}

// TODO(crbug.com/1372129): This test is flaking on all platforms. The renderer
// seems to be terinated occasionally preventing the ASSERT_EQ from succeeding.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       DISABLED_BrowserHistoryUpdatesOnActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* profile = browser()->profile();
  ASSERT_TRUE(profile);
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  WaitForHistoryBackendToRun(profile);
  EXPECT_TRUE(
      base::Contains(ui_test_utils::HistoryEnumerator(profile).urls(), url1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_EQ(true,
            content::EvalJs(
                contents, content::JsReplace(
                              "new Promise((resolve) => {"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.onload = () => { resolve(true); };"
                              "  document.body.appendChild(portal);"
                              "});",
                              url2)));
  WaitForHistoryBackendToRun(profile);
  // Content loaded in a portal should not be considered a page visit by the
  // user.
  EXPECT_FALSE(
      base::Contains(ui_test_utils::HistoryEnumerator(profile).urls(), url2));

  ASSERT_EQ(true,
            content::EvalJs(contents,
                            "let portal = document.querySelector('portal');"
                            "portal.activate().then(() => { return true; });"));
  WaitForHistoryBackendToRun(profile);
  // Now that the portal has activated, its contents are presented to the user
  // as a navigation in the tab, so this should be considered a page visit.
  EXPECT_TRUE(
      base::Contains(ui_test_utils::HistoryEnumerator(profile).urls(), url2));
}

class PortalSafeBrowsingBrowserTest : public PortalBrowserTest {
 public:
  PortalSafeBrowsingBrowserTest()
      : safe_browsing_factory_(
            std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>()) {
  }

 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_safe_browsing_database_manager_ =
        base::MakeRefCounted<safe_browsing::FakeSafeBrowsingDatabaseManager>(
            content::GetUIThreadTaskRunner({}),
            content::GetIOThreadTaskRunner({}));
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_safe_browsing_database_manager_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(
        safe_browsing_factory_.get());
    PortalBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void TearDown() override {
    PortalBrowserTest::TearDown();
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
  }

  void AddDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        dangerous_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  }

 private:
  scoped_refptr<safe_browsing::FakeSafeBrowsingDatabaseManager>
      fake_safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

// Tests that if a page embeds a portal whose contents are considered dangerous
// by Safe Browsing, the embedder is also treated as dangerous in terms of how
// we display the Safe Browsing interstitial.
// Flaky on ChromeOS & under Ozone (crbug.com/1220319)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE)
#define MAYBE_EmbedderOfDangerousPortalConsideredDangerous \
  DISABLED_EmbedderOfDangerousPortalConsideredDangerous
#else
#define MAYBE_EmbedderOfDangerousPortalConsideredDangerous \
  EmbedderOfDangerousPortalConsideredDangerous
#endif
IN_PROC_BROWSER_TEST_F(PortalSafeBrowsingBrowserTest,
                       MAYBE_EmbedderOfDangerousPortalConsideredDangerous) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL dangerous_url(
      embedded_test_server()->GetURL("evil.com", "/title2.html"));
  AddDangerousUrl(dangerous_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver error_observer(contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);
  ASSERT_TRUE(content::ExecJs(
      contents,
      content::JsReplace("let portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);",
                         dangerous_url)));
  error_observer.WaitForNavigationFinished();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

// Test that if a page embeds a portal which contains a dangerous subresource,
// the embedder is also treated as dangerous in terms of how we display the Safe
// Browsing interstitial.
IN_PROC_BROWSER_TEST_F(PortalSafeBrowsingBrowserTest,
                       PortalDangerousSubresource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL portal_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL dangerous_url(
      embedded_test_server()->GetURL("evil.com", "/title3.html"));
  AddDangerousUrl(dangerous_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true,
            content::EvalJs(
                contents, content::JsReplace(
                              "new Promise((resolve) => {"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.onload = () => { resolve(true); };"
                              "  document.body.appendChild(portal);"
                              "});",
                              portal_url)));
  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  content::TestNavigationObserver error_observer(contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);
  ASSERT_TRUE(content::ExecJs(
      portal_contents,
      content::JsReplace("let iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         dangerous_url)));
  error_observer.WaitForNavigationFinished();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_F(PortalSafeBrowsingBrowserTest, DangerousOrphanedPortal) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL portal_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL dangerous_url(
      embedded_test_server()->GetURL("evil.com", "/title3.html"));
  AddDangerousUrl(dangerous_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ(true,
            content::EvalJs(
                contents, content::JsReplace(
                              "new Promise((resolve) => {"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.onload = () => { resolve(true); };"
                              "  document.body.appendChild(portal);"
                              "});",
                              portal_url)));
  std::vector<WebContents*> inner_web_contents =
      contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());
  WebContents* portal_contents = inner_web_contents[0];

  // Block the activate callback so that the predecessor portal stays orphaned
  // while it navigates to a dangerous URL.
  ASSERT_TRUE(content::ExecJs(
      portal_contents, "window.onportalactivate = e => { while(true) {} };"));

  // Since the portal contents becomes the top level contents from the following
  // activation, it's the contents where we show the interstitial.
  content::TestNavigationObserver error_observer(portal_contents,
                                                 net::ERR_BLOCKED_BY_CLIENT);
  ASSERT_TRUE(content::ExecJs(
      contents,
      content::JsReplace("document.querySelector('portal').activate();"
                         "window.location.href = $1;",
                         dangerous_url)));
  error_observer.WaitForNavigationFinished();
  EXPECT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(portal_contents));
}
