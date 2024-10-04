// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/login_detection/login_detection_util.h"
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
#include "chrome/test/base/profile_destruction_waiter.h"
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
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
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
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class ChromeNavigationBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNavigationBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmFeature);
  }

  ChromeNavigationBrowserTest(const ChromeNavigationBrowserTest&) = delete;
  ChromeNavigationBrowserTest& operator=(const ChromeNavigationBrowserTest&) =
      delete;

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
};

// Tests that viewing frame source on a local file:// page with an iframe
// with a remote URL shows the correct tab title.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, TestViewFrameSource) {
  // The local page file:// URL.
  GURL local_page_with_iframe_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("iframe.html")));

  // The non-file:// URL of the page to load in the iframe.
  GURL iframe_target_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), local_page_with_iframe_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents->GetPrimaryMainFrame(),
      base::StringPrintf("var iframe = document.getElementById('test');\n"
                         "iframe.setAttribute('src', '%s');\n",
                         iframe_target_url.spec().c_str())));
  observer.Wait();

  content::RenderFrameHost* frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(frame);
  ASSERT_NE(frame, web_contents->GetPrimaryMainFrame());

  content::ContextMenuParams params;
  params.page_url = local_page_with_iframe_url;
  params.frame_url = frame->GetLastCommittedURL();
  TestRenderViewContextMenu menu(*frame, params);
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
      static constexpr char kNewTabClickScriptTemplate[] =
#if BUILDFLAG(IS_MAC)
          "simulateClick(\"%s\", { metaKey: true });";
#else
          "simulateClick(\"%s\", { ctrlKey: true });";
#endif
      std::string new_tab_click_script =
          base::StringPrintf(kNewTabClickScriptTemplate, id_of_anchor_to_click);
      EXPECT_TRUE(ExecJs(main_contents, new_tab_click_script));

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
      EXPECT_EQ("main_contents", EvalJs(main_contents, "window.name"));

      // Verify that the new contents doesn't have a window.opener set.
      EXPECT_EQ(false, EvalJs(new_contents, "!!window.opener"));

      VerifyBrowsingInstanceExpectations(main_contents, new_contents);
    }

    return new_contents;
  }

  void TestCtrlClick(const char* id_of_anchor_to_click) {
    // Navigate to the test page.
    GURL main_url(embedded_test_server()->GetURL(
        "/frame_tree/anchor_to_same_site_location.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

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
    EXPECT_EQ(url::kAboutBlankURL,
              EvalJs(new_contents,
                     "w = window.open('', 'main_contents');"
                     "w.location.href;"));
  }
};

// Tests that verify that ctrl-click results 1) open up in a new renderer
// process (https://crbug.com/23815) and 2) are in a new BrowsingInstance (e.g.
// cannot find the opener's window by name - https://crbug.com/658386).
class CtrlClickShouldEndUpInNewProcessTest : public CtrlClickProcessTest {
 protected:
  void VerifyProcessExpectations(content::WebContents* main_contents,
                                 content::WebContents* new_contents) override {
    // The two WebContents should not share the same process unless process
    // sharing is explicitly allowed by a process-per-site feature.
    if (!base::FeatureList::IsEnabled(
            features::kProcessPerSiteUpToMainFrameThreshold)) {
      EXPECT_NE(main_contents->GetPrimaryMainFrame()->GetProcess(),
                new_contents->GetPrimaryMainFrame()->GetProcess());
    }
    // The new WebContents should always have a different SiteInstance and
    // BrowsingInstance from the old contents.
    EXPECT_NE(main_contents->GetPrimaryMainFrame()->GetSiteInstance(),
              new_contents->GetPrimaryMainFrame()->GetSiteInstance());
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
    EXPECT_EQ(contents1->GetPrimaryMainFrame()->GetProcess(),
              contents2->GetPrimaryMainFrame()->GetProcess());
    EXPECT_EQ(
        contents1->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
        contents2->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = initial_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Wait for the new tab to be created.
  tab_add.Wait();
  int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);

  // Verify that the invalid URL was not committed.
  content::NavigationController& navigation_controller =
      new_web_contents->GetController();
  WaitForLoadStop(new_web_contents);
  EXPECT_TRUE(navigation_controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(1, navigation_controller.GetEntryCount());
  EXPECT_NE(new_tab_url, new_web_contents->GetLastCommittedURL());

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
  EXPECT_TRUE(content::ExecJs(new_web_contents, "window.x=3"));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
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
    EXPECT_TRUE(ExecJs(main_contents,
                       content::JsReplace(kUrlSettingTemplate, kTestUrl)));

    // Simulate a click on the link and wait for the new window.
    content::WebContentsAddedObserver new_tab_observer;
    EXPECT_TRUE(ExecJs(main_contents, "simulateClick()"));
    content::WebContents* new_contents = new_tab_observer.GetWebContents();

    // Verify that the invalid URL was not committed.
    content::NavigationController& navigation_controller =
        new_contents->GetController();
    WaitForLoadStop(new_contents);
    EXPECT_TRUE(
        navigation_controller.GetLastCommittedEntry()->IsInitialEntry());
    EXPECT_EQ(1, navigation_controller.GetEntryCount());
    EXPECT_NE(test_url, new_contents->GetLastCommittedURL());

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kURL1));

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
  EXPECT_TRUE(ExecJs(web_contents, push_state));
  content::NavigationEntry* last_committed =
      web_contents->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_committed);
  EXPECT_EQ(kPushStateURL, last_committed->GetURL());

  EXPECT_TRUE(push_state_observer.has_committed());
  EXPECT_TRUE(push_state_observer.was_same_document());
  EXPECT_TRUE(push_state_observer.was_renderer_initiated());

  // Let the navigation finish. It should commit successfully.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(observer.has_committed());
  EXPECT_TRUE(observer.is_error());

  // The error page should not inherit the CSP directive that blocks all
  // scripts from the parent frame, so this script should be allowed to
  // execute.  Since ExecJs will execute the passed-in script regardless
  // of CSP, use a javascript: URL which does go through the CSP checks.
  content::RenderFrameHost* error_host =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(EvalJs(error_host,
                   R"(
                    var resolve;
                    new Promise((res) => {
                      resolve = res;
                      location = 'javascript:resolve(location.href)';
                    });
        )"),
            content::kUnreachableWebDataURL);

  // The error page should have a unique origin.
  EXPECT_EQ("null", EvalJs(error_host, "self.origin;"));
}

