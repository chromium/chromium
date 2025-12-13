// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::ExecJs;
using content::RenderFrameHost;
using content::TestNavigationManager;
using content::WaitForCopyableViewInWebContents;
using content::WaitForDOMContentLoaded;

namespace actor {

namespace {

// The site policy check bypasses localhost, so use a fake hostname to
// ensure the check is exercised.
constexpr char kDomainA[] = "a.test";

class ActorHistoryToolBrowserTest : public ActorToolsTest {
 public:
  ActorHistoryToolBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kGlicCrossOriginNavigationGating});
  }
  ~ActorHistoryToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/415385900): Add a test for navigation API canceling a
// same-document navigation.

// Basic test of the HistoryTool going back.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest, HistoryTool_Back) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Basic test of the HistoryTool going forward
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest, HistoryTool_Forward) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  GoBack();
  ASSERT_EQ(web_contents()->GetURL(), url_first);

  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action =
      MakeHistoryForwardRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_second);
}

// Basic test will, under normal circumstances use BFCache. Ensure coverage
// without BFCache as well.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest, HistoryTool_BackNoBFCache) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Test that tool fails validation if there's no further session history in the
// direction of travel.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_FailNoSessionHistory) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?first");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?second");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  // Attempting a forward history navigation should fail since we're at the
  // latest entry.
  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeHistoryForwardRequest(*active_tab());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kHistoryNoForwardEntries);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }

  // Prune all earlier entries so we can't go back.
  web_contents()->GetController().PruneAllButLastCommitted();
  ASSERT_FALSE(web_contents()->GetController().CanGoBack());

  // Attempting a back history navigation should fail since we're at the first
  // entry.
  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kHistoryNoBackEntries);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_BackSameDocument) {
  const GURL url_first = embedded_test_server()->GetURL("/actor/blank.html");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html#foo");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetURL(), url_first);
  }

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> action =
        MakeHistoryForwardRequest(*active_tab());
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_BasicIframeBack) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/actor/simple_iframe.html");
  const GURL child_frame_url_1 =
      embedded_test_server()->GetURL("/actor/blank.html");
  const GURL child_frame_url_2 =
      embedded_test_server()->GetURL("/actor/blank.html?next");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate the child frame to a new document.
  RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  ASSERT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_1);
  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(child_frame, child_frame_url_2));
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_2);

  // Invoke the history back tool. The iframe should be navigated back.
  ActResultFuture result;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_1);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

// Ensure the history tool doesn't return until the navigation completes.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest, HistoryTool_SlowBack) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TestNavigationManager back_navigation(web_contents(), url_first);
  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ASSERT_TRUE(back_navigation.WaitForResponse());
  EXPECT_FALSE(result_success.IsReady());

  for (int i = 0; i < 3; ++i) {
    TinyWait();
    EXPECT_FALSE(result_success.IsReady());
  }

  ASSERT_TRUE(back_navigation.WaitForNavigationFinished());
  ExpectOkResult(result_success);
}

// Test a case where history back causes navigation in two frames.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_ConcurrentNavigations) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/actor/concurrent_navigations.html");
  const GURL child_frame_1_start_url =
      embedded_test_server()->GetURL("/actor/blank.html?A1");
  const GURL child_frame_1_target_url =
      embedded_test_server()->GetURL("/actor/blank.html?A2");
  const GURL child_frame_2_start_url =
      embedded_test_server()->GetURL("/actor/blank.html?B1");
  const GURL child_frame_2_target_url =
      embedded_test_server()->GetURL("/actor/blank.html?B2");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate the child frame to a new document.
  RenderFrameHost* child_frame_1 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  RenderFrameHost* child_frame_2 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(child_frame_1);
  ASSERT_TRUE(child_frame_2);
  ASSERT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_start_url);
  ASSERT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_start_url);

  ASSERT_TRUE(content::NavigateToURLFromRenderer(child_frame_1,
                                                 child_frame_1_target_url));
  child_frame_1 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_target_url);

  // The first frame navigated to A2 so the session history looks like:
  // [about:blank], [Main, A1, B1], [Main, A2, B1]

  // Now navigate the second iframe but with replacement so we get:
  // [about:blank], [Main, A1, B1], [Main, A2, B2]
  TestNavigationManager replace_navigation(web_contents(),
                                           child_frame_2_target_url);
  ASSERT_TRUE(ExecJs(
      child_frame_2,
      content::JsReplace("location.replace($1);", child_frame_2_target_url)));
  ASSERT_TRUE(replace_navigation.WaitForNavigationFinished());
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_target_url);

  // Invoke the history back tool. Both should be navigated back to their
  // starting URL.
  ActResultFuture result;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  child_frame_1 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_start_url);
  EXPECT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_start_url);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

