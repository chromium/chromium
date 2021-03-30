// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_formatter/url_formatter.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class ChromeNavigationBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNavigationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmFeature);
  }
  ~ChromeNavigationBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Backgrounded renderer processes run at a lower priority, causing the
    // tests to take more time to complete. Disable backgrounding so that the
    // tests don't time out.
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 protected:
  void ExpectHideAndRestoreSadTabWhenNavigationCancels(bool cross_site);
  void ExpectHideSadTabWhenNavigationCompletes(bool cross_site);

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationBrowserTest);
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// Fails on chromium.memory/Linux Chromium OS ASan LSan:
// https://crbug.com/897879
#define MAYBE_TransientEntryPreservedOnMultipleNavigationsDuringInterstitial \
  DISABLED_TransientEntryPreservedOnMultipleNavigationsDuringInterstitial
#else
#define MAYBE_TransientEntryPreservedOnMultipleNavigationsDuringInterstitial \
  TransientEntryPreservedOnMultipleNavigationsDuringInterstitial
#endif

// Tests that viewing frame source on a local file:// page with an iframe
// with a remote URL shows the correct tab title.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, TestViewFrameSource) {
  // The local page file:// URL.
  GURL local_page_with_iframe_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("iframe.html")));

  // The non-file:// URL of the page to load in the iframe.
  GURL iframe_target_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), local_page_with_iframe_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents->GetMainFrame(),
      base::StringPrintf("var iframe = document.getElementById('test');\n"
                         "iframe.setAttribute('src', '%s');\n",
                         iframe_target_url.spec().c_str())));
  observer.Wait();

  content::RenderFrameHost* frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(frame);
  ASSERT_NE(frame, web_contents->GetMainFrame());

  content::ContextMenuParams params;
  params.page_url = local_page_with_iframe_url;
  params.frame_url = frame->GetLastCommittedURL();
  TestRenderViewContextMenu menu(frame, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE, 0);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(new_web_contents, web_contents);
  EXPECT_TRUE(WaitForLoadStop(new_web_contents));

  GURL view_frame_source_url(content::kViewSourceScheme + std::string(":") +
                             iframe_target_url.spec());
  EXPECT_EQ(url_formatter::FormatUrl(view_frame_source_url),
            new_web_contents->GetTitle());
}

// Base class for ctrl+click tests, which contains all the common functionality
// independent from which process the navigation happens in. Each subclass
// defines its own expectations depending on the conditions of the test.
class CtrlClickProcessTest : public ChromeNavigationBrowserTest {
 protected:
  virtual void VerifyProcessExpectations(
      content::WebContents* main_contents,
      content::WebContents* new_contents) = 0;

  // Simulates ctrl-clicking an anchor with the given id in |main_contents|.
  // Verifies that the new contents are in the correct process and separate
  // BrowsingInstance from |main_contents|.  Returns contents of the newly
  // opened tab.
  content::WebContents* SimulateCtrlClick(content::WebContents* main_contents,
                                          const char* id_of_anchor_to_click) {
    // Ctrl-click the anchor/link in the page.
    content::WebContents* new_contents = nullptr;
    {
      content::WebContentsAddedObserver new_tab_observer;
#if defined(OS_MAC)
      const char* new_tab_click_script_template =
          "simulateClick(\"%s\", { metaKey: true });";
#else
      const char* new_tab_click_script_template =
          "simulateClick(\"%s\", { ctrlKey: true });";
#endif
      std::string new_tab_click_script = base::StringPrintf(
          new_tab_click_script_template, id_of_anchor_to_click);
      EXPECT_TRUE(ExecuteScript(main_contents, new_tab_click_script));

      // Wait for a new tab to appear (the whole point of this test).
      new_contents = new_tab_observer.GetWebContents();
    }

    // Verify that the new tab has the right contents and is in the tab strip.
    EXPECT_TRUE(WaitForLoadStop(new_contents));
    EXPECT_LT(1, browser()->tab_strip_model()->count());  // More than 1 tab?
    EXPECT_NE(
        TabStripModel::kNoTab,
        browser()->tab_strip_model()->GetIndexOfWebContents(new_contents));
    GURL expected_url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_EQ(expected_url, new_contents->GetLastCommittedURL());

    VerifyProcessExpectations(main_contents, new_contents);

    {
      // Double-check that main_contents has expected window.name set.
      // This is a sanity check of test setup; this is not a product test.
      std::string name_of_main_contents_window;
      EXPECT_TRUE(ExecuteScriptAndExtractString(
          main_contents, "window.domAutomationController.send(window.name)",
          &name_of_main_contents_window));
      EXPECT_EQ("main_contents", name_of_main_contents_window);

      // Verify that the new contents doesn't have a window.opener set.
      bool window_opener_cast_to_bool = true;
      EXPECT_TRUE(ExecuteScriptAndExtractBool(
          new_contents, "window.domAutomationController.send(!!window.opener)",
          &window_opener_cast_to_bool));
      EXPECT_FALSE(window_opener_cast_to_bool);

      VerifyBrowsingInstanceExpectations(main_contents, new_contents);
    }

    return new_contents;
  }

  void TestCtrlClick(const char* id_of_anchor_to_click) {
    // Navigate to the test page.
    GURL main_url(embedded_test_server()->GetURL(
        "/frame_tree/anchor_to_same_site_location.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);

    // Verify that there is only 1 active tab (with the right contents
    // committed).
    EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
    content::WebContents* main_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(main_url, main_contents->GetLastCommittedURL());

    // Test what happens after ctrl-click.  SimulateCtrlClick will verify
    // that |new_contents1| is in the correct process and separate
    // BrowsingInstance from |main_contents|.
    content::WebContents* new_contents1 =
        SimulateCtrlClick(main_contents, id_of_anchor_to_click);

    // Test that each subsequent ctrl-click also gets the correct process.
    content::WebContents* new_contents2 =
        SimulateCtrlClick(main_contents, id_of_anchor_to_click);
    EXPECT_FALSE(new_contents1->GetSiteInstance()->IsRelatedSiteInstance(
        new_contents2->GetSiteInstance()));
    VerifyProcessExpectations(new_contents1, new_contents2);
  }

 private:
  void VerifyBrowsingInstanceExpectations(content::WebContents* main_contents,
                                          content::WebContents* new_contents) {
    // Verify that the new contents cannot find the old contents via
    // window.open. (i.e. window.open should open a new window, rather than
    // returning a reference to main_contents / old window).
    std::string location_of_opened_window;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        new_contents,
        "w = window.open('', 'main_contents');"
        "window.domAutomationController.send(w.location.href);",
        &location_of_opened_window));
    EXPECT_EQ(url::kAboutBlankURL, location_of_opened_window);
  }
};

