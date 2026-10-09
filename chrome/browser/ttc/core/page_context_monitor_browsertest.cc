// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/safety_list_manager.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ttc {
namespace {

// Returns the first iframe node found in the tree rooted at `node`, or null if
// there isn't one.
const optimization_guide::proto::ContentNode* FindIframeNode(
    const optimization_guide::proto::ContentNode& node) {
  if (node.content_attributes().attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    return &node;
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    if (const optimization_guide::proto::ContentNode* found =
            FindIframeNode(child)) {
      return found;
    }
  }

  return nullptr;
}

// Returns the text of all text nodes in the tree rooted at `node`.
std::vector<std::string> CollectText(
    const optimization_guide::proto::ContentNode& node) {
  std::vector<std::string> text;
  if (node.content_attributes().has_text_data()) {
    text.push_back(node.content_attributes().text_data().text_content());
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    std::vector<std::string> child_text = CollectText(child);
    text.insert(text.end(), std::make_move_iterator(child_text.begin()),
                std::make_move_iterator(child_text.end()));
  }

  return text;
}

class PageContextMonitorBrowserTest : public TtcCoreBrowserTestBase {
 public:
  PageContextMonitorBrowserTest() = default;
  ~PageContextMonitorBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    // Serves /cross-site/<host>/<path>. Must be set up before the base class
    // starts the server.
    content::SetupCrossSiteRedirector(embedded_test_server());
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    // Allows tests to load content from different sites.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    if (conversation()) {
      // Release expectations after the test to avoid any uninteresting calls
      // on the mock conversation.
      testing::Mock::VerifyAndClearExpectations(conversation());
    }
    // SafetyListManager is a process-wide singleton; clear any lists a test
    // set.
    actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    "{}");
    TtcCoreBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  // Starts a session and waits for the initial notification, leaving no
  // expectations set on the conversation.
  void StartSession() {
    ttc_service().StartSession();
    ASSERT_TRUE(conversation());
    EXPECT_TRUE(ExpectPageChange().Wait());
  }

  // Expects the conversation to be notified of a page change exactly once.
  // Callers wait for the notification using Wait() on the returned future.
  base::test::TestFuture<void> ExpectPageChange() {
    base::test::TestFuture<void> future;
    EXPECT_CALL(*conversation(), OnPageContextInvalidated())
        .WillOnce(base::test::InvokeFuture(future));
    return future;
  }

  // Navigates to an arbitrary page but blocks it from firing the load event.
  void NavigateButBlockLoad() {
    // The test page embeds `subframe_url()` in an iframe, so holding back the
    // subframe's response keeps the page from firing its load event.
    GURL main_url = embedded_test_server()->GetURL("/iframe.html");
    GURL subframe_url = embedded_test_server()->GetURL("/title1.html");
    main_manager_ = std::make_unique<content::TestNavigationManager>(
        web_contents(), main_url);
    subframe_manager_ = std::make_unique<content::TestNavigationManager>(
        web_contents(), subframe_url);

    // Don't use NavigateToURL since it waits for the load to stop, which the
    // held back subframe prevents.
    web_contents()->GetController().LoadURL(main_url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            std::string());
    ASSERT_TRUE(main_manager_->WaitForNavigationFinished());
    ASSERT_TRUE(subframe_manager_->WaitForResponse());
  }

  // Unblocks the load in the navigation started from NavigateButBlockLoad.
  void UnblockLoad() {
    CHECK(subframe_manager_);
    ASSERT_TRUE(subframe_manager_->WaitForNavigationFinished());
  }

  bool IsUrlSupported(const GURL& url) {
    base::test::TestFuture<bool> supported;
    IsUrlSupportedForPageContext(url, supported.GetCallback());
    return supported.Get();
  }

 private:
  std::unique_ptr<content::TestNavigationManager> main_manager_;
  std::unique_ptr<content::TestNavigationManager> subframe_manager_;
};

// Starting a session must asynchronously notify the conversation.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, InitialPageChange) {
  ttc_service().StartSession();
  ASSERT_TRUE(conversation());
  EXPECT_TRUE(ExpectPageChange().Wait());
}