// Ensure the history tool works correctly when a before unload handler is
// present (but doesn't cause a prompt to show).
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_HasBeforeUnload) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  // Add a no-op beforeunload handler. This won't show the prompt but may force
  // the browser to send an event to the renderer to confirm which can change
  // the async path taken by the navigation.
  ASSERT_TRUE(ExecJs(web_contents(),
                     R"JS(
                      addEventListener('beforeunload', () => {});
                      )JS"));

  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);
  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Test that back navigation from a POST request works as expected.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest, HistoryTool_BackFromPOST) {
  // Ensure BFCache isn't used so the back navigation loads a new document.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);
  const GURL url_a =
      embedded_test_server()->GetURL(kDomainA, "/actor/history_post_form.html");
  const GURL url_b = embedded_test_server()->GetURL(
      kDomainA, "/actor/history_post_page_b.html");
  const GURL url_c =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?page_c");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  ASSERT_EQ(web_contents()->GetURL(), url_a);

  // Submit form to go to page B.
  {
    content::TestNavigationObserver nav_observer(web_contents(), 1);
    ASSERT_TRUE(
        ExecJs(web_contents(), "document.getElementById('submit').click();"));
    nav_observer.Wait();
    ASSERT_EQ(web_contents()->GetURL(), url_b);
    ASSERT_TRUE(web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->GetHasPostData());
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_c));
  ASSERT_EQ(web_contents()->GetURL(), url_c);

  {
    // Go back to page B. This should show a POST resubmission page.
    content::TestNavigationObserver back_nav_observer(web_contents(), 1);
    ActResultFuture fut;
    std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
    actor_task().Act(ToRequestList(action), fut.GetCallback());
    back_nav_observer.Wait();
    EXPECT_EQ(back_nav_observer.last_net_error_code(), net::ERR_CACHE_MISS);

    // The history tool should see the navigation to the error page as an
    // error.
    const mojom::ActionResultPtr& result = std::get<0>(fut.Get());
    EXPECT_EQ(result->code, mojom::ActionResultCode::kHistoryErrorPage);
    EXPECT_THAT(result->message, testing::HasSubstr("ERR_CACHE_MISS"));

    // We should be on the error page for B. The last committed entry is the
    // error page, which has the URL of B.
    EXPECT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
              url_b);
    EXPECT_EQ(web_contents()->GetURL(), url_b);
    EXPECT_FALSE(web_contents()->GetController().GetPendingEntry());
  }

  // Go back again to page A.
  ActResultFuture fut;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), fut.GetCallback());

  // The second back navigation should complete successfully.
  ExpectOkResult(fut);

  EXPECT_EQ(web_contents()->GetURL(), url_a);
}

// Test that forward navigation from a POST request works as expected.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_ForwardFromPOST) {
  // Ensure BFCache isn't used so the back navigation loads a new document.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);
  const GURL url_a =
      embedded_test_server()->GetURL(kDomainA, "/actor/history_post_form.html");
  const GURL url_b = embedded_test_server()->GetURL(
      kDomainA, "/actor/history_post_page_b.html");
  const GURL url_c =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?page_c");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  ASSERT_EQ(web_contents()->GetURL(), url_a);

  // Submit form to go to page B.
  {
    content::TestNavigationObserver nav_observer(web_contents(), 1);
    ASSERT_TRUE(
        ExecJs(web_contents(), "document.getElementById('submit').click();"));
    nav_observer.Wait();
    ASSERT_EQ(web_contents()->GetURL(), url_b);
    ASSERT_TRUE(web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->GetHasPostData());
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_c));
  ASSERT_EQ(web_contents()->GetURL(), url_c);

  {
    // Go back to page B. This should show a POST resubmission page.
    content::TestNavigationObserver back_nav_observer(web_contents(), 1);
    ActResultFuture fut;
    std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
    actor_task().Act(ToRequestList(action), fut.GetCallback());
    back_nav_observer.Wait();
    EXPECT_EQ(back_nav_observer.last_net_error_code(), net::ERR_CACHE_MISS);

    // The history tool should see the navigation to the error page as an
    // error.
    const mojom::ActionResultPtr& result = std::get<0>(fut.Get());
    EXPECT_EQ(result->code, mojom::ActionResultCode::kHistoryErrorPage);
    EXPECT_THAT(result->message, testing::HasSubstr("ERR_CACHE_MISS"));

    // We should be on the error page for B. The last committed entry is the
    // error page, which has the URL of B.
    EXPECT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
              url_b);
    EXPECT_EQ(web_contents()->GetURL(), url_b);
    EXPECT_FALSE(web_contents()->GetController().GetPendingEntry());
  }

  // Go forward to page C.
  ActResultFuture fut;
  std::unique_ptr<ToolRequest> action =
      MakeHistoryForwardRequest(*active_tab());
  actor_task().Act(ToRequestList(action), fut.GetCallback());

  // The forward navigation should complete successfully.
  ExpectOkResult(fut);

  EXPECT_EQ(web_contents()->GetURL(), url_c);
}