// Tests that verify that ctrl-click results 1) open up in a new renderer
// process (https://crbug.com/23815) and 2) are in a new BrowsingInstance (e.g.
// cannot find the opener's window by name - https://crbug.com/658386).
class CtrlClickShouldEndUpInNewProcessTest : public CtrlClickProcessTest {
 protected:
  void VerifyProcessExpectations(content::WebContents* main_contents,
                                 content::WebContents* new_contents) override {
    // Verify that the two WebContents are in a different process, SiteInstance
    // and BrowsingInstance from the old contents.
    EXPECT_NE(main_contents->GetMainFrame()->GetProcess(),
              new_contents->GetMainFrame()->GetProcess());
    EXPECT_NE(main_contents->GetMainFrame()->GetSiteInstance(),
              new_contents->GetMainFrame()->GetSiteInstance());
    EXPECT_FALSE(main_contents->GetSiteInstance()->IsRelatedSiteInstance(
        new_contents->GetSiteInstance()));
  }
};

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, NoTarget) {
  TestCtrlClick("test-anchor-no-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, BlankTarget) {
  TestCtrlClick("test-anchor-with-blank-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInNewProcessTest, SubframeTarget) {
  TestCtrlClick("test-anchor-with-subframe-target");
}

// Similar to the tests above, but verifies that the new WebContents ends up in
// the same process as the opener when it is exceeding the process limit.
// See https://crbug.com/774723.
class CtrlClickShouldEndUpInSameProcessTest : public CtrlClickProcessTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CtrlClickProcessTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
    content::RenderProcessHost::SetMaxRendererProcessCount(1);
  }

 protected:
  void VerifyProcessExpectations(content::WebContents* contents1,
                                 content::WebContents* contents2) override {
    // Verify that the two WebContents are in the same process, though different
    // SiteInstance and BrowsingInstance from the old contents.
    EXPECT_EQ(contents1->GetMainFrame()->GetProcess(),
              contents2->GetMainFrame()->GetProcess());
    EXPECT_EQ(contents1->GetMainFrame()->GetSiteInstance()->GetSiteURL(),
              contents2->GetMainFrame()->GetSiteInstance()->GetSiteURL());
    EXPECT_FALSE(contents1->GetSiteInstance()->IsRelatedSiteInstance(
        contents2->GetSiteInstance()));
  }
};

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInSameProcessTest, NoTarget) {
  TestCtrlClick("test-anchor-no-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInSameProcessTest, BlankTarget) {
  TestCtrlClick("test-anchor-with-blank-target");
}

IN_PROC_BROWSER_TEST_F(CtrlClickShouldEndUpInSameProcessTest, SubframeTarget) {
  TestCtrlClick("test-anchor-with-subframe-target");
}

// Test to verify that spoofing a URL via a redirect from a slightly malformed
// URL doesn't work.  See also https://crbug.com/657720.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       ContextMenuNavigationToInvalidUrl) {
  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  GURL new_tab_url(
      "www.foo.com::/server-redirect?http%3A%2F%2Fbar.com%2Ftitle2.html");
  EXPECT_TRUE(new_tab_url.is_valid());
  EXPECT_EQ("www.foo.com", new_tab_url.scheme());

  // Navigate to an initial page, to ensure we have a committed document
  // from which to perform a context menu initiated navigation.
  ui_test_utils::NavigateToURL(browser(), initial_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = initial_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Wait for the new tab to be created.
  tab_add.Wait();
  int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);

  // Verify that the load fails (because of the wrong "scheme" - www.foo.com is
  // not a real scheme).
  EXPECT_FALSE(WaitForLoadStop(new_web_contents));

  // Verify that the invalid URL was not committed.
  content::NavigationController& navigation_controller =
      new_web_contents->GetController();
  EXPECT_EQ(nullptr, navigation_controller.GetLastCommittedEntry());
  EXPECT_EQ(0, navigation_controller.GetEntryCount());

  // Verify that the pending entry is still present, even though the navigation
  // has failed and didn't commit.  We preserve the pending entry if it is a
  // valid URL in an unmodified blank tab.
  content::NavigationEntry* pending_entry =
      navigation_controller.GetPendingEntry();
  ASSERT_NE(nullptr, pending_entry);
  EXPECT_EQ(new_tab_url, pending_entry->GetURL());

  // Verify that the pending entry is not shown anymore, after
  // WebContentsImpl::DidAccessInitialDocument detects that the initial, empty
  // document was accessed.
  EXPECT_EQ(pending_entry, navigation_controller.GetVisibleEntry());
  EXPECT_TRUE(content::ExecuteScript(new_web_contents, "window.x=3"));
  EXPECT_NE(pending_entry, navigation_controller.GetVisibleEntry());
}

// Ensure that URL transformations do not let a webpage populate the Omnibox
// with a javascript: URL.  See https://crbug.com/850824 and
// https://crbug.com/1116280.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       ClearInvalidPendingURLOnFail) {
  GURL initial_url = embedded_test_server()->GetURL(
      "/frame_tree/invalid_link_to_new_window.html");

  // Navigate to a page with a link that opens an invalid URL in a new window.
  ui_test_utils::NavigateToURL(browser(), initial_url);
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char* kTestUrls[] = {
      // https://crbug.com/850824
      "o.o:@javascript:foo()",

      // https://crbug.com/1116280
      "o.o:@javascript::://foo.com%0Aalert(document.domain)"};
  for (const char* kTestUrl : kTestUrls) {
    SCOPED_TRACE(testing::Message() << "kTestUrl = " << kTestUrl);
    GURL test_url(kTestUrl);
    EXPECT_TRUE(test_url.is_valid());
    EXPECT_EQ("o.o", test_url.scheme());

    // Set the test URL.
    const char kUrlSettingTemplate[] = R"(
        var url = $1;
        var anchor = document.getElementById('invalid_url_link');
        anchor.target = 'target_name: ' + url;
        anchor.href = url;
    )";
    EXPECT_TRUE(ExecuteScript(
        main_contents, content::JsReplace(kUrlSettingTemplate, kTestUrl)));

    // Simulate a click on the link and wait for the new window.
    content::WebContentsAddedObserver new_tab_observer;
    EXPECT_TRUE(ExecuteScript(main_contents, "simulateClick()"));
    content::WebContents* new_contents = new_tab_observer.GetWebContents();

    // The load in the new window should fail.
    EXPECT_FALSE(WaitForLoadStop(new_contents));

    // Ensure that the omnibox doesn't start with javascript: scheme.
    EXPECT_EQ(test_url, new_contents->GetVisibleURL());
    OmniboxView* omnibox_view =
        browser()->window()->GetLocationBar()->GetOmniboxView();
    std::string omnibox_text = base::UTF16ToASCII(omnibox_view->GetText());
    EXPECT_THAT(omnibox_text, testing::Not(testing::StartsWith("javascript:")));
  }
}

// A test performing two simultaneous navigations, to ensure code in chrome/,
// such as tab helpers, can handle those cases.
// This test starts a browser-initiated cross-process navigation, which is
// delayed. At the same time, the renderer does a synchronous navigation
// through pushState, which will create a separate navigation and associated
// NavigationHandle. Afterwards, the original cross-process navigation is
// resumed and confirmed to properly commit.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       SlowCrossProcessNavigationWithPushState) {
  const GURL kURL1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kPushStateURL =
      embedded_test_server()->GetURL("/title1.html#fragment");
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the initial page.
  ui_test_utils::NavigateToURL(browser(), kURL1);

  // Start navigating to the second page.
  content::TestNavigationManager manager(web_contents, kURL2);
  content::NavigationHandleCommitObserver navigation_observer(web_contents,
                                                              kURL2);
  web_contents->GetController().LoadURL(
      kURL2, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The current page does a PushState.
  content::NavigationHandleCommitObserver push_state_observer(web_contents,
                                                              kPushStateURL);
  std::string push_state =
      "history.pushState({}, \"title 1\", \"" + kPushStateURL.spec() + "\");";
  EXPECT_TRUE(ExecuteScript(web_contents, push_state));
  content::NavigationEntry* last_committed =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(kPushStateURL, last_committed->GetURL());

  EXPECT_TRUE(push_state_observer.has_committed());
  EXPECT_TRUE(push_state_observer.was_same_document());
  EXPECT_TRUE(push_state_observer.was_renderer_initiated());

  // Let the navigation finish. It should commit successfully.
  manager.WaitForNavigationFinished();
  last_committed = web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(kURL2, last_committed->GetURL());

  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_FALSE(navigation_observer.was_same_document());
  EXPECT_FALSE(navigation_observer.was_renderer_initiated());
}

// Check that if a page has an iframe that loads an error page, that error page
// does not inherit the Content Security Policy from the parent frame.  See
// https://crbug.com/703801.  This test is in chrome/ because error page
// behavior is only fully defined in chrome/.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       ErrorPageDoesNotInheritCSP) {
  GURL url(
      embedded_test_server()->GetURL("/page_with_csp_and_error_iframe.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a page that disallows scripts via CSP and has an iframe that
  // tries to load an invalid URL, which results in an error page.
  GURL error_url("http://invalid.foo/");
  content::NavigationHandleObserver observer(web_contents, error_url);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());

  // The error page should not inherit the CSP directive that blocks all
  // scripts from the parent frame, so this script should be allowed to
  // execute.  Since ExecuteScript will execute the passed-in script regardless
  // of CSP, use a javascript: URL which does go through the CSP checks.
  content::RenderFrameHost* error_host =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  std::string location;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      error_host,
      "location='javascript:domAutomationController.send(location.href)';",
      &location));
  EXPECT_EQ(location, content::kUnreachableWebDataURL);

  // The error page should have a unique origin.
  std::string origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      error_host, "domAutomationController.send(self.origin);", &origin));
  EXPECT_EQ("null", origin);
}