// Test that web pages can't navigate to an error page URL, either directly or
// via a redirect, and that web pages can't embed error pages in iframes.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NavigationToErrorURLIsDisallowed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  // Try navigating to the error page URL and make sure it is canceled and the
  // old URL remains the last committed one.
  GURL error_url(content::kUnreachableWebDataURL);
  EXPECT_TRUE(
      ExecJs(web_contents, "location.href = '" + error_url.spec() + "';"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  // Also ensure that a page can't embed an iframe for an error page URL.
  EXPECT_TRUE(ExecJs(web_contents,
                     "var frame = document.createElement('iframe');\n"
                     "frame.src = '" +
                         error_url.spec() +
                         "';\n"
                         "document.body.appendChild(frame);"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::RenderFrameHost* subframe_host =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  // The new subframe should remain blank without a committed URL.
  EXPECT_TRUE(subframe_host->GetLastCommittedURL().is_empty());

  // Now try navigating to a URL that tries to redirect to the error page URL
  // and make sure the navigation is ignored. Note that DidStopLoading will
  // still fire, so TestNavigationObserver can be used to wait for it.
  GURL redirect_to_error_url(
      embedded_test_server()->GetURL("/server-redirect?" + error_url.spec()));
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ExecJs(
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
  EXPECT_TRUE(ExecJs(web_contents, "location.href = '" + url.spec() + "';"));
  observer.Wait();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(
      IsLastCommittedEntryOfPageType(web_contents, content::PAGE_TYPE_ERROR));

  // Verify that the error page has correct content.  This needs to wait for
  // the error page content to be populated asynchronously by scripts after
  // DidFinishLoad.
  while (true) {
    std::string content =
        EvalJs(web_contents, "document.body ? document.body.innerText : '';")
            .ExtractString();
    if (content.find("HTTP ERROR 404") != std::string::npos) {
      break;
    }
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_EQ(
        GURL(content::kUnreachableWebDataURL),
        web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
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

  ExtensionTestMessageListener ready_listener("ready");
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
    EXPECT_TRUE(ExecJs(web_contents, "location.reload();"));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(GURL(url::kAboutBlankURL), observer.last_navigation_url());
    EXPECT_NE(
        GURL(content::kUnreachableWebDataURL),
        web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
  }

  // In the above setup, the reload was carried out with the error page being
  // the initiator of the navigation.  The error page's origin is opaque with
  // a.com as the precursor, so this becomes the initiator origin for the
  // reload to about:blank.  This means that about:blank ought to load in a
  // SiteInstance and process corresponding to a.com.
  //
  // This covers an interesting and rare corner case, where an about:blank
  // navigation can't use the source SiteInstance, which would normally keep
  // it in the initiator's process and SiteInstance.  This is because the
  // reload originates from an error page process, which is incompatible with a
  // non-error navigation to about:blank.  In this case, the final SiteInstance
  // and process selection should still honor the initiator, rather than end up
  // in an unlocked process and an unassigned SiteInstance.  See
  // https://crbug.com/1426928.
  EXPECT_EQ(
      "http://a.com/",
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL());
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
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
  ExtensionTestMessageListener ready_listener("ready");
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // 2. Open a popup containing a cross-site subframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOpenerUrl));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();
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
  content::RenderFrameHost* popup_root = popup->GetPrimaryMainFrame();
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
  EXPECT_EQ(cross_site_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // 5. Verify that the about:blank URL is hosted in the same SiteInstance
  //    as the navigation initiator (and separate from the opener and the old
  //    popup SiteInstance).
  EXPECT_EQ(old_subframe_site_instance.get(), popup->GetSiteInstance());
  EXPECT_NE(url::kAboutBlankURL,
            popup->GetSiteInstance()->GetSiteURL().scheme());
  EXPECT_NE(url::kDataScheme, popup->GetSiteInstance()->GetSiteURL().scheme());
  if (content::AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_EQ(old_popup_site_instance.get(), popup->GetSiteInstance());
  } else {
    EXPECT_NE(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_NE(old_popup_site_instance.get(), popup->GetSiteInstance());

    // Verify that full isolation results in a separate process for each
    // SiteInstance. Otherwise they share a process because none of the sites
    // require a dedicated process.
    if (content::AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(opener->GetSiteInstance()->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
      EXPECT_NE(old_popup_site_instance->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
    } else {
      EXPECT_FALSE(opener->GetSiteInstance()->RequiresDedicatedProcess());
      EXPECT_FALSE(popup->GetSiteInstance()->RequiresDedicatedProcess());
      EXPECT_FALSE(old_popup_site_instance->RequiresDedicatedProcess());
      EXPECT_EQ(opener->GetSiteInstance()->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
      EXPECT_EQ(old_popup_site_instance->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
    }
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
  ExtensionTestMessageListener ready_listener("ready");
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // 2. Open a popup containing a cross-site subframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOpenerUrl));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();
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
  content::RenderFrameHost* popup_root = popup->GetPrimaryMainFrame();
  content::RenderFrameHost* cross_site_subframe = ChildFrameAt(popup_root, 0);
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
  EXPECT_TRUE(popup->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  // 5. Verify that with strict SiteInstances the data: URL is hosted in a brand
  //    new, separate SiteInstance (separate from the opener and the previous
  //    popup SiteInstance).
  if (content::AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_EQ(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_NE(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());
  } else {
    EXPECT_NE(opener->GetSiteInstance(), popup->GetSiteInstance());
    EXPECT_NE(old_popup_site_instance.get(), popup->GetSiteInstance());
    EXPECT_EQ(url::kDataScheme,
              popup->GetSiteInstance()->GetSiteURL().scheme());

    // Verify that full isolation results in a separate process for each
    // SiteInstance. Otherwise they share a process because none of the sites
    // require a dedicated process.
    if (content::AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(opener->GetSiteInstance()->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
      EXPECT_NE(old_popup_site_instance->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
    } else {
      EXPECT_FALSE(opener->GetSiteInstance()->RequiresDedicatedProcess());
      EXPECT_FALSE(popup->GetSiteInstance()->RequiresDedicatedProcess());
      EXPECT_FALSE(old_popup_site_instance->RequiresDedicatedProcess());
      EXPECT_EQ(opener->GetSiteInstance()->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
      EXPECT_EQ(old_popup_site_instance->GetProcess(),
                popup->GetSiteInstance()->GetProcess());
    }
  }
}

// This test covers a navigation that:
// 1. is initiated by a cross-site initiator,
// 2. is a history navigation,
// 3. gets redirected via webRequest API to a data: URL, but the original
// navigation (that created the history entry) didn't get redirected.
// This covers a scenario similar to the one that led to crashes in
// https://crbug.com/40065692.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       HistoryNavigationRedirectedToDataUrl) {
  const GURL kOpenerUrl(
      embedded_test_server()->GetURL("opener.com", "/title1.html"));
  const GURL kRedirectedUrl(
      embedded_test_server()->GetURL("redirected.com", "/title2.html"));
  const GURL kRedirectTargetUrl(
      "data:text/html,%3Ch1%3EHello%2C%20World!%3C%2Fh1%3E");
  const GURL kOtherUrl(
      embedded_test_server()->GetURL("other.com", "/title3.html"));

  // 1. Open a cross-site popup. Note that the navigation won't be
  //    redirected yet, because we haven't installed the redirector extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOpenerUrl));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();
  EXPECT_EQ(kOpenerUrl, opener->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(kOpenerUrl), opener->GetLastCommittedOrigin());
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener, content::JsReplace("var popup = window.open($1, 'my-popup')",
                                   kRedirectedUrl)));
    popup = popup_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup));
  }
  url::Origin first_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_FALSE(first_origin.opaque());
  scoped_refptr<content::SiteInstance> first_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());

  // 2. Navigate the popup elsewhere, so that we can do a back navigation.
  content::TestNavigationObserver nav_observer(popup, 1);
  ASSERT_TRUE(
      ExecJs(opener, content::JsReplace("popup.location = $1", kOtherUrl)));
  nav_observer.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 3. Install an extension, which will redirect all navigations to
  //    redirected.com URLs to a data: URL. In general, web servers cannot
  //    redirect to data: URLs, but extensions with webRequest API permissions
  //    can.
  const char kManifest[] = R"(
      {
        "name": "Test",
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
  ExtensionTestMessageListener ready_listener("ready");
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // 4. Do a history navigation to the redirected.com page, which will get
  //    redirected to a data: URL. This used to crash, see also
  //    https://crbug.com/40065692. The navigation should use a new opaque
  //    origin with the opener's origin as the precursor (since the initiator
  //    origin in the FrameNavigationEntry is still the opener's origin) and
  //    a new SiteInstance. Because the request is redirected, the saved
  //    PageState is reset.
  //    TODO(crbug.com/40266169): Reconsider whether we should keep
  //    using the initiator origin as the precursor here, since the data: URL
  //    redirection is triggered by the extension and isn't actually related to
  //    the initiator.
  content::TestNavigationObserver nav_observer2(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer2.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  url::Origin second_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_TRUE(second_origin.opaque());
  EXPECT_EQ(opener->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque(),
            second_origin.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_NE(first_origin, second_origin);
  scoped_refptr<content::SiteInstance> second_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(first_instance, second_instance);
  }

  // 5. Go forward.
  content::TestNavigationObserver nav_observer3(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.forward();"));
  nav_observer3.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 6. Go back again, and ensure we reuse the same SiteInstance as the last
  // time we navigated from it, but use a new opaque origin (with the precursor
  // still set to the opener origin).
  content::TestNavigationObserver nav_observer4(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer4.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());

  url::Origin third_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_NE(second_origin, third_origin);
  EXPECT_TRUE(third_origin.opaque());
  EXPECT_EQ(opener->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque(),
            third_origin.GetTupleOrPrecursorTupleIfOpaque());
  scoped_refptr<content::SiteInstance> third_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(first_instance, third_instance);
  }
  EXPECT_EQ(second_instance, third_instance);
}

// Same as above but the history navigation got redirected to about:blank
// instead.
// TODO(crbug.com/40266169): This is currently disabled because of
// a bug where we will reuse the previous SiteInstance on the about:blank
// navigation, even if the origins don't match, resulting in a CHECK failure
// during the redirect on step 4. See also the TODO with the same bug number in
// `SiteInstanceImpl::IsSameSite()`.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       DISABLED_HistoryNavigationRedirectedToAboutBlank) {
  const GURL kOpenerUrl(
      embedded_test_server()->GetURL("opener.com", "/title1.html"));
  const GURL kRedirectedUrl(
      embedded_test_server()->GetURL("redirected.com", "/title2.html"));
  const GURL kRedirectTargetUrl("about:blank");
  const GURL kOtherUrl(
      embedded_test_server()->GetURL("other.com", "/title3.html"));

  // 1. Open a cross-site popup. Note that the navigation won't be
  //    redirected yet, because we haven't installed the redirector extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOpenerUrl));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();
  EXPECT_EQ(kOpenerUrl, opener->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(kOpenerUrl), opener->GetLastCommittedOrigin());
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener, content::JsReplace("var popup = window.open($1, 'my-popup')",
                                   kRedirectedUrl)));
    popup = popup_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup));
  }
  url::Origin first_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_FALSE(first_origin.opaque());
  scoped_refptr<content::SiteInstance> first_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());

  // 2. Navigate the popup elsewhere, so that we can do a back navigation.
  content::TestNavigationObserver nav_observer(popup, 1);
  ASSERT_TRUE(
      ExecJs(opener, content::JsReplace("popup.location = $1", kOtherUrl)));
  nav_observer.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 3. Install an extension, which will redirect all navigations to
  //    redirected.com URLs to about:blank. In general, web servers cannot
  //    redirect to about:blank, but extensions with webRequest API permissions
  //    can.
  const char kManifest[] = R"(
      {
        "name": "Test",
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
  ExtensionTestMessageListener ready_listener("ready");
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // 4. Do a history navigation to the redirected.com page, which will get
  //    redirected to about:blank. The navigation will recalculate its
  //    origin, which will inherit from the initiator origin in the
  //    FrameNavigationEntry (the opener URL). The SiteInstance will also
  //    use the opener's SiteInstance.
  //    TODO(crbug.com/40266169): Reconsider whether we should keep
  //    inheriting the initiator origin here, since the about:blank redirection
  //    is triggered by the extension and isn't actually related to the
  //    initiator.
  content::TestNavigationObserver nav_observer2(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer2.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  url::Origin second_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_NE(first_origin, second_origin);
  EXPECT_EQ(second_origin, opener->GetLastCommittedOrigin());
  scoped_refptr<content::SiteInstance> second_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(first_instance, second_instance);
  }
  EXPECT_EQ(second_instance, opener->GetSiteInstance());

  // 5. Go forward.
  content::TestNavigationObserver nav_observer3(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.forward();"));
  nav_observer3.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 6. Go back again, and ensure we reuse the same SiteInstance and origin.
  content::TestNavigationObserver nav_observer4(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer4.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  EXPECT_EQ(second_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(second_instance, popup->GetPrimaryMainFrame()->GetSiteInstance());
}

// Same as above but the history navigation is same-site with the previous page,
// so the crash won't happen as the navigation is reusing the same SiteInstance.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       HistoryNavigationRedirectedToAboutBlank_SameSite) {
  const GURL kOpenerUrl(
      embedded_test_server()->GetURL("opener.com", "/title1.html"));
  const GURL kRedirectedUrl(
      embedded_test_server()->GetURL("redirected.com", "/title2.html"));
  const GURL kRedirectTargetUrl("about:blank");
  const GURL kOtherUrl(
      embedded_test_server()->GetURL("opener.com", "/title3.html"));

  // 1. Open a cross-site popup. Note that the navigation won't be
  //    redirected yet, because we haven't installed the redirector extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOpenerUrl));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();
  EXPECT_EQ(kOpenerUrl, opener->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(kOpenerUrl), opener->GetLastCommittedOrigin());
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener, content::JsReplace("var popup = window.open($1, 'my-popup')",
                                   kRedirectedUrl)));
    popup = popup_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup));
  }
  url::Origin first_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_FALSE(first_origin.opaque());
  scoped_refptr<content::SiteInstance> first_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());

  // 2. Navigate the popup elsewhere, so that we can do a back navigation.
  content::TestNavigationObserver nav_observer(popup, 1);
  ASSERT_TRUE(
      ExecJs(opener, content::JsReplace("popup.location = $1", kOtherUrl)));
  nav_observer.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 3. Install an extension, which will redirect all navigations to
  //    redirected.com URLs to about:blank. In general, web servers cannot
  //    redirect to about:blank, but extensions with webRequest API permissions
  //    can.
  const char kManifest[] = R"(
      {
        "name": "Test",
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
  ExtensionTestMessageListener ready_listener("ready");
  extensions::ChromeTestExtensionLoader extension_loader(browser()->profile());
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // 4. Do a history navigation to the redirected.com page, which will get
  //    redirected to about:blank. The navigation will recalculate its
  //    origin, which will inherit from the initiator origin in the
  //    FrameNavigationEntry (the opener URL). The SiteInstance will also
  //    use the opener's SiteInstance.
  //    TODO(crbug.com/40266169): Reconsider whether we should keep
  //    inheriting the initiator origin here, since the about:blank redirection
  //    is triggered by the extension and isn't actually related to the
  //    initiator.
  content::TestNavigationObserver nav_observer2(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer2.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  url::Origin second_origin =
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_NE(first_origin, second_origin);
  EXPECT_EQ(second_origin, opener->GetLastCommittedOrigin());
  scoped_refptr<content::SiteInstance> second_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(first_instance, second_instance);
  }
  EXPECT_EQ(second_instance, opener->GetSiteInstance());

  // 5. Go forward.
  content::TestNavigationObserver nav_observer3(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.forward();"));
  nav_observer3.Wait();
  EXPECT_EQ(kOtherUrl, popup->GetLastCommittedURL());

  // 6. Go back again, and ensure we reuse the same SiteInstance and origin.
  content::TestNavigationObserver nav_observer4(popup);
  ASSERT_TRUE(ExecJs(popup->GetPrimaryMainFrame(), "history.back();"));
  nav_observer4.Wait();
  EXPECT_EQ(kRedirectTargetUrl, popup->GetLastCommittedURL());
  EXPECT_EQ(second_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(second_instance, popup->GetPrimaryMainFrame()->GetSiteInstance());
}

// Tests scenario where a blank iframe inside a blank popup (a popup with only
// the initial navigation entry) does a same document navigation. This test was
// added as a regression test for crbug.com/1237874. The main purpose of this
// test is to ensure that WebContentsObservers and Chrome features don't crash.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       SameDocumentNavigationInIframeInBlankDocument) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();

  // 1. Create a new blank window that stays on the initial NavigationEntry.
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        opener,
        content::JsReplace("window.open($1, 'my-popup')",
                           embedded_test_server()->GetURL("/nocontent"))));
    popup = popup_observer.GetWebContents();
  }
  content::RenderFrameHost* popup_main_rfh = popup->GetPrimaryMainFrame();
  // Popup should be on the initial entry,
  content::NavigationEntry* last_entry =
      popup->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_entry->IsInitialEntry());

  // 2. Add blank iframe in popup.
  EXPECT_TRUE(content::ExecJs(popup_main_rfh,
                              "let iframe = document.createElement('iframe');"
                              "document.body.appendChild(iframe);"));

  // 3. Same-document navigation in iframe.
  {
    const GURL kSameDocUrl("about:blank#foo");
    content::TestNavigationManager navigation_manager(popup, kSameDocUrl);
    EXPECT_TRUE(content::ExecJs(
        popup_main_rfh, "document.querySelector('iframe').src = '#foo';"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  }

  // Check that same-document navigation doesn't commit a new navigation entry,
  // but instead reuses the last entry (which might be null).
  EXPECT_EQ(last_entry, popup->GetController().GetLastCommittedEntry());
}

// Test scenario where we attempt a synchronous renderer-initiated same-document
// navigation inside a blank popup (a popup with only the initial navigation
// entry). Regression test for crbug.com/1254238. The main purpose of this test
// is to ensure that WebContentsObservers and Chrome features don't crash.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       SameDocumentNavigationInBlankPopup) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::RenderFrameHost* opener = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetPrimaryMainFrame();

  // 1. Create a new blank window that will stay on the initial NavigationEntry.
  content::WebContents* popup = nullptr;
  {
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(opener, "var w = window.open('', 'my-popup')"));
    popup = popup_observer.GetWebContents();
  }
  // Popup should be on the initial entry.
  content::NavigationEntry* last_entry =
      popup->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(last_entry->IsInitialEntry());

  // 2. Same-document navigation in popup.
  {
    const GURL kSameDocUrl("about:blank#foo");
    content::TestNavigationManager navigation_manager(popup, kSameDocUrl);
    EXPECT_TRUE(
        content::ExecJs(opener, "w.history.replaceState({}, '', '#foo');"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  }

  // Check that same-document navigation doesn't commit a new navigation entry,
  // but instead reuses the last entry (which might be null).
  EXPECT_EQ(last_entry, popup->GetController().GetLastCommittedEntry());
}

class SignInIsolationBrowserTest : public ChromeNavigationBrowserTest {
 public:
  SignInIsolationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  SignInIsolationBrowserTest(const SignInIsolationBrowserTest&) = delete;
  SignInIsolationBrowserTest& operator=(const SignInIsolationBrowserTest&) =
      delete;

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::SiteInstance> first_instance(
      web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Make sure that a renderer-initiated navigation to the sign-in page swaps
  // processes.
  content::TestNavigationManager manager(web_contents, signin_url);
  EXPECT_TRUE(ExecJs(web_contents, "location = '" + signin_url.spec() + "';"));
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_NE(web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            first_instance);
}

class WebstoreIsolationBrowserTest : public ChromeNavigationBrowserTest {
 public:
  WebstoreIsolationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  WebstoreIsolationBrowserTest(const WebstoreIsolationBrowserTest&) = delete;
  WebstoreIsolationBrowserTest& operator=(const WebstoreIsolationBrowserTest&) =
      delete;

  ~WebstoreIsolationBrowserTest() override {}

  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    // Also serve files from the extensions test directory as it has a
    // /webstore/ directory, which the Webstore hosted app expects for the URL
    // it is associated with.
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/extensions");
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ChromeNavigationBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ignore cert errors so that the webstore URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames and standard ports in URLs (i.e.,
    // without having to inject the port number into all URLs). This is
    // important as the URL check to determine if a navigation is to/from the
    // new Webstore compares the full origin, which includes the scheme, host
    // and port.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + https_server_.host_port_pair().ToString());

    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ChromeNavigationBrowserTest::SetUpOnMainThread();
  }

  void OpenPopup(content::WebContents* creating_contents, GURL destination) {
    content::TestNavigationObserver popup_waiter(destination);
    popup_waiter.StartWatchingNewWebContents();
    EXPECT_TRUE(
        content::EvalJs(creating_contents,
                        content::JsReplace("!!window.open($1);", destination))
            .ExtractBool());
    popup_waiter.WaitForNavigationFinished();
    EXPECT_TRUE(popup_waiter.last_navigation_succeeded());
  }

 private:
  net::EmbeddedTestServer https_server_;
};

// Tests that Chrome Web Store URL used by the hosted app in production
// (chrome.google.com/webstore/) is isolated from the rest of google.com and
// other chrome.google.com pages not in the /webstore/ path. See
// https://crbug.com/939108.
IN_PROC_BROWSER_TEST_F(WebstoreIsolationBrowserTest, WebstorePopupIsIsolated) {
  const GURL first_url("https://google.com/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::SiteInstance> initial_instance(
      initial_web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Open a popup for chrome.google.com and ensure that it's isolated in a
  // different SiteInstance and process from the previous google.com page.
  const GURL webstore_origin_url("https://chrome.google.com/title1.html");
  OpenPopup(initial_web_contents, webstore_origin_url);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, initial_web_contents);
  EXPECT_EQ(webstore_origin_url, popup->GetLastCommittedURL());

  scoped_refptr<content::SiteInstance> popup_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(initial_instance, popup_instance);
  EXPECT_NE(initial_instance->GetProcess(), popup_instance->GetProcess());
  // This URL still does *not* match the web store URL due to it not having the
  // /webstore/ path, so there will not have been a full BrowsingInstance swap.
  EXPECT_TRUE(initial_instance->IsRelatedSiteInstance(popup_instance.get()));

  // Now navigate the popup to the full web store URL. This will again cause it
  // to be isolated in a different SiteInstance and process from the previous
  // pages, but also now cause a BrowsingInstance swap.
  const GURL webstore_url("https://chrome.google.com/webstore/mock_store.html");
  EXPECT_TRUE(content::NavigateToURLFromRenderer(popup, webstore_url));
  scoped_refptr<content::SiteInstance> webstore_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(webstore_instance, popup_instance);
  EXPECT_NE(webstore_instance, initial_instance);
  EXPECT_NE(webstore_instance->GetProcess(), initial_instance->GetProcess());
  EXPECT_NE(webstore_instance->GetProcess(), popup_instance->GetProcess());
  EXPECT_FALSE(webstore_instance->IsRelatedSiteInstance(popup_instance.get()));
  EXPECT_FALSE(
      webstore_instance->IsRelatedSiteInstance(initial_instance.get()));

  // Finally navigate the popup back away from the web store URL. This will lead
  // to another new process and BrowsingInstance swap.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(popup, first_url));
  scoped_refptr<content::SiteInstance> final_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(final_instance->GetProcess(), webstore_instance->GetProcess());
  EXPECT_FALSE(final_instance->IsRelatedSiteInstance(webstore_instance.get()));
}

// Make sure that the new Chrome Web Store URL used in production
// (chromewebstore.google.com) is isolated from the rest of the google.com
// domain.
IN_PROC_BROWSER_TEST_F(WebstoreIsolationBrowserTest,
                       NewWebstorePopupIsIsolated) {
  const GURL first_url("https://google.com/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::SiteInstance> initial_instance(
      initial_web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Open a popup for chromewebstore.google.com and ensure that it's isolated in
  // a different SiteInstance and process from the rest of google.com. Since the
  // new Webstore encompasses the entire subdomain, there should have also been
  // a BrowsingInstance swap at this point.
  const GURL webstore_origin_url(
      "https://chromewebstore.google.com/title1.html");
  OpenPopup(initial_web_contents, webstore_origin_url);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, initial_web_contents);
  EXPECT_EQ(webstore_origin_url, popup->GetLastCommittedURL());

  scoped_refptr<content::SiteInstance> popup_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(initial_instance, popup_instance);
  EXPECT_NE(initial_instance->GetProcess(), popup_instance->GetProcess());
  EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(popup_instance.get()));

  // Navigating the popup away from the webstore should cause another new
  // process and BrowsingInstance swap.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(popup, first_url));
  scoped_refptr<content::SiteInstance> final_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(final_instance->GetProcess(), popup_instance->GetProcess());
  EXPECT_FALSE(final_instance->IsRelatedSiteInstance(popup_instance.get()));
}

class WebstoreOverrideIsolationBrowserTest
    : public WebstoreIsolationBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the webstore URL. Note: although this specifies a path, in
    // reality we just look at the scheme, host and port when using the
    // override.
    command_line->AppendSwitchASCII(::switches::kAppsGalleryURL,
                                    "https://chrome.foo.com/frame_tree");

    WebstoreIsolationBrowserTest::SetUpCommandLine(command_line);
  }
};

// Make sure that Chrome Web Store origins are isolated from the rest of their
// site when overriding the URL from the command line. See
// https://crbug.com/939108.
IN_PROC_BROWSER_TEST_F(WebstoreOverrideIsolationBrowserTest,
                       WebstorePopupIsIsolated) {
  const GURL first_url("https://foo.com/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::SiteInstance> initial_instance(
      initial_web_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Open a popup for chrome.foo.com and ensure that it's isolated in a
  // different SiteInstance and process from the rest of foo.com. Since the
  // command line override applies to the entire subdomain, there should have
  // been a BrowsingInstance swap at this point.
  const GURL webstore_origin_url("https://chrome.foo.com/title1.html");
  OpenPopup(initial_web_contents, webstore_origin_url);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, initial_web_contents);
  EXPECT_EQ(webstore_origin_url, popup->GetLastCommittedURL());

  scoped_refptr<content::SiteInstance> popup_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(initial_instance, popup_instance);
  EXPECT_NE(initial_instance->GetProcess(), popup_instance->GetProcess());
  EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(popup_instance.get()));

  // Navigate the popup back away from the web store URL. This will lead
  // to another new process and BrowsingInstance swap.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(popup, first_url));
  scoped_refptr<content::SiteInstance> final_instance(
      popup->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(final_instance->GetProcess(), popup_instance->GetProcess());
  EXPECT_FALSE(final_instance->IsRelatedSiteInstance(popup_instance.get()));
}

// Check that it's possible to navigate to a chrome scheme URL from a crashed
// tab. See https://crbug.com/764641.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest, ChromeSchemeNavFromSadTab) {
  // Kill the renderer process.
  content::RenderProcessHost* process = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  // Attempt to navigate to a chrome://... URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_site_redirecting_url));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  Attach();
  base::Value::Dict params;
  params.Set("width", 400);
  params.Set("height", 800);
  params.Set("deviceScaleFactor", 1.0);
  params.Set("mobile", true);
  SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params));

  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));

  EXPECT_EQ(pdf_url, web_contents()->GetLastCommittedURL());
  EXPECT_EQ(
      "<head></head>"
      "<body><!-- no enabled plugin supports this MIME type --></body>",
      content::EvalJs(web_contents(), "document.documentElement.innerHTML")
          .ExtractString());
}

