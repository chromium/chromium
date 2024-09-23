// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::HasSubstr;
using testing::ContainsRegex;

namespace {
const char kTestHtml[] = "/viewsource/test.html";
const char kTestNavigationHtml[] = "/viewsource/navigation.html";
const char kTestMedia[] = "/media/pink_noise_140ms.wav";
}

class ViewSourceTest : public InProcessBrowserTest {
 public:
  ViewSourceTest() {
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

  ViewSourceTest(const ViewSourceTest&) = delete;
  ViewSourceTest& operator=(const ViewSourceTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ViewSourcePermissionsPolicyTest : public ViewSourceTest {
 public:
  ViewSourcePermissionsPolicyTest() : ViewSourceTest() {}

  ViewSourcePermissionsPolicyTest(const ViewSourcePermissionsPolicyTest&) =
      delete;
  ViewSourcePermissionsPolicyTest& operator=(
      const ViewSourcePermissionsPolicyTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// This test renders a page in view-source and then checks to see if the title
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source).
// Flaky; see http://crbug.com/72201.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserRenderInViewSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our view-source test page.
  GURL url(content::kViewSourceScheme + std::string(":") +
           embedded_test_server()->GetURL(kTestHtml).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the title didn't get set.  It should not be there (because we
  // are in view-source mode).
  EXPECT_NE(u"foo",
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// This test renders a page normally and then renders the same page in
// view-source mode. This is done since we had a problem at one point during
// implementation of the view-source: prefix being consumed (removed from the
// URL) if the URL was not changed (apart from adding the view-source prefix)
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserConsumeViewSourcePrefix) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to google.html.
  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Then we navigate to the same url but with the "view-source:" prefix.
  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_viewsource));

  // The URL should still be prefixed with "view-source:".
  EXPECT_EQ(url_viewsource.spec(),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());
}

// Make sure that when looking at the actual page, we can select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuEnabledOnANormalPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(chrome::CanViewSource(browser()));
}

// For page that is media content, make sure that we cannot select "View Source"
// See http://crbug.com/83714
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuDisabledOnAMediaPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestMedia));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const char* mime_type = browser()->tab_strip_model()->GetActiveWebContents()->
      GetContentsMimeType().c_str();

  EXPECT_STREQ("audio/wav", mime_type);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Make sure that when looking at the page source, we can't select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceInMenuDisabledWhileViewingSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_viewsource));

  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Tests that reload initiated by the script on the view-source page leaves
// the page in view-source mode.
// Times out on Mac, Windows, ChromeOS Linux: crbug.com/162080
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DISABLED_TestViewSourceReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());

  content::LoadStopObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_viewsource));
  observer.Wait();

  ASSERT_TRUE(content::ExecJs(browser()->tab_strip_model()->GetWebContentsAt(0),
                              "window.location.reload();"));

  content::LoadStopObserver observer2(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  observer2.Wait();
  ASSERT_TRUE(browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetController().GetActiveEntry()->IsViewSourceMode());
}