// Test that web pages can't navigate to an error page URL, either directly or
// via a redirect, and that web pages can't embed error pages in iframes.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NavigationToErrorURLIsDisallowed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  // Try navigating to the error page URL and make sure it is canceled and the
  // old URL remains the last committed one.
  GURL error_url(content::kUnreachableWebDataURL);
  EXPECT_TRUE(ExecuteScript(web_contents,
                            "location.href = '" + error_url.spec() + "';"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  // Also ensure that a page can't embed an iframe for an error page URL.
  EXPECT_TRUE(ExecuteScript(web_contents,
                            "var frame = document.createElement('iframe');\n"
                            "frame.src = '" + error_url.spec() + "';\n"
                            "document.body.appendChild(frame);"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::RenderFrameHost* subframe_host =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  // The new subframe should remain blank without a committed URL.
  EXPECT_TRUE(subframe_host->GetLastCommittedURL().is_empty());

  // Now try navigating to a URL that tries to redirect to the error page URL
  // and make sure the navigation is ignored. Note that DidStopLoading will
  // still fire, so TestNavigationObserver can be used to wait for it.
  GURL redirect_to_error_url(
      embedded_test_server()->GetURL("/server-redirect?" + error_url.spec()));
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ExecuteScript(
      web_contents, "location.href = '" + redirect_to_error_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(
      content::PAGE_TYPE_NORMAL,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());
  // Check the pending URL is not left in the address bar.
  EXPECT_EQ(url, web_contents->GetVisibleURL());
}

// This test ensures that navigating to a page that returns an error code and
// an empty document still shows Chrome's helpful error page instead of the
// empty document.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       EmptyDocumentWithErrorCode) {
  GURL url(embedded_test_server()->GetURL("/empty_with_404.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the navigation to complete.  The empty document should trigger
  // loading of the 404 error page, so check that the last committed entry was
  // indeed for the error page.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(
      ExecuteScript(web_contents, "location.href = '" + url.spec() + "';"));
  observer.Wait();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(
      IsLastCommittedEntryOfPageType(web_contents, content::PAGE_TYPE_ERROR));

  // Verify that the error page has correct content.  This needs to wait for
  // the error page content to be populated asynchronously by scripts after
  // DidFinishLoad.
  while (true) {
    std::string content;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        web_contents,
        "domAutomationController.send("
        "    document.body ? document.body.innerText : '');",
        &content));
    if (content.find("HTTP ERROR 404") != std::string::npos)
      break;
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

// Test for https://crbug.com/866549#c2. It verifies that about:blank does not
// commit in the error page process when it is redirected to.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       RedirectErrorPageReloadToAboutBlank) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::unique_ptr<content::URLLoaderInterceptor> url_interceptor =
      content::URLLoaderInterceptor::SetupRequestFailForURL(
          url, net::ERR_DNS_TIMED_OUT);

  // Start off with navigation to a.com, which results in an error page.
  {
    content::TestNavigationObserver observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_EQ(GURL(content::kUnreachableWebDataURL),
              web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  }

  // Install an extension, which will redirect all navigations to a.com URLs to
  // about:blank. In general, web servers cannot redirect to about:blank, but
  // extensions with webRequest API permissions can.
  extensions::TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
           "name": "Redirect a.com to about:blank",
           "manifest_version": 2,
           "version": "0.1",
           "permissions": ["webRequest", "webRequestBlocking", "*://a.com/*"],
           "background": { "scripts": ["background.js"] }
         })");
  test_extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(chrome.webRequest.onBeforeRequest.addListener(function(d) {
          console.log("onBeforeRequest: ", d);
          return {redirectUrl:"about:blank"};
        }, {urls: ["*://a.com/*"]}, ["blocking"]);
        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener ready_listener("ready", false /* will_reply */);
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  extension_loader.LoadExtension(test_extension_dir.UnpackedPath());

  // Wait for the background page to load.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Remove the interceptor to allow a reload to succeed, which the extension
  // will intercept and redirect. The navigation should complete successfully
  // and commit in a process that is different than the error page one.
  url_interceptor.reset();
  {
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecuteScript(web_contents, "location.reload();"));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(GURL(url::kAboutBlankURL), observer.last_navigation_url());
    EXPECT_NE(GURL(content::kUnreachableWebDataURL),
              web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL());
  }
}

// This test covers a navigation that:
// 1. is initiated by a cross-site initiator,
// 2. gets redirected via webRequest API to about:blank.
// This is a regression test for https://crbug.com/1026738.
IN_PROC_BROWSER_TEST_F(
    ChromeNavigationBrowserTest,
    NavigationInitiatedByCrossSiteSubframeRedirectedToAboutBlank) {
  const GURL kOpenerUrl(
      embedded_test_server()->GetURL("opener.com", "/title1.html"));
  const GURL kInitialPopupUrl(embedded_test_server()->GetURL(
      "initial-site.com",
      "/frame_tree/page_with_two_frames_remote_and_local.html"));
  const GURL kRedirectedUrl("https://redirected.com/no-such-path");

  // 1. Install an extension, which will redirect all navigations to
  //    redirected.com URLs to about:blank. In general, web servers cannot
  //    redirect to about:blank, but extensions with declarativeWebRequest API
  //    permissions can.
  const char kManifest[] = R"(
      {
        "name": "Test for Bug1026738 - about:blank flavour",
        "version": "0.1",
        "manifest_version": 2,
        "background": {
          "scripts": ["background.js"]
        },
        "permissions": ["webRequest", "webRequestBlocking", "<all_urls>"]
      }
  )";
  const char kRulesScript[] = R"(
      chrome.webRequest.onBeforeRequest.addListener(function(d) {
          console.log("onBeforeRequest: ", d);
          return {redirectUrl: "about:blank"};
        }, {urls: ["*://redirected.com/*"]}, ["blocking"]);
      chrome.test.sendMessage('ready');
  )";
  extensions::TestExtensionDir ext_dir;
  ext_dir.WriteManifest(kManifest);
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kRulesScript);
  ExtensionTestMessageListener ready_listener("ready", false /* will_reply */);
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->FlushNetworkInterfaceForTesting();

  // 2. Open a popup containing a cross-site subframe.
  ui_test_utils::NavigateToURL(browser(), kOpenerUrl);
  content::RenderFrameHost* opener =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  EXPECT_EQ(kOpenerUrl, opener->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(kOpenerUrl), opener->GetLastCommittedOrigin());
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener,
        content::JsReplace("window.open($1, 'my-popup')", kInitialPopupUrl)));
    popup = popup_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup));
  }

  // 3. Find the cross-site subframes in the popup.
  content::RenderFrameHost* popup_root = popup->GetMainFrame();
  content::RenderFrameHost* cross_site_subframe =
      content::ChildFrameAt(popup_root, 0);
  ASSERT_TRUE(cross_site_subframe);
  EXPECT_NE(cross_site_subframe->GetLastCommittedOrigin(),
            popup_root->GetLastCommittedOrigin());
  EXPECT_NE(cross_site_subframe->GetLastCommittedOrigin(),
            opener->GetLastCommittedOrigin());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(cross_site_subframe->GetSiteInstance(),
              popup_root->GetSiteInstance());
    EXPECT_NE(cross_site_subframe->GetSiteInstance(),
              opener->GetSiteInstance());
  }
  scoped_refptr<content::SiteInstance> old_popup_site_instance =
      popup_root->GetSiteInstance();
  scoped_refptr<content::SiteInstance> old_subframe_site_instance =
      cross_site_subframe->GetSiteInstance();

  // 4. Initiate popup navigation from the cross-site subframe.
  //    Note that the extension from step 1 above will redirect
  //    this navigation to an about:blank URL.
  //
  // This step would have hit the CHECK from https://crbug.com/1026738.
  url::Origin cross_site_origin = cross_site_subframe->GetLastCommittedOrigin();
  content::TestNavigationObserver nav_observer(popup, 1);
  ASSERT_TRUE(ExecJs(cross_site_subframe,
                     content::JsReplace("top.location = $1", kRedirectedUrl)));
  nav_observer.Wait();
  EXPECT_EQ(url::kAboutBlankURL, popup->GetLastCommittedURL());
  EXPECT_EQ(cross_site_origin, popup->GetMainFrame()->GetLastCommittedOrigin());

  // 5. Verify that the about:blank URL is hosted in the same process
  //    as the navigation initiator (and separate from the opener and the old
  //    popup process).
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_NE(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_EQ(old_subframe_site_instance.get(), popup->GetSiteInstance());
    EXPECT_NE(url::kAboutBlankURL,
              popup->GetSiteInstance()->GetSiteURL().scheme());
    EXPECT_NE(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());
  } else {
    EXPECT_EQ(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_EQ(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_EQ(old_subframe_site_instance.get(), popup->GetSiteInstance());
    EXPECT_NE(url::kAboutBlankURL,
              popup->GetSiteInstance()->GetSiteURL().scheme());
    EXPECT_NE(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());
  }
}

// This test covers a navigation that:
// 1. is initiated by a cross-site initiator,
// 2. gets redirected via webRequest API to a data: URL
// This covers a scenario similar to the one that led to crashes in
// https://crbug.com/1026738.
IN_PROC_BROWSER_TEST_F(
    ChromeNavigationBrowserTest,
    NavigationInitiatedByCrossSiteSubframeRedirectedToDataUrl) {
  const GURL kOpenerUrl(
      embedded_test_server()->GetURL("opener.com", "/title1.html"));
  const GURL kInitialPopupUrl(embedded_test_server()->GetURL(
      "initial-site.com",
      "/frame_tree/page_with_two_frames_remote_and_local.html"));
  const GURL kRedirectedUrl("https://redirected.com/no-such-path");
  const GURL kRedirectTargetUrl(
      "data:text/html,%3Ch1%3EHello%2C%20World!%3C%2Fh1%3E");

  // 1. Install an extension, which will redirect all navigations to
  //    redirected.com URLs to a data: URL. In general, web servers cannot
  //    redirect to data: URLs, but extensions with declarativeWebRequest API
  //    permissions can.
  const char kManifest[] = R"(
      {
        "name": "Test for Bug1026738 - data: URL flavour",
        "version": "0.1",
        "manifest_version": 2,
        "background": {
          "scripts": ["background.js"]
        },
        "permissions": ["webRequest", "webRequestBlocking", "<all_urls>"]
      }
  )";
  const char kRulesScriptTemplate[] = R"(
      chrome.webRequest.onBeforeRequest.addListener(function(d) {
          console.log("onBeforeRequest: ", d);
          return {redirectUrl: $1};
        }, {urls: ["*://redirected.com/*"]}, ["blocking"]);
      chrome.test.sendMessage('ready');
  )";
  extensions::TestExtensionDir ext_dir;
  ext_dir.WriteManifest(kManifest);
  ext_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      content::JsReplace(kRulesScriptTemplate, kRedirectTargetUrl));
  ExtensionTestMessageListener ready_listener("ready", false /* will_reply */);
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->FlushNetworkInterfaceForTesting();

  // 2. Open a popup containing a cross-site subframe.
  ui_test_utils::NavigateToURL(browser(), kOpenerUrl);
  content::RenderFrameHost* opener =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  EXPECT_EQ(kOpenerUrl, opener->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(kOpenerUrl), opener->GetLastCommittedOrigin());
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener,
        content::JsReplace("window.open($1, 'my-popup')", kInitialPopupUrl)));
    popup = popup_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup));
  }

  // 3. Find the cross-site subframes in the popup.
  EXPECT_EQ(3u, popup->GetAllFrames().size());
  content::RenderFrameHost* popup_root = popup->GetMainFrame();
  content::RenderFrameHost* cross_site_subframe = popup->GetAllFrames()[1];
  EXPECT_NE(cross_site_subframe->GetLastCommittedOrigin(),
            popup_root->GetLastCommittedOrigin());
  EXPECT_NE(cross_site_subframe->GetLastCommittedOrigin(),
            opener->GetLastCommittedOrigin());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(cross_site_subframe->GetSiteInstance(),
              popup_root->GetSiteInstance());
    EXPECT_NE(cross_site_subframe->GetSiteInstance(),
              opener->GetSiteInstance());
  }
  scoped_refptr<content::SiteInstance> old_popup_site_instance =
      popup_root->GetSiteInstance();

  // 4. Initiate popup navigation from the cross-site subframe.
  //    Note that the extension from step 1 above will redirect
  //    this navigation to a data: URL.
  //
  // This step might hit the CHECK in GetOriginForURLLoaderFactory once we start
  // enforcing opaque origins with no precursor in CanAccessDataForOrigin.
  content::TestNavigationObserver nav_observer(popup, 1);
  ASSERT_TRUE(ExecJs(cross_site_subframe,
                     content::JsReplace("top.location = $1", kRedirectedUrl)));
  nav_observer.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  EXPECT_TRUE(popup->GetMainFrame()->GetLastCommittedOrigin().opaque());

  // 5. Verify that with site-per-process the data: URL is hosted in a brand
  //    new, separate process (separate from the opener and the previous popup
  //    process).
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_NE(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_EQ(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());
  } else {
    EXPECT_EQ(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_EQ(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_NE(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());
  }
}

class SignInIsolationBrowserTest : public ChromeNavigationBrowserTest {
 public:
  SignInIsolationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SignInIsolationBrowserTest() override {}

  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ChromeNavigationBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the sign-in URL so that it includes correct port from the test
    // server.
    command_line->AppendSwitchASCII(
        ::switches::kGaiaUrl,
        https_server()->GetURL("accounts.google.com", "/").spec());

    // Ignore cert errors so that the sign-in URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ChromeNavigationBrowserTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(SignInIsolationBrowserTest);
};

// This test ensures that the sign-in origin requires a dedicated process.  It
// only ensures that the sign-in origin is added as an isolated origin at
// chrome/ layer; IsolatedOriginTest provides the main test coverage of origins
// whitelisted for process isolation.  See https://crbug.com/739418.
IN_PROC_BROWSER_TEST_F(SignInIsolationBrowserTest, NavigateToSignInPage) {
  const GURL first_url =
      embedded_test_server()->GetURL("google.com", "/title1.html");
  const GURL signin_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), first_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::SiteInstance> first_instance(
      web_contents->GetMainFrame()->GetSiteInstance());

  // Make sure that a renderer-initiated navigation to the sign-in page swaps
  // processes.
  content::TestNavigationManager manager(web_contents, signin_url);
  EXPECT_TRUE(
      ExecuteScript(web_contents, "location = '" + signin_url.spec() + "';"));
  manager.WaitForNavigationFinished();
  EXPECT_NE(web_contents->GetMainFrame()->GetSiteInstance(), first_instance);
}