// Test that direct navigation from a POST request works as expected.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_NavigateFromPOST) {
  // Ensure BFCache isn't used so the back navigation loads a new document.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);
  const GURL url_a =
      embedded_test_server()->GetURL(kDomainA, "/actor/history_post_form.html");
  const GURL url_b = embedded_test_server()->GetURL(
      kDomainA, "/actor/history_post_page_b.html");
  const GURL url_c =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?page_c");
  const GURL url_d =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?page_d");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  ASSERT_EQ(web_contents()->GetURL(), url_a);

  // Submit form to go to page B.
  {
    content::TestNavigationObserver nav_observer(web_contents(), 1);
    ASSERT_TRUE(
        ExecJs(web_contents(), "document.getElementById('submit').click();"));
    nav_observer.Wait();
    ASSERT_EQ(web_contents()->GetURL(), url_b);
    ASSERT_TRUE(web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->GetHasPostData());
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_c));
  ASSERT_EQ(web_contents()->GetURL(), url_c);

  {
    // Go back to page B. This should show a POST resubmission page.
    content::TestNavigationObserver back_nav_observer(web_contents(), 1);
    ActResultFuture fut;
    std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
    actor_task().Act(ToRequestList(action), fut.GetCallback());
    back_nav_observer.Wait();
    EXPECT_EQ(back_nav_observer.last_net_error_code(), net::ERR_CACHE_MISS);

    // The history tool should see the navigation to the error page as an
    // error.
    const mojom::ActionResultPtr& result = std::get<0>(fut.Get());
    EXPECT_EQ(result->code, mojom::ActionResultCode::kHistoryErrorPage);
    EXPECT_THAT(result->message, testing::HasSubstr("ERR_CACHE_MISS"));

    // We should be on the error page for B. The last committed entry is the
    // error page, which has the URL of B.
    EXPECT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
              url_b);
    EXPECT_EQ(web_contents()->GetURL(), url_b);
    EXPECT_FALSE(web_contents()->GetController().GetPendingEntry());
  }

  // Navigate to page D.
  ActResultFuture fut;
  std::unique_ptr<ToolRequest> action =
      MakeNavigateRequest(*active_tab(), url_d.spec());
  actor_task().Act(ToRequestList(action), fut.GetCallback());

  // The navigation should complete successfully.
  ExpectOkResult(fut);

  EXPECT_EQ(web_contents()->GetURL(), url_d);
}

// Ensure that when navigating to a new document, the history tool delays
// completion until the new page has fired the load event.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_DelaysUntilLoad) {
  // Ensure BFCache isn't used so the back navigation loads a new document.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/simple_iframe.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/simple_iframe.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  const GURL url_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
          ->GetLastCommittedURL();

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TestNavigationManager subframe_manager(web_contents(), url_subframe);
  TestNavigationManager main_manager(web_contents(), url_first);

  ActResultFuture result;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // Wait for the main frame navigation to finish and for the main document to
  // reach DOMContentLoaded and for a frame to be presented.
  ASSERT_TRUE(main_manager.WaitForNavigationFinished());
  ASSERT_TRUE(WaitForDOMContentLoaded(main_frame()));
  WaitForCopyableViewInWebContents(web_contents());

  // Prevent the subframe response from being processed.
  ASSERT_TRUE(subframe_manager.WaitForResponse());

  EXPECT_FALSE(result.IsReady());
  TinyWait();
  EXPECT_FALSE(result.IsReady());
  ASSERT_FALSE(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  // Unblocking the subframe response will allow the page to fire the load event
  // and complete the tool request.
  ASSERT_TRUE(subframe_manager.WaitForNavigationFinished());
  ExpectOkResult(result);
}

// Test that the history tool correctly adds the acted on tab to the task's set
// of tabs.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(actor_task().GetTabs().empty());

  ActResultFuture result_success;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), result_success.GetCallback());
  ExpectOkResult(result_success);
  EXPECT_EQ(actor_task().GetTabs().size(), 1ul);
  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

// Test that the history tool fails validation if the destination URL is
// blocked.
IN_PROC_BROWSER_TEST_F(ActorHistoryToolBrowserTest,
                       HistoryTool_BackToBlockedUrlFailsValidation) {
  // Use a non-localhost hostname to ensure the site policy check is exercised.
  const GURL url_a =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?a");
  const GURL url_blocked = embedded_test_server()->GetURL(
      "blocked.example.com", "/actor/blank.html?blocked");
  const GURL url_c =
      embedded_test_server()->GetURL(kDomainA, "/actor/blank.html?c");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_blocked));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_c));

  // Attempting a back navigation to the blocked URL should fail validation.
  ActResultFuture fut;
  std::unique_ptr<ToolRequest> action = MakeHistoryBackRequest(*active_tab());
  actor_task().Act(ToRequestList(action), fut.GetCallback());
  ExpectErrorResult(fut, mojom::ActionResultCode::kUrlBlocked);

  // The browser should remain on the current page.
  EXPECT_EQ(web_contents()->GetURL(), url_c);
}

}  // namespace
}  // namespace actor