// This test ensures that view-source session history navigations work
// correctly when switching processes. See https://crbug.com/544868.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceCrossProcessAndBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_viewsource));
  EXPECT_FALSE(chrome::CanViewSource(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open another tab to the same origin, so the process is kept alive while
  // the original tab is navigated cross-process. This is required for the
  // original bug to reproduce.
  {
    GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
    ui_test_utils::UrlLoadObserver load_complete(url);
    EXPECT_TRUE(
        content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "window.open('" + url.spec() + "');"));
    load_complete.Wait();
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
  }

  // Switch back to the first tab and navigate it cross-process.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_TRUE(chrome::CanViewSource(browser()));

  // Navigate back in session history to ensure view-source mode is still
  // active.
  {
    ui_test_utils::UrlLoadObserver load_complete(url_viewsource);
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    load_complete.Wait();
  }

  // Check whether the page is in view-source mode or not by checking if an
  // expected element on the page exists or not. In view-source mode it
  // should not be found.
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('bar') === null;"));
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Tests that view-source mode of b.com subframe won't commit in an a.com (main
// frame) process.  This is a regresion test for https://crbug.com/770946.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, CrossSiteSubframe) {
  // Navigate to a page with a cross-site frame.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Grab the original frames.
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* original_main_frame =
      original_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* original_child_frame =
      ChildFrameAt(original_main_frame, 0);
  ASSERT_TRUE(original_child_frame);

  // Do a sanity check that in this particular test page the main frame and the
  // subframe are cross-site.
  EXPECT_NE(
      original_main_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL(),
      original_child_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL());
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(original_main_frame->GetSiteInstance(),
              original_child_frame->GetSiteInstance());
    EXPECT_NE(original_main_frame->GetProcess()->GetID(),
              original_child_frame->GetProcess()->GetID());
  }

  // Open view-source mode tab for the subframe.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  original_child_frame->ViewSource();

  // Grab the view-source frame and wait for load stop.
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  content::RenderFrameHost* view_source_frame =
      view_source_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify that the last committed URL is the same in the original and the
  // view-source frames.
  EXPECT_EQ(original_child_frame->GetLastCommittedURL(),
            view_source_frame->GetLastCommittedURL());

  // Verify that the original main frame and the view-source subframe are in a
  // different process (e.g. if the main frame was malicious and the subframe
  // was an isolated origin, then the malicious frame shouldn't be able to see
  // the contents of the isolated document).  See https://crbug.com/770946.
  EXPECT_NE(original_main_frame->GetSiteInstance(),
            view_source_frame->GetSiteInstance());

  // Verify that the original subframe and the view-source subframe are in a
  // different process - see https://crbug.com/699493.
  EXPECT_NE(original_child_frame->GetSiteInstance(),
            view_source_frame->GetSiteInstance());

  // Verify the contents of the view-source tab (should match title1.html).
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      output; )";
  EXPECT_EQ(
      "<html><head></head><body>This page has no title.</body></html>",
      content::EvalJs(view_source_contents, view_source_extraction_script));

  // Verify the title is derived from the subframe URL.
  GURL original_url = original_child_frame->GetLastCommittedURL();
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}