// Navigating the monitored page must notify the conversation that the page
// context it has is stale before the load event is fired.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, PrimaryPageNavigated) {
  StartSession();

  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());

  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
}

// Switching the window to a different tab must notify the conversation that
// the page context it has is stale, switch observation to the newly active
// tab's WebContents, and fetch fresh page context for it.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, ActiveTabChanged) {
  const GURL first_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));

  // Open a second, fully loaded tab and leave it active, so that activating
  // the original tab below is the only thing that can signal a page change.
  const GURL second_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(AddTabAtIndex(1, second_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  StartSession();
  {
    base::test::TestFuture<void> initial_sent;
    EXPECT_CALL(*conversation(),
                SendContextUpdate(second_url, "OK", testing::_))
        .WillOnce(base::test::RunOnceClosure(initial_sent.GetCallback()));
    EXPECT_TRUE(initial_sent.Wait());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));
  }

  auto tab_changed = ExpectPageChange();
  base::test::TestFuture<void> first_tab_sent;
  EXPECT_CALL(*conversation(),
              SendContextUpdate(first_url, "Title Of Awesomeness", testing::_))
      .WillOnce(base::test::RunOnceClosure(first_tab_sent.GetCallback()));
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(tab_changed.Wait());
  EXPECT_TRUE(first_tab_sent.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Navigating the newly active tab must also trigger a page change
  // notification.
  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  auto switched_back = ExpectPageChange();
  base::test::TestFuture<void> second_tab_sent;
  EXPECT_CALL(*conversation(), SendContextUpdate(second_url, "OK", testing::_))
      .WillOnce(base::test::RunOnceClosure(second_tab_sent.GetCallback()));
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(switched_back.Wait());
  EXPECT_TRUE(second_tab_sent.Wait());
}

// Activating a different browser window of the same profile must switch
// monitoring to the newly active window's active tab, and closing all browser
// windows of the profile must invalidate the page context.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       SwitchingBrowserWindowsAndClosingAllWindows) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  StartSession();

  // Creating a new browser window activates it, which must switch monitoring to
  // the new window's active tab.
  EXPECT_CALL(*conversation(), OnPageContextInvalidated())
      .Times(testing::AtLeast(1));
  BrowserWindowInterface* second_browser = CreateBrowser(profile());
  // CreateBrowser() does not wait for the new window to be activated. This
  // returns immediately if the window is already last-active, and otherwise
  // waits for it to become so. Do not pass
  // wait_for_set_last_active_observed=true here: that disables the
  // already-active fast path and blocks on a *subsequent* activation that
  // never arrives.
  ui_test_utils::WaitForBrowserSetLastActive(second_browser);
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Navigating the active tab in the second window must notify the
  // conversation.
  EXPECT_CALL(*conversation(), OnPageContextInvalidated())
      .Times(testing::AtLeast(1));
  ASSERT_TRUE(content::NavigateToURL(
      second_browser->GetTabStripModel()->GetActiveWebContents(),
      embedded_test_server()->GetURL("/title2.html")));
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Re-activating the original window must switch monitoring back to its active
  // tab.
  auto reactivated = ExpectPageChange();
  browser()->GetWindow()->Activate();
  ui_test_utils::WaitForBrowserSetLastActive(browser());
  EXPECT_TRUE(reactivated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Keep an incognito browser open so closing all regular profile windows does
  // not tear down the browser process.
  Profile* test_profile = profile();
  CreateIncognitoBrowser(test_profile);

  CloseBrowserSynchronously(second_browser);
  MockConversation* conv = conversation();
  EXPECT_CALL(*conv, OnPageContextInvalidated()).Times(testing::AtLeast(1));
  CloseBrowserSynchronously(browser());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conv));

  // Restore a browser for the test profile so fixture teardown has a valid
  // profile and browser.
  SetBrowser(CreateBrowser(test_profile));
}

