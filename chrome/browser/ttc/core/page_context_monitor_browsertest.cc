// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ttc/core/page_context.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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
    EXPECT_CALL(*conversation(), OnPageContextChanged())
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
// the page context it has is stale and switch observation to the newly active
// tab's WebContents.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, ActiveTabChanged) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title2.html")));

  // Open a second, fully loaded tab and leave it active, so that activating
  // the original tab below is the only thing that can signal a page change.
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/simple.html"),
                            ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  StartSession();

  auto tab_changed = ExpectPageChange();
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(tab_changed.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Fetching page context must now return the newly active tab's content.
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);
  base::test::TestFuture<PageContextResult> future;
  session_controller->GetPageContext(future.GetCallback());
  PageContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->ai_page_content.has_value());
  EXPECT_EQ(result->ai_page_content->proto.main_frame_data().title(),
            "Title Of Awesomeness");

  // Navigating the newly active tab must also trigger a page change
  // notification from WebContentsObserver.
  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // Switching tabs while a fetch is in flight must destroy the previous tab's
  // TtcPageContextMonitor and cancel the in-flight fetch without invoking its
  // callback.
  bool cancelled_fetch_called = false;
  session_controller->GetPageContext(base::BindLambdaForTesting(
      [&](PageContextResult) { cancelled_fetch_called = true; }));

  auto switched_back = ExpectPageChange();
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(switched_back.Wait());
  EXPECT_FALSE(cancelled_fetch_called);
}

// Activating a different browser window of the same profile must switch
// monitoring to the newly active window's active tab, and closing all browser
// windows of the profile must invalidate the page context.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       SwitchingBrowserWindowsAndClosingAllWindows) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // Creating a new browser window activates it, which must switch monitoring to
  // the new window's active tab.
  EXPECT_CALL(*conversation(), OnPageContextChanged())
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

  // Navigating the active tab in the second window must notify the conversation
  // and update the fetched page context.
  EXPECT_CALL(*conversation(), OnPageContextChanged())
      .Times(testing::AtLeast(1));
  ASSERT_TRUE(content::NavigateToURL(
      second_browser->GetTabStripModel()->GetActiveWebContents(),
      embedded_test_server()->GetURL("/title2.html")));
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  {
    base::test::TestFuture<PageContextResult> future;
    session_controller->GetPageContext(future.GetCallback());
    PageContextResult result = future.Take();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->ai_page_content.has_value());
    EXPECT_EQ(result->ai_page_content->proto.main_frame_data().title(),
              "Title Of Awesomeness");
  }

  // Re-activating the original window must switch monitoring back to its active
  // tab.
  auto reactivated = ExpectPageChange();
  browser()->GetWindow()->Activate();
  ui_test_utils::WaitForBrowserSetLastActive(browser());
  EXPECT_TRUE(reactivated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  {
    base::test::TestFuture<PageContextResult> future;
    session_controller->GetPageContext(future.GetCallback());
    PageContextResult result = future.Take();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->ai_page_content.has_value());
    EXPECT_EQ(result->ai_page_content->proto.main_frame_data().title(), "OK");
  }

  // Keep an incognito browser open so closing all regular profile windows does
  // not tear down the browser process.
  Profile* test_profile = profile();
  CreateIncognitoBrowser(test_profile);

  CloseBrowserSynchronously(second_browser);
  MockConversation* conv = conversation();
  EXPECT_CALL(*conv, OnPageContextChanged()).Times(testing::AtLeast(1));
  CloseBrowserSynchronously(browser());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conv));

  {
    base::test::TestFuture<PageContextResult> future;
    session_controller->GetPageContext(future.GetCallback());
    PageContextResult result = future.Take();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        page_content_annotations::FetchPageContextError::kWebContentsWentAway);
  }

  // Restore a browser for the test profile so fixture teardown has a valid
  // profile and browser.
  SetBrowser(CreateBrowser(test_profile));
}