// Tests that "View Source" works fine for pages shown via HTTP POST.
// This is a regression test for https://crbug.com/523.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, HttpPostInMainframe) {
  // Navigate to a page with a form.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_to_echoall.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_url));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* original_main_frame =
      original_contents->GetPrimaryMainFrame();

  // Submit the form and verify that we arrived at the expected location.
  content::TestNavigationObserver form_post_observer(original_contents, 1);
  EXPECT_TRUE(
      ExecJs(original_main_frame, "document.getElementById('form').submit();"));
  form_post_observer.Wait();
  GURL target_url(embedded_test_server()->GetURL("a.com", "/echoall"));

  content::RenderFrameHost* current_main_frame =
      original_contents->GetPrimaryMainFrame();
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main frame navigations, the form submission above will result
    // in a change of RFH.
    EXPECT_NE(current_main_frame, original_main_frame);
  } else {
    EXPECT_EQ(current_main_frame, original_main_frame);
  }
  EXPECT_EQ(target_url, current_main_frame->GetLastCommittedURL());

  // Extract the response nonce.
  std::string response_nonce_extraction_script =
      "document.getElementById('response-nonce').innerText;";
  std::string response_nonce =
      EvalJs(current_main_frame, response_nonce_extraction_script)
          .ExtractString();

  // Open view-source mode tab for the main frame.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  current_main_frame->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify contents of the view-source tab.  In particular:
  // 1) the sources should contain the POST data
  // 2) the sources should contain the original response-nonce
  //    (i.e. no new network request should be made - the data should be
  //    retrieved from the cache)
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      output; )";
  std::string source_text =
      content::EvalJs(view_source_contents, view_source_extraction_script)
          .ExtractString();
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Body:</h1><pre>text=value</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<pre id='request-headers'>POST /echoall HTTP"));
  EXPECT_THAT(source_text,
              ContainsRegex("Request Headers:.*Referer: " + form_url.spec()));
  EXPECT_THAT(
      source_text,
      ContainsRegex(
          "Request Headers:.*Content-Type: application/x-www-form-urlencoded"));
  EXPECT_THAT(source_text, HasSubstr("<h1>Response nonce:</h1>"
                                     "<pre id='response-nonce'>" +
                                     response_nonce + "</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<title>EmbeddedTestServer - EchoAll</title>"));

  // Verify that the original contents and the view-source contents are in a
  // different process - see https://crbug.com/699493.
  EXPECT_NE(current_main_frame->GetSiteInstance(),
            view_source_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Verify the title of view-source is derived from the URL (not from the title
  // of the original contents).
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_EQ("EmbeddedTestServer - EchoAll",
            base::UTF16ToUTF8(original_contents->GetTitle()));
  EXPECT_THAT(title, Not(HasSubstr("EmbeddedTestServer - EchoAll")));
  GURL original_url = current_main_frame->GetLastCommittedURL();
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}

// Test the case where ViewSource() is called on a top-level RenderFrameHost
// that has never had a commit, so has an empty IsolationInfo. For ViewSource()
// to do anything, the NavigationController for the tab must have a
// LastCommittedEntry(). This sounds like a contradiction of requirements, but
// can happen when a tab is cloned, and possibly other cases as well, like
// session restore.
//
// The main concern here is that the source RenderFrameHost has an empty
// IsolationInfo, and accessing it would DCHECK, so this path should mint a new
// one.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceWithRenderFrameHostWithoutCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a URL, it doesn't matter which, just need the tab to have a
  // committed entry other than about:blank or the NTP.
  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Duplicate the tab. The newly created tab should be active.
  chrome::DuplicateTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Check preconditions.
  EXPECT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetController()
                  .GetLastCommittedEntry());
  EXPECT_EQ(GURL(), browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetPrimaryMainFrame()
                        ->GetLastCommittedURL());

  // Open a view source tab, and watch for its main network request.
  content::URLLoaderMonitor loader_monitor({url});
  content::WebContentsAddedObserver view_source_contents_observer;
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetPrimaryMainFrame()
      ->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       url.spec());
  EXPECT_EQ(view_source_url, view_source_contents->GetLastCommittedURL());
  // Make sure that the navigation type reported is "back_forward" on the
  // duplicated tab.
  EXPECT_EQ(
      "back_forward",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "performance.getEntriesByType('navigation')[0].type"));

  // Verify the request for the view-source tab had the correct IsolationInfo.
  std::optional<network::ResourceRequest> request =
      loader_monitor.GetRequestInfo(url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->trusted_params);
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(request->trusted_params->isolation_info.IsEqualForTesting(
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 origin, origin,
                                 net::SiteForCookies::FromOrigin(origin))));
}

class ViewSourceWithSplitCacheTest
    : public ViewSourceTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    bool split_cache_by_network_isolation_key = GetParam();
    if (split_cache_by_network_isolation_key) {
      enabled_features.push_back(
          net::features::kSplitCacheByNetworkIsolationKey);
    } else {
      disabled_features.push_back(
          net::features::kSplitCacheByNetworkIsolationKey);
    }
    disabled_features.push_back(features::kHttpsUpgrades);
    feature_list()->Reset();
    feature_list()->InitWithFeatures(enabled_features, disabled_features);

    ViewSourceTest::SetUp();
  }
};