// Tests the behavior of cross origin redirection to a PDF with mobile emulation
// is enabled.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTestWithMobileEmulation,
                       CrossSiteRedirectionToPDFWithMobileEmulation) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  Attach();
  base::Value::Dict params;
  params.Set("width", 400);
  params.Set("height", 800);
  params.Set("deviceScaleFactor", 1.0);
  params.Set("mobile", true);
  SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params));

  GURL pdf_url = embedded_test_server()->GetURL("/pdf/test.pdf");
  GURL cross_site_redirecting_url =
      https_server.GetURL("/server-redirect?" + pdf_url.spec());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_site_redirecting_url));

  EXPECT_EQ(pdf_url, web_contents()->GetLastCommittedURL());
}

// Check that clicking on a link doesn't carry the transient user activation
// from the original page to the navigated page (crbug.com/865243).
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       WindowOpenBlockedAfterClickNavigation) {
  // Navigate to a test page with links.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/links.html")));

  // Click to navigate to title1.html.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  ASSERT_TRUE(
      ExecJs(main_contents, "document.getElementById('title1').click();"));
  observer.Wait();

  // Make sure popup attempt fails due to lack of transient user activation.
  EXPECT_EQ(false, content::EvalJs(main_contents, "!!window.open();",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            main_contents->GetLastCommittedURL());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       OpenerNavigation_DownloadPolicy_Disallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup.
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  static constexpr char kScriptFormat[] = "!!window.open('%s');";
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  content::TestNavigationObserver popup_waiter(nullptr, 1);
  popup_waiter.StartWatchingNewWebContents();
  EXPECT_EQ(true, content::EvalJs(
                      opener, base::StringPrintf(kScriptFormat,
                                                 popup_url.spec().c_str())));
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
  EXPECT_TRUE(content::ExecJs(
      popup,
      "window.opener.location ='data:html/text;base64,'+btoa('payload');"));

  ASSERT_TRUE(console_observer.Wait());
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin, 1);

  // Ensure that no download happened.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items;
  content::DownloadManager* manager =
      browser()->profile()->GetDownloadManager();
  manager->GetAllDownloads(&download_items);
  EXPECT_TRUE(download_items.empty());
}