class WebstoreIsolationBrowserTest : public ChromeNavigationBrowserTest {
 public:
  WebstoreIsolationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~WebstoreIsolationBrowserTest() override {}

  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ChromeNavigationBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the webstore URL.
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        https_server()->GetURL("chrome.foo.com", "/frame_tree").spec());

    // Ignore cert errors so that the webstore URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ChromeNavigationBrowserTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreIsolationBrowserTest);
};

// Make sure that Chrome Web Store origins are isolated from the rest of their
// foo.com site.  See https://crbug.com/939108.
IN_PROC_BROWSER_TEST_F(WebstoreIsolationBrowserTest, WebstorePopupIsIsolated) {
  const GURL first_url = https_server()->GetURL("foo.com", "/title1.html");
  ui_test_utils::NavigateToURL(browser(), first_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a popup for chrome.foo.com and ensure that it's isolated in a
  // different SiteInstance and process from the rest of foo.com.  Note that
  // we're opening a URL that does *not* match the web store URL due to a
  // different path, so there will be no BrowsingInstance swap, and window.open
  // is still expected to return a valid window reference.
  content::TestNavigationObserver popup_waiter(nullptr, 1);
  popup_waiter.StartWatchingNewWebContents();
  const GURL webstore_origin_url =
      https_server()->GetURL("chrome.foo.com", "/title1.html");
  EXPECT_TRUE(content::EvalJs(
                  web_contents,
                  content::JsReplace("!!window.open($1);", webstore_origin_url))
                  .ExtractBool());
  popup_waiter.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(popup));

  scoped_refptr<content::SiteInstance> popup_instance(
      popup->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(web_contents->GetMainFrame()->GetSiteInstance(), popup_instance);
  EXPECT_NE(web_contents->GetMainFrame()->GetSiteInstance()->GetProcess(),
            popup_instance->GetProcess());

  // Also navigate the popup to the full web store URL and confirm that this
  // causes a BrowsingInstance swap.
  const GURL webstore_url =
      https_server()->GetURL("chrome.foo.com", "/frame_tree/simple.htm");
  content::TestNavigationManager manager(popup, webstore_url);
  EXPECT_TRUE(
      ExecuteScript(popup, "location = '" + webstore_url.spec() + "';"));
  manager.WaitForNavigationFinished();
  EXPECT_NE(popup->GetMainFrame()->GetSiteInstance(), popup_instance);
  EXPECT_NE(popup->GetMainFrame()->GetSiteInstance(),
            web_contents->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(popup->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_instance.get()));
  EXPECT_FALSE(popup->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
      web_contents->GetMainFrame()->GetSiteInstance()));
}