// A session on a page whose URL is not supported (e.g. non-NTP chrome:// pages,
// data: and about: URLs) must never send page context to the conversation,
// whether PCES extracts the page's content or not, and must resume sending
// context once the page navigates to a supported URL.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       UnsupportedUrlsAreNotFetchedOrEmitted) {
  page_content_annotations::PageContentExtractionService* extraction_service =
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile());
  ASSERT_TRUE(extraction_service);

  // Starts a session, which fetches context for the current page, and waits
  // for the initial notification. The conversation is left expecting no
  // context updates.
  auto start_session_expecting_no_context = [&]() {
    ttc_service().StartSession();
    ASSERT_TRUE(conversation());
    EXPECT_CALL(*conversation(), SendContextUpdate).Times(0);
    EXPECT_TRUE(ExpectPageChange().Wait());
  };

  for (const GURL& url :
       {GURL("chrome://version/"), GURL("data:text/html,<title>Data</title>"),
        GURL("about:blank")}) {
    SCOPED_TRACE(url);
    ASSERT_FALSE(IsUrlSupported(url));
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    // Starting a session must not send anything for the page.
    ASSERT_NO_FATAL_FAILURE(start_session_expecting_no_context());

    // Having an active session makes PCES extract every page in the profile.
    // Wait for that extraction (triggering it if it has not started), to make
    // sure its result is not sent either. PCES never extracts about:blank, so
    // there is nothing to wait for there.
    if (!url.IsAboutBlank()) {
      base::test::TestFuture<
          std::optional<page_content_annotations::ExtractedPageContentResult>>
          extracted;
      extraction_service->GetExtractedPageContentAndEligibilityForPageAsync(
          web_contents()->GetPrimaryPage(), extracted.GetCallback(),
          /*trigger_if_not_cached=*/true);
      ASSERT_TRUE(extracted.Wait());
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

    // Neither must a new session, which finds the extracted content cached.
    ttc_service().EndSession();
    ASSERT_NO_FATAL_FAILURE(start_session_expecting_no_context());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

    ttc_service().EndSession();
  }

  // Navigating from an unsupported page to a supported one during a session
  // must invalidate the context and send context for the new page.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL("chrome://version/")));
  ASSERT_NO_FATAL_FAILURE(start_session_expecting_no_context());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  const GURL supported_url = embedded_test_server()->GetURL("/simple.html");
  auto navigated = ExpectPageChange();
  base::test::TestFuture<void> sent;
  EXPECT_CALL(*conversation(),
              SendContextUpdate(supported_url, "OK", testing::_))
      .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), supported_url));
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(sent.Wait());
}

// Page context fetched for the voice-focused tab must be sent to the
// conversation.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       SendsContextToConversation) {
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 1) Starting a session sends the initial page context to the conversation.
  StartSession();
  {
    base::test::TestFuture<void> sent;
    EXPECT_CALL(*conversation(), SendContextUpdate(url, "OK", testing::_))
        .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));
    EXPECT_TRUE(sent.Wait());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));
  }

  // 2) Navigating the monitored page invalidates context and sends fresh
  // context once extracted.
  const GURL second_url = embedded_test_server()->GetURL("/title2.html");
  {
    auto navigated = ExpectPageChange();
    base::test::TestFuture<void> sent;
    EXPECT_CALL(
        *conversation(),
        SendContextUpdate(second_url, "Title Of Awesomeness", testing::_))
        .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));

    ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));
    EXPECT_TRUE(navigated.Wait());
    EXPECT_TRUE(sent.Wait());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));
  }

  // 3) Ending the session and starting a new one delivers cached context
  // asynchronously.
  ttc_service().EndSession();

  StartSession();
  {
    base::test::TestFuture<void> sent;
    EXPECT_CALL(
        *conversation(),
        SendContextUpdate(second_url, "Title Of Awesomeness", testing::_))
        .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));
    EXPECT_TRUE(sent.Wait());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));
  }
}