// Opener navigations from a same-origin popup should be allowed.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       OpenerNavigation_DownloadPolicy_Allowed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup.
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  static constexpr char kScriptFormat[] = "!!window.open('%s');";
  GURL popup_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  content::TestNavigationObserver popup_waiter(nullptr, 1);
  popup_waiter.StartWatchingNewWebContents();
  EXPECT_EQ(true, content::EvalJs(
                      opener, base::StringPrintf(kScriptFormat,
                                                 popup_url.spec().c_str())));
  popup_waiter.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Using the popup, navigate its opener to a download.
  base::HistogramTester histograms;
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, opener);
  EXPECT_TRUE(WaitForLoadStop(popup));

  content::DownloadTestObserverInProgress observer(
      browser()->profile()->GetDownloadManager(), 1 /* wait_count */);
  EXPECT_TRUE(content::ExecJs(
      popup,
      "window.opener.location ='data:html/text;base64,'+btoa('payload');"));
  observer.WaitForFinished();

  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin, 0);

  // Delete any pending download.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items;
  content::DownloadManager* manager =
      browser()->profile()->GetDownloadManager();
  manager->GetAllDownloads(&download_items);
  for (download::DownloadItem* item : download_items) {
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/click-noreferrer-links.html")));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), skippable_url));

  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer without a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecJs(main_contents,
                     "location = '" + redirected_url.spec() + "';",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));
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
  back_model->MenuWillShow();
  back_model->MenuWillClose();
  back_model->ActivatedAt(0);
  histogram.ExpectTotalCount(
      "Navigation.BackForward.TimeFromOpenBackNavigationMenuToActivateItem", 1);
  histogram.ExpectTotalCount(
      "Navigation.BackForward.TimeFromOpenBackNavigationMenuToCloseMenu", 1);
}