// Helper class. Track one navigation and tell whether a response from the
// server has been received or not. It is useful for discerning navigations
// blocked after or before the request has been sent.
class WillProcessResponseObserver : public content::WebContentsObserver {
 public:
  explicit WillProcessResponseObserver(content::WebContents* web_contents,
                                       const GURL& url)
      : content::WebContentsObserver(web_contents), url_(url) {}
  ~WillProcessResponseObserver() override {}

  bool WillProcessResponseCalled() { return will_process_response_called_; }

 private:
  GURL url_;
  bool will_process_response_called_ = false;

  // Is used to set |will_process_response_called_| to true when
  // NavigationThrottle::WillProcessResponse() is called.
  class WillProcessResponseObserverThrottle
      : public content::NavigationThrottle {
   public:
    WillProcessResponseObserverThrottle(content::NavigationHandle* handle,
                                        bool* will_process_response_called)
        : NavigationThrottle(handle),
          will_process_response_called_(will_process_response_called) {}

    const char* GetNameForLogging() override {
      return "WillProcessResponseObserverThrottle";
    }

   private:
    bool* will_process_response_called_;
    NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
      *will_process_response_called_ = true;
      return NavigationThrottle::PROCEED;
    }
  };

  // WebContentsObserver
  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (handle->GetURL() == url_) {
      handle->RegisterThrottleForTesting(
          std::make_unique<WillProcessResponseObserverThrottle>(
              handle, &will_process_response_called_));
    }
  }
};

// In HTTP/HTTPS documents, check that no request with the "ftp:" scheme are
// submitted to load an iframe.
// See https://crbug.com/757809.
// Note: This test couldn't be a content_browsertests, since there would be
// no handler defined for the "ftp" protocol in
// URLRequestJobFactoryImpl::protocol_handler_map_.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, BlockLegacySubresources) {
  net::SpawnedTestServer ftp_server(net::SpawnedTestServer::TYPE_FTP,
                                    GetChromeTestDataDir());
  ASSERT_TRUE(ftp_server.Start());

  GURL main_url_http(embedded_test_server()->GetURL("/iframe.html"));
  GURL iframe_url_http(embedded_test_server()->GetURL("/simple.html"));
  GURL iframe_url_ftp(ftp_server.GetURL("simple.html"));
  GURL redirect_url(embedded_test_server()->GetURL("/server-redirect?"));

  struct {
    GURL main_url;
    GURL iframe_url;
    bool allowed;
  } kTestCases[] = {
      {main_url_http, iframe_url_http, true},
      {main_url_http, iframe_url_ftp, false},
  };
  for (const auto& test_case : kTestCases) {
    // Blocking the request should work, even after a redirect.
    for (bool redirect : {false, true}) {
      GURL iframe_url =
          redirect ? GURL(redirect_url.spec() + test_case.iframe_url.spec())
                   : test_case.iframe_url;
      SCOPED_TRACE(::testing::Message()
                   << std::endl
                   << "- main_url = " << test_case.main_url << std::endl
                   << "- iframe_url = " << iframe_url << std::endl);

      ui_test_utils::NavigateToURL(browser(), test_case.main_url);
      content::WebContents* web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      content::NavigationHandleObserver navigation_handle_observer(web_contents,
                                                                   iframe_url);
      WillProcessResponseObserver will_process_response_observer(web_contents,
                                                                 iframe_url);
      EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", iframe_url));

      if (test_case.allowed) {
        EXPECT_TRUE(will_process_response_observer.WillProcessResponseCalled());
        EXPECT_FALSE(navigation_handle_observer.is_error());
        EXPECT_EQ(test_case.iframe_url,
                  navigation_handle_observer.last_committed_url());
      } else {
        EXPECT_FALSE(
            will_process_response_observer.WillProcessResponseCalled());
        EXPECT_TRUE(navigation_handle_observer.is_error());
        EXPECT_EQ(net::ERR_ABORTED,
                  navigation_handle_observer.net_error_code());
      }
    }
  }
}

// Check that it's possible to navigate to a chrome scheme URL from a crashed
// tab. See https://crbug.com/764641.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, ChromeSchemeNavFromSadTab) {
  // Kill the renderer process.
  content::RenderProcessHost* process = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetMainFrame()
                                            ->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  // Attempt to navigate to a chrome://... URL.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
}

// Check that a browser-initiated navigation to a cross-site URL that then
// redirects to a pdf hosted on another site works.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, CrossSiteRedirectionToPDF) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");
  GURL cross_site_redirecting_url =
      https_server.GetURL("/server-redirect?" + pdf_url.spec());
  ui_test_utils::NavigateToURL(browser(), initial_url);
  ui_test_utils::NavigateToURL(browser(), cross_site_redirecting_url);
  EXPECT_EQ(pdf_url, browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
}

using ChromeNavigationBrowserTestWithMobileEmulation = DevToolsProtocolTestBase;

// Tests the behavior of navigating to a PDF when mobile emulation is enabled.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTestWithMobileEmulation,
                       NavigateToPDFWithMobileEmulation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), initial_url);

  Attach();
  base::Value params(base::Value::Type::DICTIONARY);
  params.SetIntKey("width", 400);
  params.SetIntKey("height", 800);
  params.SetDoubleKey("deviceScaleFactor", 1.0);
  params.SetBoolKey("mobile", true);
  SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params));

  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");
  ui_test_utils::NavigateToURL(browser(), pdf_url);

  EXPECT_EQ(pdf_url, web_contents()->GetLastCommittedURL());
  EXPECT_EQ(
      "<head></head>"
      "<body><!-- no enabled plugin supports this MIME type --></body>",
      content::EvalJs(web_contents(), "document.documentElement.innerHTML")
          .ExtractString());
}

// Check that clicking on a link doesn't carry the transient user activation
// from the original page to the navigated page (crbug.com/865243).
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       WindowOpenBlockedAfterClickNavigation) {
  // Navigate to a test page with links.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/links.html"));

  // Click to navigate to title1.html.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  ASSERT_TRUE(ExecuteScript(main_contents,
                            "document.getElementById('title1').click();"));
  observer.Wait();

  // Make sure popup attempt fails due to lack of transient user activation.
  bool opened = false;
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      main_contents, "window.domAutomationController.send(!!window.open());",
      &opened));
  EXPECT_FALSE(opened);

  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            main_contents->GetLastCommittedURL());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       OpenerNavigation_DownloadPolicy_Disallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Open a popup.
  bool opened = false;
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  const char* kScriptFormat =
      "window.domAutomationController.send(!!window.open('%s'));";
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  content::TestNavigationObserver popup_waiter(nullptr, 1);
  popup_waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      opener, base::StringPrintf(kScriptFormat, popup_url.spec().c_str()),
      &opened));
  EXPECT_TRUE(opened);
  popup_waiter.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Using the popup, navigate its opener to a download.
  base::HistogramTester histograms;
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, opener);
  EXPECT_TRUE(WaitForLoadStop(popup));

  content::WebContentsConsoleObserver console_observer(opener);
  console_observer.SetPattern(
      "Navigating a cross-origin opener to a download (*) is deprecated*");
  EXPECT_TRUE(content::ExecuteScript(
      popup,
      "window.opener.location ='data:html/text;base64,'+btoa('payload');"));

  console_observer.Wait();
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin, 1);

  // Ensure that no download happened.
  std::vector<download::DownloadItem*> download_items;
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());
  manager->GetAllDownloads(&download_items);
  EXPECT_TRUE(download_items.empty());
}