// A session on a page for which the actor safety lists block a navigation from
// the page's URL to itself must never send page context to the conversation,
// whether the page's content is extracted during the session or already cached,
// and must resume sending context once the page navigates to an allowed URL.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       SafetyListBlockedUrlsAreNotFetchedOrEmitted) {
  actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                  R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "blocked.com" }
      ]
    }
  )json");

  page_content_annotations::PageContentExtractionService* extraction_service =
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile());
  ASSERT_TRUE(extraction_service);

  auto start_session_expecting_no_context = [&]() {
    ttc_service().StartSession();
    ASSERT_TRUE(conversation());
    EXPECT_CALL(*conversation(), SendContextUpdate).Times(0);
    EXPECT_TRUE(ExpectPageChange().Wait());
  };

  // Pages the safety lists do not block send context to the conversation as
  // usual.
  const GURL allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), allowed_url));
  StartSession();
  {
    base::test::TestFuture<void> sent;
    EXPECT_CALL(*conversation(),
                SendContextUpdate(allowed_url, "OK", testing::_))
        .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));
    EXPECT_TRUE(sent.Wait());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));
  }
  ttc_service().EndSession();

  const GURL blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/simple.html");
  ASSERT_FALSE(IsUrlSupported(blocked_url));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), blocked_url));

  // Starting a session on a blocked page must not send context even once PCES
  // extracts the page's content.
  ASSERT_NO_FATAL_FAILURE(start_session_expecting_no_context());
  base::test::TestFuture<
      std::optional<page_content_annotations::ExtractedPageContentResult>>
      extracted;
  extraction_service->GetExtractedPageContentAndEligibilityForPageAsync(
      web_contents()->GetPrimaryPage(), extracted.GetCallback(),
      /*trigger_if_not_cached=*/true);
  ASSERT_TRUE(extracted.Wait());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Neither must a new session, which finds the extracted content cached.
  ttc_service().EndSession();
  ASSERT_NO_FATAL_FAILURE(start_session_expecting_no_context());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Navigating from a blocked page to an allowed one during a session must
  // invalidate the context and send context for the new page.
  auto navigated = ExpectPageChange();
  base::test::TestFuture<void> sent;
  EXPECT_CALL(*conversation(), SendContextUpdate(allowed_url, "OK", testing::_))
      .WillOnce(base::test::RunOnceClosure(sent.GetCallback()));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), allowed_url));
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(sent.Wait());
}

// Extracted page context includes the content of a cross-site iframe.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       ExtractedPageContentIncludesIframe) {
  // The iframe must be cross-site when the page loads: PCES may extract (and
  // cache) the page's content once it loads, and does not re-extract it when a
  // subframe navigates.
  const GURL url =
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(subframe);
  ASSERT_FALSE(subframe->GetLastCommittedOrigin().IsSameOriginWith(
      main_frame->GetLastCommittedOrigin()));

  StartSession();

  optimization_guide::proto::AnnotatedPageContent page_content;
  base::test::TestFuture<void> sent;
  EXPECT_CALL(*conversation(),
              SendContextUpdate(url, "cross-site iframe test", testing::_))
      .WillOnce(
          [&](const GURL&, const std::string&,
              const optimization_guide::proto::AnnotatedPageContent& apc) {
            page_content = apc;
            sent.SetValue();
          });

  EXPECT_TRUE(sent.Wait());

  EXPECT_EQ(page_content.main_frame_data().title(), "cross-site iframe test");
  ASSERT_TRUE(page_content.has_root_node());

  const optimization_guide::proto::ContentNode* iframe =
      FindIframeNode(page_content.root_node());
  ASSERT_TRUE(iframe);
  const optimization_guide::proto::IframeData& iframe_data =
      iframe->content_attributes().iframe_data();
  EXPECT_TRUE(iframe_data.has_frame_data());
  EXPECT_FALSE(iframe_data.has_redacted_frame_metadata());
  EXPECT_GT(iframe->children_nodes_size(), 0);

  // The main frame has no text of its own; this is the iframes' (title1.html).
  EXPECT_THAT(CollectText(page_content.root_node()),
              testing::Contains(testing::HasSubstr("This page has no title")));
}

}  // namespace
}  // namespace ttc