// Same as above except the navigation is cross-site.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NoUserActivationSetSkipOnBackForwardCrossSite) {
  GURL skippable_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), skippable_url));

  GURL redirected_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  {
    // Navigate to a new document from the renderer without a user gesture.
    content::WebContents* main_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(main_contents);
    EXPECT_TRUE(ExecJs(main_contents,
                       "location = '" + redirected_url.spec() + "';",
                       content::EXECUTE_SCRIPT_NO_USER_GESTURE));
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

// Verify that profile shutdown cancels an ongoing navigation for a WebContents
// in that profile, even if the shutdown logic forgets to clean up the
// WebContents itself (which would normally cancel all navigations in it). See
// https://crbug.com/40274462.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       NavigationCanceledOnProfileShutdown) {
  Browser* incognito = CreateIncognitoBrowser();
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  ASSERT_TRUE(incognito_profile);

  // Create a custom WebContents in which to perform a navigation. Note that we
  // explicitly do not use GetActiveWebContents() from the `incognito` browser,
  // since we will be closing that window to shut down the profile, and that
  // will destroy all of its tabs and WebContents, which also implicitly cancels
  // navigations. The purpose of this test is to test the fallback logic for
  // navigations in WebContents that isn't closed this way.
  std::unique_ptr<content::WebContents> incognito_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(incognito_profile));

  // Start a second navigation but don't let it proceed past the request start
  // stage.
  GURL url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  content::TestNavigationManager manager(incognito_contents.get(), url);
  incognito_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Destroy the incognito profile. This should trigger navigation cancellation,
  // which should dispatch DidFinishNavigation.
  ProfileDestructionWaiter profile_destruction_waiter(incognito_profile);
  bool was_navigation_canceled = false;
  content::DidFinishNavigationObserver observer(
      incognito_contents.get(),
      base::BindLambdaForTesting(
          [&](content::NavigationHandle* navigation_handle) {
            if (navigation_handle->GetURL() != url) {
              return;
            }
            EXPECT_FALSE(navigation_handle->HasCommitted());
            was_navigation_canceled = true;
          }));
  incognito->window()->Close();
  profile_destruction_waiter.Wait();

  // Make sure the navigation was canceled during profile destruction.
  ASSERT_TRUE(was_navigation_canceled);

  // The `incognito_contents` wasn't destroyed as part of closing the normal
  // incognito window since we created it manually. Ensure it's destroyed now.
  incognito_contents.reset();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_start));

  // No sad tab should be visible after a successful navigation.
  ASSERT_FALSE(sad_tab_helper->sad_tab());

  // Kill the renderer process.
  content::RenderProcessHost* process =
      contents->GetPrimaryMainFrame()->GetProcess();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_succeed));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_start));

  // No sad tab should be visible after a successful navigation.
  ASSERT_FALSE(sad_tab_helper->sad_tab());

  // Kill the renderer process.
  content::RenderProcessHost* process =
      contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  // Make sure the sad tab is shown.
  ASSERT_TRUE(sad_tab_helper->sad_tab());

  // Make sure the sad tab goes away when we commit successfully.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_succeed));
  EXPECT_FALSE(sad_tab_helper->sad_tab());
}