// Ensure the context monitor signals when a load event is fired.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, LoadStopped) {
  StartSession();

  auto navigated = ExpectPageChange();
  NavigateButBlockLoad();
  EXPECT_TRUE(navigated.Wait());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  // No notification while the page is still loading.
  EXPECT_CALL(*conversation(), OnPageContextChanged).Times(0);
  TinyWait();
  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(conversation()));

  auto load_stopped = ExpectPageChange();
  UnblockLoad();
  EXPECT_TRUE(load_stopped.Wait());
}

// Fetching the page context must return the content of the monitored page.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest, GetPageContext) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<PageContextResult> future;
  session_controller->GetPageContext(future.GetCallback());

  PageContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->ai_page_content.has_value());

  const optimization_guide::proto::AnnotatedPageContent& page_content =
      result->ai_page_content->proto;
  EXPECT_EQ(page_content.main_frame_data().title(), "OK");

  ASSERT_TRUE(page_content.has_root_node());
  ASSERT_GT(page_content.root_node().children_nodes_size(), 0);
  const auto& first_child = page_content.root_node().children_nodes(0);
  ASSERT_TRUE(first_child.content_attributes().has_text_data());
  EXPECT_EQ(base::TrimWhitespaceASCII(
                first_child.content_attributes().text_data().text_content(),
                base::TRIM_ALL),
            "Non empty simple page");
}

// Starting a new fetch must cancel any fetch already in flight without
// invoking the cancelled fetch's callback, and starting a new fetch
// synchronously from within a fetch completion callback must not UAF.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       CancelInFlightFetchAndReentrancy) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  bool first_callback_called = false;
  session_controller->GetPageContext(base::BindLambdaForTesting(
      [&](PageContextResult) { first_callback_called = true; }));

  base::test::TestFuture<PageContextResult> reentrant_future;
  session_controller->GetPageContext(
      base::BindLambdaForTesting([&](PageContextResult second_result) {
        EXPECT_TRUE(second_result.has_value());
        // Synchronously start another fetch while `OnFetchComplete` is still
        // on the call stack.
        session_controller->GetPageContext(reentrant_future.GetCallback());
      }));

  PageContextResult third_result = reentrant_future.Take();
  EXPECT_FALSE(first_callback_called);
  ASSERT_TRUE(third_result.has_value());
  ASSERT_TRUE(third_result->ai_page_content.has_value());
  EXPECT_EQ(third_result->ai_page_content->proto.main_frame_data().title(),
            "OK");
}

// Fetching the page context must include the content of a cross-site iframe,
// as iframes are processed on the server side.
IN_PROC_BROWSER_TEST_F(PageContextMonitorBrowserTest,
                       GetPageContextCrossSiteIframe) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/iframe.html")));
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL("b.com", "/simple.html")));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(subframe);
  ASSERT_FALSE(subframe->GetLastCommittedOrigin().IsSameOriginWith(
      main_frame->GetLastCommittedOrigin()));

  StartSession();
  SessionController* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<PageContextResult> future;
  session_controller->GetPageContext(future.GetCallback());

  PageContextResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->ai_page_content.has_value());

  const optimization_guide::proto::AnnotatedPageContent& page_content =
      result->ai_page_content->proto;
  EXPECT_EQ(page_content.main_frame_data().title(), "iframe test");
  ASSERT_TRUE(page_content.has_root_node());

  // The iframe must be present and unredacted on the client, with its content.
  const optimization_guide::proto::ContentNode* iframe =
      FindIframeNode(page_content.root_node());
  ASSERT_TRUE(iframe);
  const optimization_guide::proto::IframeData& iframe_data =
      iframe->content_attributes().iframe_data();
  EXPECT_TRUE(iframe_data.has_frame_data());
  EXPECT_FALSE(iframe_data.has_redacted_frame_metadata());
  EXPECT_GT(iframe->children_nodes_size(), 0);

  // The subframe's text must appear in the content.
  EXPECT_THAT(CollectText(page_content.root_node()),
              testing::Contains(testing::HasSubstr("Non empty simple page")));
}

}  // namespace
}  // namespace ttc