// Opener navigations from a same-origin popup should be allowed.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       OpenerNavigation_DownloadPolicy_Allowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Open a popup.
  bool opened = false;
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  const char* kScriptFormat =
      "window.domAutomationController.send(!!window.open('%s'));";
  GURL popup_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  content::TestNavigationObserver popup_waiter(nullptr, 1);
  popup_waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      opener, base::StringPrintf(kScriptFormat, popup_url.spec().c_str()),
      &opened));
  EXPECT_TRUE(opened);
  popup_waiter.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Using the popup, navigate its opener to a download.
  base::HistogramTester histograms;
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, opener);
  EXPECT_TRUE(WaitForLoadStop(popup));

  content::DownloadTestObserverInProgress observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1 /* wait_count */);
  EXPECT_TRUE(content::ExecuteScript(
      popup,
      "window.opener.location ='data:html/text;base64,'+btoa('payload');"));
  observer.WaitForFinished();

  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin, 0);

  // Delete any pending download.
  std::vector<download::DownloadItem*> download_items;
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());
  manager->GetAllDownloads(&download_items);
  for (auto* item : download_items) {
    if (!item->IsDone())
      item->Cancel(true);
  }
}

// Test which verifies that a noopener link/window.open() properly focus the
// newly opened tab. See https://crbug.com/912348.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NoopenerCorrectlyFocusesNewTab) {
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a test page with links.
  {
    content::TestNavigationObserver observer(main_contents);
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/click-noreferrer-links.html"));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Click a link with noopener that navigates in a new window.
  content::WebContents* link_web_contents = nullptr;
  {
    ui_test_utils::AllBrowserTabAddedWaiter tab_added;
    EXPECT_TRUE(
        content::ExecJs(main_contents, "clickSameSiteNoOpenerTargetedLink();"));
    link_web_contents = tab_added.Wait();
  }

  EXPECT_NE(main_contents, link_web_contents);
  EXPECT_TRUE(link_web_contents->GetRenderWidgetHostView()->HasFocus());

  // Execute window.open() with noopener.
  content::WebContents* open_web_contents = nullptr;
  {
    ui_test_utils::AllBrowserTabAddedWaiter tab_added;
    EXPECT_TRUE(content::ExecJs(
        main_contents, content::JsReplace("window.open($1, 'bar', 'noopener');",
                                          embedded_test_server()->GetURL(
                                              "a.com", "/title1.html"))));
    open_web_contents = tab_added.Wait();
  }

  EXPECT_NE(main_contents, open_web_contents);
  EXPECT_NE(link_web_contents, open_web_contents);
  EXPECT_TRUE(open_web_contents->GetRenderWidgetHostView()->HasFocus());
}

// Tests the ukm entry logged when the navigation entry is marked as skippable
// on back/forward button on doing a renderer initiated navigation without ever
// getting a user activation.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NoUserActivationSetSkipOnBackForward) {
  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), skippable_url);

  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer without a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      main_contents, "location = '" + redirected_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(redirected_url, main_contents->GetLastCommittedURL());

  // Verify UKM.
  using Entry = ukm::builders::HistoryManipulationIntervention;
  const auto& ukm_entries =
      test_ukm_recorder()->GetEntriesByName(Entry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder()->ExpectEntrySourceHasUrl(ukm_entries[0], skippable_url);

  // Verify the metric where user tries to go specifically to a skippable entry
  // using long press.
  base::HistogramTester histogram;
  std::unique_ptr<BackForwardMenuModel> back_model(
      std::make_unique<BackForwardMenuModel>(
          browser(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(main_contents);
  back_model->ActivatedAt(0);
  histogram.ExpectBucketCount(
      "Navigation.BackForward.NavigatingToEntryMarkedToBeSkipped", true, 1);
}

// Same as above except the navigation is cross-site.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NoUserActivationSetSkipOnBackForwardCrossSite) {
  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), skippable_url);

  GURL redirected_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  {
    // Navigate to a new document from the renderer without a user gesture.
    content::WebContents* main_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(main_contents);
    EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
        main_contents, "location = '" + redirected_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(redirected_url, main_contents->GetLastCommittedURL());
  }

  // Verify UKM.
  using Entry = ukm::builders::HistoryManipulationIntervention;
  const auto& ukm_entries =
      test_ukm_recorder()->GetEntriesByName(Entry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder()->ExpectEntrySourceHasUrl(ukm_entries[0], skippable_url);
}

// Ensure that starting a navigation out of a sad tab hides the sad tab right
// away, without waiting for the navigation to commit and restores it again
// after cancelling.
void ChromeNavigationBrowserTest::
    ExpectHideAndRestoreSadTabWhenNavigationCancels(bool cross_site) {
  // This test only applies when this policy is in place.
  if (!content::ShouldSkipEarlyCommitPendingForCrashedFrame())
    return;
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(contents);

  GURL url_start(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_hung =
      embedded_test_server()->GetURL(cross_site ? "b.com" : "a.com", "/hung");
  GURL url_succeed = embedded_test_server()->GetURL(
      cross_site ? "b.com" : "a.com", "/title2.html");
  ui_test_utils::NavigateToURL(browser(), url_start);

  // No sad tab should be visible after a successful navigation.
  ASSERT_FALSE(sad_tab_helper->sad_tab());

  // Kill the renderer process.
  content::RenderProcessHost* process = contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  // Make sure the sad tab is shown.
  ASSERT_TRUE(sad_tab_helper->sad_tab());

  // Start a navigation that will never finish and wait for request start.
  content::TestNavigationManager manager(contents, url_hung);
  contents->GetController().LoadURL(url_hung, content::Referrer(),
                                    ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Ensure that the sad tab is hidden at this point.
  ASSERT_FALSE(sad_tab_helper->sad_tab());

  // Cancel the pending navigation and ensure that the sad tab returns.
  chrome::Stop(browser());
  EXPECT_TRUE(sad_tab_helper->sad_tab());
  // Ensure that the omnibox URL is the crashed one.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  std::string omnibox_text = base::UTF16ToASCII(omnibox_view->GetText());
  EXPECT_EQ(omnibox_text, url_start.spec());

  // Make sure the sad tab goes away when we commit successfully.
  ui_test_utils::NavigateToURL(browser(), url_succeed);
  EXPECT_FALSE(sad_tab_helper->sad_tab());
}

// Ensure that starting a navigation out of a sad tab hides the sad tab right
// away, without waiting for the navigation to commit and restores it again
// after cancelling.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       RestoreSadTabWhenNavigationCancels_CrossSite) {
  ExpectHideAndRestoreSadTabWhenNavigationCancels(/*cross_site=*/true);
}

// Same-site version of above.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       RestoreSadTabWhenNavigationCancels_SameSite) {
  ExpectHideAndRestoreSadTabWhenNavigationCancels(/*cross_site=*/false);
}

// Ensure that completing a navigation from a sad tab will clear the sad tab.
void ChromeNavigationBrowserTest::ExpectHideSadTabWhenNavigationCompletes(
    bool cross_site) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(contents);

  GURL url_start(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_succeed = embedded_test_server()->GetURL(
      cross_site ? "b.com" : "a.com", "/title2.html");
  ui_test_utils::NavigateToURL(browser(), url_start);

  // No sad tab should be visible after a successful navigation.
  ASSERT_FALSE(sad_tab_helper->sad_tab());

  // Kill the renderer process.
  content::RenderProcessHost* process = contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  // Make sure the sad tab is shown.
  ASSERT_TRUE(sad_tab_helper->sad_tab());

  // Make sure the sad tab goes away when we commit successfully.
  ui_test_utils::NavigateToURL(browser(), url_succeed);
  EXPECT_FALSE(sad_tab_helper->sad_tab());
}

// Ensure that completing a navigation from a sad tab will clear the sad tab.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       ClearSadTabWhenNavigationCompletes_CrossSite) {
  ExpectHideSadTabWhenNavigationCompletes(/*cross_site=*/true);
}

// Same-site version of above.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       ClearSadTabWhenNavigationCompletes_SameSite) {
  ExpectHideSadTabWhenNavigationCompletes(/*cross_site=*/false);
}

// TODO(csharrison): These tests should become tentative WPT, once the feature
// is enabled by default.
using NavigationConsumingTest = ChromeNavigationBrowserTest;