// Tests that "View Source" works fine for *subframes* shown via HTTP POST.
// This is a regression test for https://crbug.com/774691.
IN_PROC_BROWSER_TEST_P(ViewSourceWithSplitCacheTest, HttpPostInSubframe) {
  // Navigate to a page with multiple frames.
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate a child frame to a document with a form.
  GURL form_url(embedded_test_server()->GetURL(
      "b.com", "/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(
      content::NavigateIframeToURL(original_contents, "frame1", form_url));
  content::RenderFrameHost* original_child_frame =
      ChildFrameAt(original_contents, 0);
  ASSERT_TRUE(original_child_frame);

  // Submit the form and verify that we arrived at the expected location.
  content::TestNavigationObserver form_post_observer(original_contents, 1);
  EXPECT_TRUE(ExecJs(original_child_frame,
                     "document.getElementById('form').submit();"));
  form_post_observer.Wait();
  original_child_frame = ChildFrameAt(original_contents, 0);

  GURL target_url(embedded_test_server()->GetURL("b.com", "/echoall"));
  EXPECT_EQ(target_url, original_child_frame->GetLastCommittedURL());

  // Extract the response nonce.
  std::string response_nonce_extraction_script =
      "document.getElementById('response-nonce').innerText;";
  std::string response_nonce =
      EvalJs(original_child_frame, response_nonce_extraction_script)
          .ExtractString();

  // Open view-source mode tab for the subframe.  This tries to mimic the
  // behavior of RenderViewContextMenu::ExecuteCommand when it handles
  // IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE.
  content::WebContentsAddedObserver view_source_contents_observer;
  original_child_frame->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));

  // Verify contents of the view-source tab.  In particular:
  // 1) the sources should contain the POST data
  // 2) the sources should contain the original response-nonce
  //    (i.e. no new network request should be made - the data should be
  //    retrieved from the cache)
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      output; )";
  std::string source_text =
      content::EvalJs(view_source_contents, view_source_extraction_script)
          .ExtractString();
  EXPECT_THAT(source_text,
              HasSubstr("<h1>Request Body:</h1><pre>text=value</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<pre id='request-headers'>POST /echoall HTTP"));
  EXPECT_THAT(source_text,
              ContainsRegex("Request Headers:.*Referer: " + form_url.spec()));
  EXPECT_THAT(
      source_text,
      ContainsRegex(
          "Request Headers:.*Content-Type: application/x-www-form-urlencoded"));
  EXPECT_THAT(source_text, HasSubstr("<h1>Response nonce:</h1>"
                                     "<pre id='response-nonce'>" +
                                     response_nonce + "</pre>"));
  EXPECT_THAT(source_text,
              HasSubstr("<title>EmbeddedTestServer - EchoAll</title>"));

  // Verify that view-source opens in a new process - https://crbug.com/699493.
  EXPECT_NE(original_child_frame->GetSiteInstance(),
            view_source_contents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(original_contents->GetSiteInstance(),
            view_source_contents->GetPrimaryMainFrame()->GetSiteInstance());

  // Verify the title is derived from the URL.
  GURL original_url = original_child_frame->GetLastCommittedURL();
  std::string title = base::UTF16ToUTF8(view_source_contents->GetTitle());
  EXPECT_THAT(title, HasSubstr(content::kViewSourceScheme));
  EXPECT_THAT(title, HasSubstr(original_url.host()));
  EXPECT_THAT(title, HasSubstr(original_url.port()));
  EXPECT_THAT(title, HasSubstr(original_url.path()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ViewSourceWithSplitCacheTest,
    testing::Bool());

using ViewSourceWithSplitCacheEnabledTest = ViewSourceWithSplitCacheTest;

// Tests that the network isolation key for the view-source request is reused
// in the back-navigation request to the view-source page.
//
// The test runs the following steps:
// 1. Navigate to page a.com/title1.html
// 2. Create a cross-site subframe b.com/title1.html
// 3. View-source the subframe
// 4. Navigate the view-source page to a c.com/title1.html
// 5. Navigate back to the view-source page
//
// In the end, the test checks whether the back navigation request resource
// exists in the cache. |exists_in_cache == true| implies the top_frame_origin
// of the network isolation key is a.com (reused).
IN_PROC_BROWSER_TEST_P(ViewSourceWithSplitCacheEnabledTest,
                       NetworkIsolationKeyReusedForBackNavigation) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1. Navigate to page a.com/title1.html
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string subframe_url =
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html")).spec();
  {
    // 2. Create a cross-site subframe b.com/title1.html
    std::string create_frame_script = base::StringPrintf(
        "let frame = document.createElement('iframe');"
        "frame.src = '%s';"
        "document.body.appendChild(frame);",
        subframe_url.c_str());
    content::TestNavigationObserver navigation_observer(original_contents);
    original_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(create_frame_script), base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    navigation_observer.Wait();
  }

  // 3. View-source the subframe
  content::WebContentsAddedObserver view_source_contents_observer;
  ChildFrameAt(original_contents, 0)->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));
  // This test expects us to re-load a page after a back navigation (and reuse
  // the network isolation key while doing so), which won't happen when the
  // page is restored from the back forward cache. We are disabling caching for
  // |view_source_contents| to make sure it will not be put into the back
  // forward cache.
  view_source_contents->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 4. Navigate the view-source page to a c.com/title1.html
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(embedded_test_server()->GetURL("c.com", "/title1.html"))));

  base::RunLoop cache_status_waiter;
  content::URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            return false;
          }),
      base::BindLambdaForTesting(
          [&](const GURL& request_url,
              const network::URLLoaderCompletionStatus& status) {
            if (request_url == subframe_url) {
              EXPECT_TRUE(status.exists_in_cache);
              cache_status_waiter.Quit();
            }
          }),
      {});

  {
    // 5. Navigate back to the view-source page
    content::WebContents* new_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver navigation_observer(new_contents);
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
  }

  cache_status_waiter.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ViewSourceWithSplitCacheEnabledTest,
    ::testing::Values(true));

