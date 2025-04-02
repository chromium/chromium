// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tab_collections/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using content::ChildFrameAt;
using content::ExecJs;
using content::JsReplace;
using content::RenderFrameHost;
using content::TestNavigationManager;
using content::TestNavigationObserver;
using content::WebContents;
using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;
using optimization_guide::proto::NavigateAction;
using tabs::TabInterface;

namespace actor {

namespace {

constexpr int64_t kNonExistantContentNodeId = 12345;

class ActorToolsTest : public InProcessBrowserTest {
 public:
  ActorToolsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorToolsTest(const ActorToolsTest&) = delete;
  ActorToolsTest& operator=(const ActorToolsTest&) = delete;

  ~ActorToolsTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(kTestDataPath);
    ASSERT_TRUE(embedded_test_server()->Start());

    actor_coordinator_ = std::make_unique<actor::ActorCoordinator>();
  }

  void GoBack() {
    TestNavigationObserver observer(web_contents());
    web_contents()->GetController().GoBack();
    observer.Wait();
  }

  void TinyWait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  TabInterface* active_tab() { return browser()->GetActiveTabInterface(); }

  ActorCoordinator& actor_coordinator() { return *actor_coordinator_; }

 private:
  std::unique_ptr<ActorCoordinator> actor_coordinator_;

  ScopedFeatureList scoped_feature_list_;
};

// Exercises the basic API to ensure nothing CHECKs or crashes.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, BasicSmokeTest) {
  const GURL url = embedded_test_server()->GetURL("/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  BrowserAction action =
      MakeClick(/*content_node_id=*/kNonExistantContentNodeId);

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_fail;
  actor_coordinator().Act(tab, action, result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  EXPECT_FALSE(result_fail.Get());
}

// Basic test of the MouseMoveTool.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, MouseMoveTool) {
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  BrowserAction action =
      MakeMouseMove(/*content_node_id=*/kNonExistantContentNodeId);

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_fail;
  actor_coordinator().Act(tab, action, result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  // TODO(crbug.com/402218570): Add function to extract real DOMNodeId from the
  // test page so we can expect a true click returning here.
  EXPECT_FALSE(result_fail.Get());
}

// Basic test of the NavigateTool.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, NavigateTool) {
  const GURL url_start = embedded_test_server()->GetURL("/blank.html?start");
  const GURL url_target = embedded_test_server()->GetURL("/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  BrowserAction action;
  NavigateAction* navigate =
      action.add_action_information()->mutable_navigate();
  navigate->mutable_url()->assign(url_target.spec());

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, action, result_success.GetCallback());
  EXPECT_TRUE(result_success.Get());

  EXPECT_EQ(web_contents()->GetURL(), url_target);
}

// Basic test of the HistoryTool going back.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_Back) {
  const GURL url_first = embedded_test_server()->GetURL("/blank.html?start");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, MakeHistoryBack(), result_success.GetCallback());
  EXPECT_TRUE(result_success.Get());

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Basic test of the HistoryTool going forward
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_Forward) {
  const GURL url_first = embedded_test_server()->GetURL("/blank.html?start");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  GoBack();
  ASSERT_EQ(web_contents()->GetURL(), url_first);

  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, MakeHistoryForward(),
                          result_success.GetCallback());
  EXPECT_TRUE(result_success.Get());

  EXPECT_EQ(web_contents()->GetURL(), url_second);
}

// Basic test will, under normal circumstances use BFCache. Ensure coverage
// without BFCache as well.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BackNoBFCache) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first = embedded_test_server()->GetURL("/blank.html?start");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, MakeHistoryBack(), result_success.GetCallback());
  EXPECT_TRUE(result_success.Get());

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Test that tool fails validation if there's no further session history in the
// direction of travel.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_FailNoSessionHistory) {
  const GURL url_first = embedded_test_server()->GetURL("/blank.html?first");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html?second");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  // Attempting a forward history navigation should fail since we're at the
  // latest entry.
  {
    TestFuture<bool> result;
    actor_coordinator().Act(tab, MakeHistoryForward(), result.GetCallback());
    EXPECT_FALSE(result.Get());
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }

  // Prune all earlier entries so we can't go back.
  web_contents()->GetController().PruneAllButLastCommitted();
  ASSERT_FALSE(web_contents()->GetController().CanGoBack());

  // Attempting a back history navigation should fail since we're at the first
  // entry.
  {
    TestFuture<bool> result;
    actor_coordinator().Act(tab, MakeHistoryBack(), result.GetCallback());
    EXPECT_FALSE(result.Get());
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BackSameDocument) {
  const GURL url_first = embedded_test_server()->GetURL("/blank.html");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html#foo");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  {
    TestFuture<bool> result;
    actor_coordinator().Act(tab, MakeHistoryBack(), result.GetCallback());
    EXPECT_TRUE(result.Get());
    EXPECT_EQ(web_contents()->GetURL(), url_first);
  }

  {
    TestFuture<bool> result;
    actor_coordinator().Act(tab, MakeHistoryForward(), result.GetCallback());
    EXPECT_TRUE(result.Get());
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BasicIframeBack) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/simple_iframe.html");
  const GURL child_frame_url_1 = embedded_test_server()->GetURL("/blank.html");
  const GURL child_frame_url_2 =
      embedded_test_server()->GetURL("/blank.html?next");
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
  TabInterface& tab = *active_tab();
  TestFuture<bool> result;
  actor_coordinator().Act(tab, MakeHistoryBack(), result.GetCallback());
  EXPECT_TRUE(result.Get());
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_1);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

// Ensure the history tool doesn't return until the navigation completes.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_SlowBack) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first = embedded_test_server()->GetURL("/blank.html?start");
  const GURL url_second = embedded_test_server()->GetURL("/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TabInterface& tab = *active_tab();

  TestNavigationManager back_navigation(web_contents(), url_first);
  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, MakeHistoryBack(), result_success.GetCallback());
  ASSERT_TRUE(back_navigation.WaitForResponse());
  EXPECT_FALSE(result_success.IsReady());

  for (int i = 0; i < 3; ++i) {
    TinyWait();
    EXPECT_FALSE(result_success.IsReady());
  }

  ASSERT_TRUE(back_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(result_success.Get());
}

// Test a case where history back causes navigation in two frames.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_ConcurrentNavigations) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/concurrent_navigations.html");
  const GURL child_frame_1_start_url =
      embedded_test_server()->GetURL("/blank.html?A1");
  const GURL child_frame_1_target_url =
      embedded_test_server()->GetURL("/blank.html?A2");
  const GURL child_frame_2_start_url =
      embedded_test_server()->GetURL("/blank.html?B1");
  const GURL child_frame_2_target_url =
      embedded_test_server()->GetURL("/blank.html?B2");
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
  ASSERT_TRUE(ExecJs(child_frame_2, JsReplace("location.replace($1);",
                                              child_frame_2_target_url)));
  ASSERT_TRUE(replace_navigation.WaitForNavigationFinished());
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_target_url);

  // Invoke the history back tool. Both should be navigated back to their
  // starting URL.
  TabInterface& tab = *active_tab();
  TestFuture<bool> result;
  actor_coordinator().Act(tab, MakeHistoryBack(), result.GetCallback());
  EXPECT_TRUE(result.Get());

  child_frame_1 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_start_url);
  EXPECT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_start_url);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

}  // namespace

}  // namespace actor