// Flaky, see https://crbug.com/1223052 and https://crbug.com/1236500.
// Ensure that completing a navigation from a sad tab will clear the sad tab.
IN_PROC_BROWSER_TEST_F(ChromeNavigationBrowserTest,
                       DISABLED_ClearSadTabWhenNavigationCompletes_CrossSite) {
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
// https://crbug.com/1283289 Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NavigationConsumesUserGesture_Fullscreen \
  DISABLED_NavigationConsumesUserGesture_Fullscreen
#else
#define MAYBE_NavigationConsumesUserGesture_Fullscreen \
  NavigationConsumesUserGesture_Fullscreen
#endif
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest,
                       MAYBE_NavigationConsumesUserGesture_Fullscreen) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/navigation_consumes_gesture.html")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Normally, fullscreen should work, as long as there is a user gesture.
  EXPECT_EQ(true, content::EvalJs(contents,
                                  "document.body.webkitRequestFullscreen();"
                                  "resultQueue.pop();"));

  EXPECT_EQ(false, content::EvalJs(contents,
                                   "document.webkitExitFullscreen();"
                                   "resultQueue.pop();"));

  // However, starting a navigation should consume the gesture. Fullscreen
  // should not work afterwards. Make sure the navigation is synchronously
  // started via click().
  std::string script = R"(
    document.getElementsByTagName('a')[0].click();
    document.body.webkitRequestFullscreen();
    resultQueue.pop();
  )";

  // Use the TestNavigationManager to ensure the navigation is not finished
  // before fullscreen can occur.
  content::TestNavigationManager nav_manager(
      contents, embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(false, content::EvalJs(contents, script));
}

// Similar to the fullscreen test above, but checks that popups are successfully
// blocked if spawned after a navigation.
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest,
                       NavigationConsumesUserGesture_Popups) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/links.html")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Normally, a popup should open fine if it is associated with a user gesture.
  EXPECT_EQ(true, content::EvalJs(contents, "!!window.open();"));

  // Starting a navigation should consume a gesture, but make sure that starting
  // a same-document navigation doesn't do the consuming.
  std::string same_document_script = R"(
    document.getElementById("ref").click();
    !!window.open();
  )";
  EXPECT_EQ(true, content::EvalJs(contents, same_document_script));

  // If the navigation is to a different document, the gesture should be
  // successfully consumed.
  std::string different_document_script = R"(
    document.getElementById("title1").click();
    !!window.open();
  )";
  EXPECT_EQ(false, content::EvalJs(contents, different_document_script));
}

// Regression test for https://crbug.com/856779, where a navigation to a
// top-level, same process frame in another tab fails to focus that tab.
IN_PROC_BROWSER_TEST_F(NavigationConsumingTest, TargetNavigationFocus) {
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/link_with_target.html")));

  {
    content::TestNavigationObserver new_tab_observer(nullptr, 1);
    new_tab_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(
        ExecJs(opener, "document.getElementsByTagName('a')[0].click();"));
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
    ASSERT_TRUE(
        ExecJs(opener, "document.getElementsByTagName('a')[0].click();"));
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
  EXPECT_TRUE(ExecJs(main_contents,
                     "location = '" + redirected_url.spec() + "';",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(manager.WaitForNavigationFinished());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), skippable_url));

  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a new document from the renderer without a user gesture.
  GURL redirected_url(embedded_test_server()->GetURL("/title2.html"));
  content::TestNavigationManager manager(main_contents, redirected_url);
  EXPECT_TRUE(ExecJs(main_contents,
                     "location = '" + redirected_url.spec() + "';",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(manager.WaitForNavigationFinished());
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

#if BUILDFLAG(ENABLE_PDF)
// Tests that a main frame hosting pdf does not get skipped because of history
// manipulation intervention if there was a user gesture.
// TODO(crbug.com/333829580): Flaky.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       DISABLED_PDFDoNotSkipOnBackForwardDueToUserGesture) {
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer with a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecJs(main_contents, "location = '" + url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(url, main_contents->GetLastCommittedURL());

  // Since pdf_url initiated a navigation with a user gesture, it will
  // not be skipped. Going back should be allowed and should navigate to
  // pdf_url.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BACK));

  ASSERT_TRUE(chrome::CanGoBack(browser()));

  content::TestNavigationObserver go_back_observer(main_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  go_back_observer.WaitForNavigationFinished();
  ASSERT_EQ(pdf_url, main_contents->GetLastCommittedURL());
}

// Tests that a main frame hosting pdf gets skipped because of history
// manipulation intervention if there was no user gesture.
// TODO(crbug.com/333829580): Flaky.
IN_PROC_BROWSER_TEST_F(HistoryManipulationInterventionBrowserTest,
                       DISABLED_PDFSkipOnBackForwardNoUserGesture) {
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a new document from the renderer without a user gesture.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(main_contents);
  EXPECT_TRUE(ExecJs(main_contents, "location = '" + url.spec() + "';",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
  EXPECT_EQ(url, main_contents->GetLastCommittedURL());

  // Since pdf_url initiated a navigation without a user gesture, it will
  // be skipped. Going back should be allowed and should navigate to
  // about:blank.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BACK));

  ASSERT_TRUE(chrome::CanGoBack(browser()));
  content::TestNavigationObserver go_back_observer(main_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  go_back_observer.WaitForNavigationFinished();
  ASSERT_EQ(GURL("about:blank"), main_contents->GetLastCommittedURL());
}
#endif  // BUILDFLAG(ENABLE_PDF)

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

  void StartIsolatingSite(Profile* profile, const GURL& url) {
    content::SiteInstance::StartIsolatingSite(
        profile, url,
        content::ChildProcessSecurityPolicy::IsolatedOriginSource::
            USER_TRIGGERED);
  }

  std::vector<std::string> GetSavedIsolatedSites() {
    return GetSavedIsolatedSites(browser()->profile());
  }

  std::vector<std::string> GetSavedIsolatedSites(Profile* profile) {
    PrefService* prefs = profile->GetPrefs();
    auto& list =
        prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins);
    std::vector<std::string> sites;
    for (const base::Value& value : list)
      sites.push_back(value.GetString());
    return sites;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);

    // Set up the embedded HTTPS test server and set all hostnames used by
    // test cases.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    embedded_https_test_server().SetCertHostnames(
        {"isolated1.com", "isolated2.com", "sub.foo.com", "bar.com",
         "saved.com", "saved2.com", "foo.com"});
    ASSERT_TRUE(embedded_https_test_server().Start());

    // This simulates a whitelist of isolated sites.
    std::string origin_list =
        embedded_https_test_server().GetURL("isolated1.com", "/").spec() + "," +
        embedded_https_test_server().GetURL("isolated2.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
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

  GURL url(embedded_https_test_server().GetURL("sub.foo.com",
                                               "/password/password_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // foo.com should not be isolated to start with. Verify that a cross-site
  // iframe does not become an OOPIF.
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
  std::string kAppendIframe = R"(
      var i = document.createElement('iframe');
      i.id = 'child';
      document.body.appendChild(i);)";
  EXPECT_TRUE(ExecJs(contents, kAppendIframe));
  GURL bar_url(embedded_https_test_server().GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  content::RenderFrameHost* child =
      ChildFrameAt(contents->GetPrimaryMainFrame(), 0);
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
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  EXPECT_TRUE(ExecJs(contents, kAppendIframe));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  child = ChildFrameAt(contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(child->IsCrossProcessSubframe());

  // Open a fresh tab (also forcing a new BrowsingInstance), navigate to
  // foo.com, and verify that a cross-site iframe becomes an OOPIF.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(ExecJs(new_contents, kAppendIframe));
  EXPECT_TRUE(NavigateIframeToURL(new_contents, "child", bar_url));
  content::RenderFrameHost* new_child =
      ChildFrameAt(new_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(new_child->IsCrossProcessSubframe());
}

// Verifies that persistent isolated sites survive restarts.  Part 1.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       PRE_IsolatedSitesPersistAcrossRestarts) {
  // There shouldn't be any saved isolated origins to start with.
  EXPECT_THAT(GetSavedIsolatedSites(), IsEmpty());

  // Isolate saved.com and saved2.com persistently.
  GURL saved_url(
      embedded_https_test_server().GetURL("saved.com", "/title1.html"));
  StartIsolatingSite(browser()->profile(), saved_url);
  GURL saved2_url(
      embedded_https_test_server().GetURL("saved2.com", "/title1.html"));
  StartIsolatingSite(browser()->profile(), saved2_url);

  // Check that saved.com utilizes a dedicated process in future navigations.
  // Open a new tab to force creation of a new BrowsingInstance.
  AddBlankTabAndShow(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // Check that saved.com and saved2.com were saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com", "https://saved2.com"));
}

// Verifies that process-isolated sites persist across restarts.  Part 2.
// This runs after Part 1 above and in the same profile.  Part 1 has already
// added "saved.com" as a persisted isolated origin, so this part verifies that
// it requires a dedicated process after restart.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IsolatedSitesPersistAcrossRestarts) {
  // Check that saved.com and saved2.com are still saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com", "https://saved2.com"));

  // Check that these sites utilize a dedicated process after restarting, but a
  // non-isolated foo.com URL does not.
  GURL saved_url(
      embedded_https_test_server().GetURL("saved.com", "/title1.html"));
  GURL saved2_url(
      embedded_https_test_server().GetURL("saved2.com", "/title2.html"));
  GURL foo_url(embedded_https_test_server().GetURL("foo.com", "/title3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved2_url));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo_url));
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
}

// Verify that trying to isolate a site multiple times will only save it to
// disk once.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IsolatedSiteIsSavedOnlyOnce) {
  GURL saved_url(
      embedded_https_test_server().GetURL("saved.com", "/title1.html"));
  StartIsolatingSite(browser()->profile(), saved_url);
  StartIsolatingSite(browser()->profile(), saved_url);
  StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));
}

// Check that Incognito doesn't inherit saved isolated origins from its
// original profile, and that any isolated origins added in Incognito don't
// affect the original profile.
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       IncognitoWithIsolatedSites) {
  // Isolate saved.com and verify it's been saved to disk.
  GURL saved_url(
      embedded_https_test_server().GetURL("saved.com", "/title1.html"));
  StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));

  // Create an incognito browser and browse to saved.com.  Verify that it's
  // *not* isolated in incognito.
  //
  // TODO(alexmos): This might change in the future if we decide to inherit
  // main profile's isolated origins in incognito. See
  // https://crbug.com/905513.
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, saved_url));
  content::WebContents* contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());

  // Add an isolated site in incognito, and verify that while future
  // navigations to this site in incognito require a dedicated process,
  // navigations to this site in the main profile do not require a dedicated
  // process, and the site is not persisted for either the main or incognito
  // profiles.
  GURL foo_url(embedded_https_test_server().GetURL("foo.com", "/title1.html"));
  StartIsolatingSite(incognito->profile(), foo_url);

  AddBlankTabAndShow(incognito);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, foo_url));
  contents = incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  AddBlankTabAndShow(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo_url));
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());

  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              testing::Not(testing::Contains("https://foo.com")));
  EXPECT_THAT(GetSavedIsolatedSites(incognito->profile()),
              testing::Not(testing::Contains("https://foo.com")));
}