// The fullscreen API is spec'd to require a user activation (aka user gesture),
// so use that API to test if navigation consumes the activation.
// https://fullscreen.spec.whatwg.org/#allowed-to-request-fullscreen
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest,
                       NavigationConsumesUserGesture_Fullscreen) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/navigation_consumes_gesture.html"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Normally, fullscreen should work, as long as there is a user gesture.
  bool is_fullscreen = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, "document.body.webkitRequestFullscreen();", &is_fullscreen));
  EXPECT_TRUE(is_fullscreen);

  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, "document.webkitExitFullscreen();", &is_fullscreen));
  EXPECT_FALSE(is_fullscreen);

  // However, starting a navigation should consume the gesture. Fullscreen
  // should not work afterwards. Make sure the navigation is synchronously
  // started via click().
  std::string script = R"(
    document.getElementsByTagName('a')[0].click();
    document.body.webkitRequestFullscreen();
  )";

  // Use the TestNavigationManager to ensure the navigation is not finished
  // before fullscreen can occur.
  content::TestNavigationManager nav_manager(
      contents, embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractBool(contents, script, &is_fullscreen));
  EXPECT_FALSE(is_fullscreen);
}

// Similar to the fullscreen test above, but checks that popups are successfully
// blocked if spawned after a navigation.
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest,
                       NavigationConsumesUserGesture_Popups) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/links.html"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Normally, a popup should open fine if it is associated with a user gesture.
  bool did_open = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, "window.domAutomationController.send(!!window.open());",
      &did_open));
  EXPECT_TRUE(did_open);

  // Starting a navigation should consume a gesture, but make sure that starting
  // a same-document navigation doesn't do the consuming.
  std::string same_document_script = R"(
    document.getElementById("ref").click();
    window.domAutomationController.send(!!window.open());
  )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, same_document_script, &did_open));
  EXPECT_TRUE(did_open);

  // If the navigation is to a different document, the gesture should be
  // successfully consumed.
  std::string different_document_script = R"(
    document.getElementById("title1").click();
    window.domAutomationController.send(!!window.open());
  )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, different_document_script, &did_open));
  EXPECT_FALSE(did_open);
}

// Regression test for https://crbug.com/856779, where a navigation to a
// top-level, same process frame in another tab fails to focus that tab.
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest, TargetNavigationFocus) {
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/link_with_target.html"));

  {
    content::TestNavigationObserver new_tab_observer(nullptr, 1);
    new_tab_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(ExecuteScript(
        opener, "document.getElementsByTagName('a')[0].click();"));
    new_tab_observer.Wait();
  }

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, opener);

  // Re-focusing the opener and clicking again should re-focus the popup.
  opener->GetDelegate()->ActivateContents(opener);
  EXPECT_EQ(opener, browser()->tab_strip_model()->GetActiveWebContents());
  {
    content::TestNavigationObserver new_tab_observer(new_contents, 1);
    ASSERT_TRUE(ExecuteScript(
        opener, "document.getElementsByTagName('a')[0].click();"));
    new_tab_observer.Wait();
  }
  EXPECT_EQ(new_contents, browser()->tab_strip_model()->GetActiveWebContents());
}

using HistoryManipulationInterventionBrowserTest = ChromeNavigationBrowserTest;

// Tests that chrome::GoBack does nothing if all the previous entries are marked
// as skippable and the back button is disabled.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       AllEntriesSkippableBackButtonDisabled) {
  // Create a new tab to avoid confusion from having a NTP navigation entry.
  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), skippable_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a new document from the renderer without a user gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  content::TestNavigationManager manager(main_contents, redirected_url);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      main_contents, "location = '" + redirected_url.spec() + "';"));
  manager.WaitForNavigationFinished();
  ASSERT_EQ(redirected_url, main_contents->GetLastCommittedURL());
  ASSERT_EQ(2, main_contents->GetController().GetEntryCount());

  // Attempting to go back should do nothing.
  ASSERT_FALSE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_EQ(redirected_url, main_contents->GetLastCommittedURL());

  // Back command should be disabled.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_BACK));
}

// Tests that chrome::GoBack is successful if there is at least one entry not
// marked as skippable and the back button should be enabled.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       AllEntriesNotSkippableBackButtonEnabled) {
  // Navigate to a URL in the same tab. Note that at the start of the test this
  // tab already has about:blank.
  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), skippable_url);

  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a new document from the renderer without a user gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  content::TestNavigationManager manager(main_contents, redirected_url);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      main_contents, "location = '" + redirected_url.spec() + "';"));
  manager.WaitForNavigationFinished();
  ASSERT_EQ(redirected_url, main_contents->GetLastCommittedURL());
  ASSERT_EQ(3, main_contents->GetController().GetEntryCount());

  // Back command should be enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BACK));

  // Attempting to go back should skip |skippable_url| and go to about:blank.
  ASSERT_TRUE(chrome::CanGoBack(browser()));
  content::TestNavigationObserver observer(main_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  ASSERT_EQ(GURL("about:blank"), main_contents->GetLastCommittedURL());
}

// Tests that a main frame hosting pdf does not get skipped because of history
// manipulation intervention if there was a user gesture.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       PDFDoNotSkipOnBackForwardDueToUserGesture) {
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), pdf_url);

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer with a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecuteScript(main_contents, "location = '" + url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(url, main_contents->GetLastCommittedURL());

  // Since pdf_url initiated a navigation with a user gesture, it will
  // not be skipped. Going back should be allowed and should navigate to
  // pdf_url.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BACK));

  ASSERT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(main_contents));
  ASSERT_EQ(pdf_url, main_contents->GetLastCommittedURL());
}

// Tests that a main frame hosting pdf gets skipped because of history
// manipulation intervention if there was no user gesture.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       PDFSkipOnBackForwardNoUserGesture) {
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), pdf_url);

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer without a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(
      main_contents, "location = '" + url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(url, main_contents->GetLastCommittedURL());

  // Since pdf_url initiated a navigation without a user gesture, it will
  // be skipped. Going back should be allowed and should navigate to
  // about:blank.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BACK));

  ASSERT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(main_contents));
  ASSERT_EQ(GURL("about:blank"), main_contents->GetLastCommittedURL());
}