// Verify that links clicked from view-source do not send a Referer header.
// See https://crbug.com/834023.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, NavigationOmitsReferrer) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(content::kViewSourceScheme + std::string(":") +
           embedded_test_server()->GetURL(kTestNavigationHtml).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Click the first link in the view-source markup.
  content::WebContentsAddedObserver nav_observer;
  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementsByTagName('A')[0].click();"));
  content::WebContents* new_contents = nav_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Validate that no referrer was sent.
  EXPECT_EQ("None", EvalJs(new_contents, "document.body.innerText;"));
}

// Verify that JavaScript URIs are sanitized to about:blank.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, JavaScriptURISanitized) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(content::kViewSourceScheme + std::string(":") +
           embedded_test_server()->GetURL(kTestNavigationHtml).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get the href of the second link in the view-source markup.
  std::string link_href_extraction_script =
      "document.getElementsByTagName('A')[1].href;";

  EXPECT_EQ(
      "about:blank",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      link_href_extraction_script));
}

// This test verifies that 'view-source' documents are not affected by vertical
// scroll (see https://crbug.com/898688).
IN_PROC_BROWSER_TEST_F(ViewSourcePermissionsPolicyTest,
                       ViewSourceNotAffectedByHeaderPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string k_verify_feature = R"(
      var all_features = document.featurePolicy.allowedFeatures();
      var vs = all_features.find((f) => f === 'vertical-scroll');
      console.log(vs);
      "" + vs;)";
  // Sanity-check: 'vertical-scroll' is disabled in the actual page (set by the
  // mock headers).
  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("undefined",
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   k_verify_feature));
  // Ensure the policy is enabled in the view-source version.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(content::kViewSourceScheme + std::string(":") + url.spec())));
  EXPECT_EQ("vertical-scroll",
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   k_verify_feature));
}

namespace {

class ViewSourcePrerenderTest : public ViewSourceTest {
 protected:
  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* target() const { return target_; }
  void set_target(content::WebContents* target) { target_ = target; }

  void SetUp() override {
    prerender_test_helper().RegisterServerRequestMonitor(
        embedded_test_server());
    ViewSourceTest::SetUp();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_{
      base::BindRepeating(&ViewSourcePrerenderTest::target,
                          base::Unretained(this))};

  // The WebContents which is expected to request prerendering.
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> target_ = nullptr;
};

// A frame in a prerendered page should be able to have its source viewed, like
// any other. There is currently no UI for this, but in principle it should
// work.
IN_PROC_BROWSER_TEST_F(ViewSourcePrerenderTest, ViewSourceForPrerender) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL referrer_url = embedded_test_server()->GetURL("/title1.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  content::RenderFrameHost* referrer_frame =
      ui_test_utils::NavigateToURL(browser(), referrer_url);
  set_target(content::WebContents::FromRenderFrameHost(referrer_frame));

  prerender_test_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(prerender_frame);

  content::WebContentsAddedObserver view_source_contents_observer;
  prerender_frame->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_contents));
  EXPECT_EQ(view_source_contents->GetLastCommittedURL(),
            GURL(base::StrCat(
                {content::kViewSourceScheme, ":", prerender_url.spec()})));
  EXPECT_THAT(base::UTF16ToUTF8(view_source_contents->GetTitle()),
              HasSubstr(content::kViewSourceScheme));
}

}  // namespace