// Verify that serving a Clear-Site-Data header does not clear saved isolated
// sites.  Saved isolated sites should only be cleared by user-initiated
// actions. (Note: Clear-Site-Data is only available on HTTPS URLs.)
IN_PROC_BROWSER_TEST_F(SiteIsolationForPasswordSitesBrowserTest,
                       ClearSiteDataDoesNotClearSavedIsolatedSites) {
  // Isolate saved.com and verify it's been saved to disk.
  GURL saved_url(embedded_https_test_server().GetURL("saved.com",
                                                     "/clear_site_data.html"));
  StartIsolatingSite(browser()->profile(), saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));

  // Navigate to a URL that serves a Clear-Site-Data header for cache, cookies,
  // and DOM storage. This is the most that a Clear-Site-Data header could
  // clear, and this should not clear saved isolated sites.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved_url));
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));
}

// This test class turns on the feature to dynamically isolate sites where the
// user logs in via OAuth. This also requires enabling OAuth login detection
// (which is used by other features as well) and disabling strict site
// isolation (so that OAuth isolation can be observed on desktop platforms).
class SiteIsolationForOAuthSitesBrowserTest
    : public ChromeNavigationBrowserTest {
 public:
  SiteIsolationForOAuthSitesBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {login_detection::kLoginDetection,
         site_isolation::features::kSiteIsolationForOAuthSites},
        {features::kSitePerProcess});
  }

  using IsolatedOriginSource =
      content::ChildProcessSecurityPolicy::IsolatedOriginSource;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);

    // Allow HTTPS server to be used on sites other than localhost.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ChromeNavigationBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ChromeNavigationBrowserTest::SetUpOnMainThread();
  }

  // Login detection only works for HTTPS sites.
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

// Simulate a popup-based OAuth login flow, where a client opens a popup to log
// in via OAuth.  Ensure that the client's site becomes isolated when the OAuth
// login completes.
IN_PROC_BROWSER_TEST_F(SiteIsolationForOAuthSitesBrowserTest, PopupFlow) {
  // Navigate to the OAuth requestor.  It shouldn't be isolated yet.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("www.oauthclient.com", "/title1.html")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsProcessLockedToSiteForTesting());

  using IsolatedOriginSource =
      content::ChildProcessSecurityPolicy::IsolatedOriginSource;
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://oauthclient.com")),
      IsolatedOriginSource::USER_TRIGGERED));

  // Create a popup that emulates an OAuth sign-in flow.
  content::WebContentsAddedObserver web_contents_added_observer;
  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::JsReplace(
          "window.open($1, 'oauth_window', 'width=10,height=10');",
          https_server()->GetURL("www.oauthprovider.com",
                                 "/title2.html?client_id=123"))));
  auto* popup_contents = web_contents_added_observer.GetWebContents();
  navigation_observer.WaitForNavigationFinished();

  // When the popup is closed, it will be detected as an OAuth login.
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecJs(popup_contents, "window.close()"));
  destroyed_watcher.Wait();

  // oauthclient.com should now be isolated. Check that it's now registered
  // with ChildProcessSecurityPolicy (with its eTLD+1).
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://oauthclient.com")),
      IsolatedOriginSource::USER_TRIGGERED));

  // Check that oauthclient.com navigations are site-isolated in future
  // BrowsingInstances. Note that because there are no other window references
  // at this point, a new navigation in the main window should force a
  // BrowsingInstance swap to apply the new isolation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("www2.oauthclient.com", "/title1.html")));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
}

// Similar to previous test, but simulate a same-window OAuth login flow, where
// a client navigates directly to the OAuth provider, which will
// navigate/redirect back to the client when the login flow completes.
//
// Part 2 of this test also verifies that OAuth site isolation persists across
// restarts.
IN_PROC_BROWSER_TEST_F(SiteIsolationForOAuthSitesBrowserTest,
                       PRE_RedirectFlow) {
  // Navigate to the OAuth requestor.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("oauthclient.com", "/title1.html")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsProcessLockedToSiteForTesting());

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://oauthclient.com")),
      IsolatedOriginSource::USER_TRIGGERED));

  // Use an interceptor to allow referencing arbitrary paths on
  // oauthprovider.com without worrying that corresponding test files exist.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "oauthprovider.com") {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/title2.html", params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  // Simulate start of OAuth login.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("oauthprovider.com",
                                        "/authenticate?client_id=123")));

  // Simulate another OAuth login step.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("oauthprovider.com",
                                        "/another_stage?client_id=123")));

  // Simulate completion of OAuth login.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("oauthclient.com", "/title2.html?code=secret")));

  // oauthclient.com should now be isolated. Check that it's now registered
  // with ChildProcessSecurityPolicy.
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://oauthclient.com")),
      IsolatedOriginSource::USER_TRIGGERED));

  // Check that oauthclient.com navigations are site-isolated in future
  // BrowsingInstances. Open a new unrelated window, which forces a new
  // BrowsingInstance.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("oauthclient.com", "/title1.html")));
  EXPECT_TRUE(new_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
}