// This test class turns on the mode where sites where the user enters a
// password are dynamically added to the list of sites requiring a dedicated
// process.  It also disables strict site isolation so that the effects of
// password isolation can be observed.
class SiteIsolationForPasswordSitesBrowserTest
    : public ChromeNavigationBrowserTest {
 public:
  SiteIsolationForPasswordSitesBrowserTest() {
    feature_list_.InitWithFeatures(
        {site_isolation::features::kSiteIsolationForPasswordSites},
        {features::kSitePerProcess});
  }

  std::vector<std::string> GetSavedIsolatedSites() {
    return GetSavedIsolatedSites(browser()->profile());
  }

  std::vector<std::string> GetSavedIsolatedSites(Profile* profile) {
    PrefService* prefs = profile->GetPrefs();
    auto* list =
        prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins);
    std::vector<std::string> sites;
    for (const base::Value& value : list->GetList())
      sites.push_back(value.GetString());
    return sites;
  }

  bool HasSyntheticTrial(const std::string& trial_name) {
    std::vector<std::string> synthetic_trials;
    variations::GetSyntheticTrialGroupIdsAsString(&synthetic_trials);
    std::string trial_hash =
        base::StringPrintf("%x", variations::HashName(trial_name));
    auto it =
        std::find_if(synthetic_trials.begin(), synthetic_trials.end(),
                     [trial_hash](const auto& trial) {
                       return base::StartsWith(trial, trial_hash,
                                               base::CompareCase::SENSITIVE);
                     });
    return it != synthetic_trials.end();
  }

  bool IsInSyntheticTrialGroup(const std::string& trial_name,
                               const std::string& trial_group) {
    std::vector<std::string> synthetic_trials;
    variations::GetSyntheticTrialGroupIdsAsString(&synthetic_trials);
    std::string expected_entry =
        base::StringPrintf("%x-%x", variations::HashName(trial_name),
                           variations::HashName(trial_group));
    return std::find(synthetic_trials.begin(), synthetic_trials.end(),
                     expected_entry) != synthetic_trials.end();
  }

  const std::string kSiteIsolationSyntheticTrialName = "SiteIsolationActive";
  const std::string kOOPIFSyntheticTrialName = "OutOfProcessIframesActive";
  const std::string kSyntheticTrialGroup = "Enabled";

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);

    // This simulates a whitelist of isolated sites.
    std::string origin_list =
        embedded_test_server()->GetURL("isolated1.com", "/").spec() + "," +
        embedded_test_server()->GetURL("isolated2.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);

    // Allow HTTPS server to be used on sites other than localhost.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that a site gets process-isolated after a password is typed on a
// page from that site.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       SiteIsIsolatedAfterEnteringPassword) {
  // This test requires dynamic isolated origins to be enabled.
  if (!content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return;

  GURL url(embedded_test_server()->GetURL("sub.foo.com",
                                          "/password/password_form.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // foo.com should not be isolated to start with. Verify that a cross-site
  // iframe does not become an OOPIF.
  EXPECT_FALSE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());
  std::string kAppendIframe = R"(
      var i = document.createElement('iframe');
      i.id = 'child';
      document.body.appendChild(i);)";
  EXPECT_TRUE(ExecJs(contents, kAppendIframe));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  content::RenderFrameHost* child = ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_FALSE(child->IsCrossProcessSubframe());

  // Fill a form and submit through a <input type="submit"> button.
  content::TestNavigationObserver observer(contents);
  std::string kFillAndSubmit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  EXPECT_TRUE(content::ExecJs(contents, kFillAndSubmit));
  observer.Wait();

  // Since there were no script references from other windows, we should've
  // swapped BrowsingInstances and put the result of the form submission into a
  // dedicated process, locked to foo.com.  Check that a cross-site iframe now
  // becomes an OOPIF.
  EXPECT_TRUE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(ExecJs(contents, kAppendIframe));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  child = ChildFrameAt(contents->GetMainFrame(), 0);
  EXPECT_TRUE(child->IsCrossProcessSubframe());

  // Open a fresh tab (also forcing a new BrowsingInstance), navigate to
  // foo.com, and verify that a cross-site iframe becomes an OOPIF.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, contents);

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(ExecJs(new_contents, kAppendIframe));
  EXPECT_TRUE(NavigateIframeToURL(new_contents, "child", bar_url));
  content::RenderFrameHost* new_child =
      ChildFrameAt(new_contents->GetMainFrame(), 0);
  EXPECT_TRUE(new_child->IsCrossProcessSubframe());
}

// This test checks that the synthetic field trial is activated properly after
// a navigation to an isolated origin commits in a main frame.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       SyntheticTrialFromMainFrame) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  recorder->EnableSiteIsolationSyntheticTrialForTesting();

  EXPECT_FALSE(HasSyntheticTrial(kSiteIsolationSyntheticTrialName));
  EXPECT_FALSE(HasSyntheticTrial(kOOPIFSyntheticTrialName));

  // Browse to a page with some iframes without involving any isolated origins.
  GURL unisolated_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c(a))"));
  ui_test_utils::NavigateToURL(browser(), unisolated_url);
  EXPECT_FALSE(HasSyntheticTrial(kSiteIsolationSyntheticTrialName));

  // Now browse to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated1.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), isolated_url);
  EXPECT_TRUE(IsInSyntheticTrialGroup(kSiteIsolationSyntheticTrialName,
                                      kSyntheticTrialGroup));

  // The OOPIF synthetic trial shouldn't be activated, since the isolated
  // oriign page doesn't have any OOPIFs.
  EXPECT_FALSE(
      IsInSyntheticTrialGroup(kOOPIFSyntheticTrialName, kSyntheticTrialGroup));
}

// This test checks that the synthetic field trials for both site isolation and
// encountering OOPIFs are activated properly after a navigation to an isolated
// origin commits in a subframe.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       SyntheticTrialFromSubframe) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NavigationMetricsRecorder* recorder =
      content::WebContentsUserData<NavigationMetricsRecorder>::FromWebContents(
          web_contents);
  recorder->EnableSiteIsolationSyntheticTrialForTesting();

  EXPECT_FALSE(HasSyntheticTrial(kSiteIsolationSyntheticTrialName));
  EXPECT_FALSE(HasSyntheticTrial(kOOPIFSyntheticTrialName));

  // Browse to a page with an isolated origin on one of the iframes.
  GURL isolated_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c,isolated2,d)"));
  ui_test_utils::NavigateToURL(browser(), isolated_url);
  EXPECT_TRUE(IsInSyntheticTrialGroup(kSiteIsolationSyntheticTrialName,
                                      kSyntheticTrialGroup));
  EXPECT_TRUE(
      IsInSyntheticTrialGroup(kOOPIFSyntheticTrialName, kSyntheticTrialGroup));
}

// Verifies that persistent isolated sites survive restarts.  Part 1.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       PRE_IsolatedSitesPersistAcrossRestarts) {
  // There shouldn't be any saved isolated origins to start with.
  EXPECT_THAT(GetSavedIsolatedSites(), IsEmpty());

  // Isolate saved.com and saved2.com persistently.
  GURL saved_url(embedded_test_server()->GetURL("saved.com", "/title1.html"));
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  GURL saved2_url(embedded_test_server()->GetURL("saved2.com", "/title1.html"));
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved2_url);

  // Check that saved.com utilizes a dedicated process in future navigations.
  // Open a new tab to force creation of a new BrowsingInstance.
  AddBlankTabAndShow(browser());
  ui_test_utils::NavigateToURL(browser(), saved_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());

  // Check that saved.com and saved2.com were saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com", "http://saved2.com"));
}

// Verifies that process-isolated sites persist across restarts.  Part 2.
// This runs after Part 1 above and in the same profile.  Part 1 has already
// added "saved.com" as a persisted isolated origin, so this part verifies that
// it requires a dedicated process after restart.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IsolatedSitesPersistAcrossRestarts) {
  // Check that saved.com and saved2.com are still saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com", "http://saved2.com"));

  // Check that these sites utilize a dedicated process after restarting, but a
  // non-isolated foo.com URL does not.
  GURL saved_url(embedded_test_server()->GetURL("saved.com", "/title1.html"));
  GURL saved2_url(embedded_test_server()->GetURL("saved2.com", "/title2.html"));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title3.html"));
  ui_test_utils::NavigateToURL(browser(), saved_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());
  ui_test_utils::NavigateToURL(browser(), saved2_url);
  EXPECT_TRUE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());
  ui_test_utils::NavigateToURL(browser(), foo_url);
  EXPECT_FALSE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());
}

// Verify that trying to isolate a site multiple times will only save it to
// disk once.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IsolatedSiteIsSavedOnlyOnce) {
  GURL saved_url(embedded_test_server()->GetURL("saved.com", "/title1.html"));
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com"));
}

// Check that Incognito doesn't inherit saved isolated origins from its
// original profile, and that any isolated origins added in Incognito don't
// affect the original profile.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IncognitoWithIsolatedSites) {
  // Isolate saved.com and verify it's been saved to disk.
  GURL saved_url(embedded_test_server()->GetURL("saved.com", "/title1.html"));
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com"));

  // Create an incognito browser and browse to saved.com.  Verify that it's
  // *not* isolated in incognito.
  //
  // TODO(alexmos): This might change in the future if we decide to inherit
  // main profile's isolated origins in incognito. See
  // https://crbug.com/905513.
  Browser* incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(incognito, saved_url);
  content::WebContents* contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());

  // Add an isolated site in incognito, and verify that while future
  // navigations to this site in incognito require a dedicated process,
  // navigations to this site in the main profile do not require a dedicated
  // process, and the site is not persisted for either the main or incognito
  // profiles.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  content::SiteInstance::StartIsolatingSite(incognito->profile(), foo_url);

  AddBlankTabAndShow(incognito);
  ui_test_utils::NavigateToURL(incognito, foo_url);
  contents = incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());

  AddBlankTabAndShow(browser());
  ui_test_utils::NavigateToURL(browser(), foo_url);
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      contents->GetMainFrame()->GetSiteInstance()->RequiresDedicatedProcess());

  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              testing::Not(testing::Contains("http://foo.com")));
  EXPECT_THAT(GetSavedIsolatedSites(incognito->profile()),
              testing::Not(testing::Contains("http://foo.com")));
}

// Verify that serving a Clear-Site-Data header does not clear saved isolated
// sites.  Saved isolated sites should only be cleared by user-initiated
// actions.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       ClearSiteDataDoesNotClearSavedIsolatedSites) {
  // Start an HTTPS server, as Clear-Site-Data is only available on HTTPS URLs.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  // Isolate saved.com and verify it's been saved to disk.
  GURL saved_url(https_server.GetURL("saved.com", "/clear_site_data.html"));
  content::SiteInstance::StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));

  // Navigate to a URL that serves a Clear-Site-Data header for cache, cookies,
  // and DOM storage. This is the most that a Clear-Site-Data header could
  // clear, and this should not clear saved isolated sites.
  ui_test_utils::NavigateToURL(browser(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));
}