// See part 1 of the test above.  This is part 2, which verifies that OAuth
// site isolation persists across restarts.
IN_PROC_BROWSER_TEST_F(SiteIsolationForOAuthSitesBrowserTest, RedirectFlow) {
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://oauthclient.com")),
      IsolatedOriginSource::USER_TRIGGERED));

  // By the time this test starts running, there should be one sample recorded
  // for one saved OAuth site.
  histograms_.ExpectBucketCount("SiteIsolation.SavedOAuthSites.Size", 1, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("oauthclient.com", "/title1.html")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
}

// This test class turns on the mode where sites served with
// Cross-Origin-Opener-Policy headers are site-isolated.  This complements
// COOPIsolationTest in content_browsertests and focuses on persistence of COOP
// sites in user prefs, which requires the //chrome layer.
class SiteIsolationForCOOPBrowserTest : public ChromeNavigationBrowserTest {
 public:
  // Use an HTTP server, since the COOP header is only populated for HTTPS.
  SiteIsolationForCOOPBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Enable COOP isolation with a max of 3 stored sites.
    const std::vector<base::test::FeatureRefAndParams> kEnabledFeatures = {
        {::features::kSiteIsolationForCrossOriginOpenerPolicy,
         {{"stored_sites_max_size", base::NumberToString(3)},
          {"should_persist_across_restarts", "true"}}}};
    // Disable full site isolation so we can observe effects of COOP isolation.
    const std::vector<base::test::FeatureRef> kDisabledFeatures = {
        features::kSitePerProcess};
    feature_list_.InitWithFeaturesAndParameters(kEnabledFeatures,
                                                kDisabledFeatures);
  }

  // Returns the list of COOP sites currently stored in user prefs.
  std::vector<std::string> GetSavedIsolatedSites(Profile* profile) {
    PrefService* prefs = profile->GetPrefs();
    auto& dict =
        prefs->GetDict(site_isolation::prefs::kWebTriggeredIsolatedOrigins);
    std::vector<std::string> sites;
    for (auto site_time_pair : dict)
      sites.push_back(site_time_pair.first);
    return sites;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeNavigationBrowserTest::SetUpCommandLine(command_line);

    // Allow HTTPS server to be used on sites other than localhost.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ChromeNavigationBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ChromeNavigationBrowserTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

// Verifies that sites isolated due to COOP headers are persisted across
// restarts.  Note that persistence requires both visiting the COOP site and
// interacting with it via a user activation.  Part 1/2.
IN_PROC_BROWSER_TEST_F(SiteIsolationForCOOPBrowserTest,
                       PRE_PersistAcrossRestarts) {
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()), IsEmpty());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a couple of URLs with COOP and trigger user activation on each
  // one to add them to the saved list in user prefs.
  GURL coop_url = https_server()->GetURL(
      "saved.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  GURL coop_url2 = https_server()->GetURL(
      "saved2.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop_url));
  // Simulate user activation.
  EXPECT_TRUE(ExecJs(contents, "// no-op"));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop_url2));
  // Simulate user activation.
  EXPECT_TRUE(ExecJs(contents, "// no-op"));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // Check that saved.com and saved2.com were saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              UnorderedElementsAre("https://saved.com", "https://saved2.com"));
}

// Verifies that sites isolated due to COOP headers with a user activation are
// persisted across restarts.  Part 2/2.
IN_PROC_BROWSER_TEST_F(SiteIsolationForCOOPBrowserTest, PersistAcrossRestarts) {
  // Check that saved.com and saved2.com are still saved after a restart.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              UnorderedElementsAre("https://saved.com", "https://saved2.com"));

  // Check that these sites have been loaded as isolated on startup and utilize
  // a dedicated process after restarting even without serving COOP headers.
  GURL saved_url(https_server()->GetURL("saved.com", "/title1.html"));
  GURL saved2_url(https_server()->GetURL("saved2.com", "/title2.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), saved2_url));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // Sanity check that an unrelated non-isolated foo.com URL does not require a
  // dedicated process.
  GURL foo_url(https_server()->GetURL("foo.com", "/title3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo_url));
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
}

// Check that COOP sites are not persisted in Incognito; the isolation should
// only persist for the duration of the Incognito session.
IN_PROC_BROWSER_TEST_F(SiteIsolationForCOOPBrowserTest, Incognito) {
  Browser* incognito = CreateIncognitoBrowser();

  GURL coop_url = https_server()->GetURL(
      "foo.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, coop_url));
  content::WebContents* contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  // Simulate user activation to isolate foo.com for the rest of the incognito
  // session.
  EXPECT_TRUE(ExecJs(contents, "// no-op"));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // Check that navigations to foo.com (even without COOP) are isolated in
  // future BrowsingInstances in Incognito.
  AddBlankTabAndShow(incognito);
  GURL foo_url = https_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, foo_url));
  contents = incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // foo.com should not be isolated in the regular profile.
  AddBlankTabAndShow(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo_url));
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());

  // Neither profile should've saved foo.com to COOP isolated sites prefs.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()), IsEmpty());
  EXPECT_THAT(GetSavedIsolatedSites(incognito->profile()), IsEmpty());
}

// Verify that when a COOP-isolated site is visited again, the timestamp in its
// stored pref entry is updated correctly and taken into consideration when
// trimming the list of stored COOP sites to its maximum size.
IN_PROC_BROWSER_TEST_F(SiteIsolationForCOOPBrowserTest,
                       TimestampUpdateOnSecondVisit) {
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()), IsEmpty());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const std::string kCoopPath =
      "/set-header?Cross-Origin-Opener-Policy: same-origin";
  GURL coop1 = https_server()->GetURL("coop1.com", kCoopPath);
  GURL coop2 = https_server()->GetURL("coop2.com", kCoopPath);
  GURL coop3 = https_server()->GetURL("coop3.com", kCoopPath);
  GURL coop4 = https_server()->GetURL("coop4.com", kCoopPath);

  // Navigate to three COOP sites and trigger user actuvation on each one to
  // add them all to the list of persistently isolated COOP sites.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop1));
  EXPECT_TRUE(ExecJs(contents, "// no-op"));  // Simulate user activation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop2));
  EXPECT_TRUE(ExecJs(contents, "// no-op"));  // Simulate user activation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop3));
  EXPECT_TRUE(ExecJs(contents, "// no-op"));  // Simulate user activation.

  // At this point, the first three sites should be saved to prefs.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              UnorderedElementsAre("https://coop1.com", "https://coop2.com",
                                   "https://coop3.com"));

  // Visit coop1.com again.  This should update its timestamp to be more recent
  // than coop2.com and coop3.com.  The set of saved sites shouldn't change.
  AddBlankTabAndShow(browser());
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop1));
  EXPECT_TRUE(ExecJs(contents, "// no-op"));  // Simulate user activation.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              UnorderedElementsAre("https://coop1.com", "https://coop2.com",
                                   "https://coop3.com"));

  // Now, visit coop4.com.  Since the maximum number of saved COOP sites is 3
  // in this test, the oldest site should be evicted.  That evicted site should
  // be coop2.com, since coop1.com's timestamp was just updated.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), coop4));
  EXPECT_TRUE(ExecJs(contents, "// no-op"));  // Simulate user activation.
  EXPECT_THAT(GetSavedIsolatedSites(browser()->profile()),
              UnorderedElementsAre("https://coop1.com", "https://coop3.com",
                                   "https://coop4.com"));
}
