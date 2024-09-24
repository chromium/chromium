// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace performance_manager {

using testing::_;

namespace {

// Sends a 'p' key press to the page, simulating a user edit if a text field is
// focused.
void SimulateKeyPress(content::WebContents* web_contents) {
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                            ui::DomCode::US_P, ui::VKEY_P, /*control=*/false,
                            /*shift=*/false, /*alt=*/false, /*command=*/false);
}

class FrameNodeImplBrowserTest : public InProcessBrowserTest {
 public:
  FrameNodeImplBrowserTest() = default;
  ~FrameNodeImplBrowserTest() override = default;
};

// Templated PassToGraph helper that also returns a pointer to the object.
template <typename DerivedType>
DerivedType* PassToPMGraph(std::unique_ptr<DerivedType> graph_owned) {
  DerivedType* object = graph_owned.get();
  PerformanceManagerImpl::PassToGraph(FROM_HERE, std::move(graph_owned));
  return object;
}

// A FrameNodeObserver that allows waiting until a frame's viewport intersection
// state is initialized to a set value.
class ViewportIntersectionStateChangedObserver
    : public GraphOwned,
      public FrameNode::ObserverDefaultImpl {
 public:
  // Needed to filter OnIntersectsViewportChanged() notifications for frames
  // that aren't under test. Since the frame node does not exist before the
  // navigation, it is not possible to directly compare the frame node pointer.
  // Note: The URL of the frame does not work because the initialization of the
  // viewport intersection can happen before the document URL is known.
  using FrameNodeMatcher = base::RepeatingCallback<bool(const FrameNode*)>;

  ViewportIntersectionStateChangedObserver(FrameNodeMatcher frame_node_matcher,
                                           bool expected_intersects_viewport,
                                           base::OnceClosure quit_closure)
      : frame_node_matcher_(std::move(frame_node_matcher)),
        expected_intersects_viewport_(expected_intersects_viewport),
        quit_closure_(std::move(quit_closure)) {}
  ~ViewportIntersectionStateChangedObserver() override = default;

  ViewportIntersectionStateChangedObserver(
      const ViewportIntersectionStateChangedObserver&) = delete;
  ViewportIntersectionStateChangedObserver& operator=(
      const ViewportIntersectionStateChangedObserver&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddFrameNodeObserver(this);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveFrameNodeObserver(this);
  }

  // FrameNodeObserver:
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override {
    if (!frame_node_matcher_.Run(frame_node))
      return;

    const bool is_intersecting =
        frame_node->GetViewportIntersection()->is_intersecting();
    EXPECT_EQ(expected_intersects_viewport_, is_intersecting);
    std::move(quit_closure_).Run();
  }

 private:
  const FrameNodeMatcher frame_node_matcher_;
  const bool expected_intersects_viewport_;
  base::OnceClosure quit_closure_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       ViewportIntersection_OutOfView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // First, set up the observer on the PM graph.
  auto frame_node_matcher =
      base::BindRepeating([](const FrameNode* frame_node) {
        DCHECK_EQ(frame_node->GetGraph()->GetAllPageNodes().size(), 1u);

        // Only match the only child node of the main frame.
        const FrameNode* main_frame_node =
            frame_node->GetPageNode()->GetMainFrameNode();
        DCHECK_EQ(main_frame_node->GetChildFrameNodes().size(), 1u);
        return frame_node->GetParentFrameNode() == main_frame_node;
      });
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<ViewportIntersectionStateChangedObserver>(
          std::move(frame_node_matcher), false, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Hidden) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // First, set up the observer on the PM graph.
  auto frame_node_matcher =
      base::BindRepeating([](const FrameNode* frame_node) {
        DCHECK_EQ(frame_node->GetGraph()->GetAllPageNodes().size(), 1u);

        // Only match the only child node of the main frame.
        const FrameNode* main_frame_node =
            frame_node->GetPageNode()->GetMainFrameNode();
        DCHECK_EQ(main_frame_node->GetChildFrameNodes().size(), 1u);
        return frame_node->GetParentFrameNode() == main_frame_node;
      });
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<ViewportIntersectionStateChangedObserver>(
          std::move(frame_node_matcher), false, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_hidden.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       ViewportIntersection_PartiallyVisible) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // First, set up the observer on the PM graph.
  auto frame_node_matcher =
      base::BindRepeating([](const FrameNode* frame_node) {
        DCHECK_EQ(frame_node->GetGraph()->GetAllPageNodes().size(), 1u);

        // Only match the only child node of the main frame.
        const FrameNode* main_frame_node =
            frame_node->GetPageNode()->GetMainFrameNode();
        DCHECK_EQ(main_frame_node->GetChildFrameNodes().size(), 1u);
        return frame_node->GetParentFrameNode() == main_frame_node;
      });
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<ViewportIntersectionStateChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_partially_visible.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Scaled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // First, set up the observer on the PM graph.
  auto frame_node_matcher =
      base::BindRepeating([](const FrameNode* frame_node) {
        DCHECK_EQ(frame_node->GetGraph()->GetAllPageNodes().size(), 1u);

        // Only match the only child node of the main frame.
        const FrameNode* main_frame_node =
            frame_node->GetPageNode()->GetMainFrameNode();
        DCHECK_EQ(main_frame_node->GetChildFrameNodes().size(), 1u);
        return frame_node->GetParentFrameNode() == main_frame_node;
      });
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<ViewportIntersectionStateChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_scaled.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Rotated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // First, set up the observer on the PM graph.
  auto frame_node_matcher =
      base::BindRepeating([](const FrameNode* frame_node) {
        DCHECK_EQ(frame_node->GetGraph()->GetAllPageNodes().size(), 1u);

        // Only match the only child node of the main frame.
        const FrameNode* main_frame_node =
            frame_node->GetPageNode()->GetMainFrameNode();
        DCHECK_EQ(main_frame_node->GetChildFrameNodes().size(), 1u);
        return frame_node->GetParentFrameNode() == main_frame_node;
      });
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<ViewportIntersectionStateChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_rotated.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});
  run_loop.Run();
}

// For the following tests, listen to OnHadFormInteractionChanged() to ensure
// that the DocumentCoordinationUnit interface is correctly bound.
class MockFrameNodeObserver : public FrameNode::ObserverDefaultImpl {
 public:
  MockFrameNodeObserver() = default;
  ~MockFrameNodeObserver() override = default;

  // FrameNodeObserver:
  MOCK_METHOD(void,
              OnHadFormInteractionChanged,
              (const FrameNode* frame_node),
              (override));
};

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, Bind_SimpleNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL kTestUrl =
      embedded_test_server()->GetURL("/form_interaction.html");

  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), kTestUrl);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Get the frame's node.

  // Check that a form interaction notification is received through the bound
  // receiver.
  MockFrameNodeObserver obs;
  RunInGraph([&](GraphImpl* graph) { graph->AddFrameNodeObserver(&obs); });

  base::RunLoop run_loop;
  EXPECT_CALL(obs, OnHadFormInteractionChanged(_)).WillOnce([&]() {
    run_loop.Quit();
  });

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents());
  run_loop.Run();

  // Clean up.
  RunInGraph([&](GraphImpl* graph) { graph->RemoveFrameNodeObserver(&obs); });
}

class FrameNodeImplBackForwardCacheBrowserTest
    : public FrameNodeImplBrowserTest {
 public:
  FrameNodeImplBackForwardCacheBrowserTest() {
    content::InitBackForwardCacheFeature(&scoped_feature_list_,
                                         /*enable_back_forward_cache=*/true);
  }

 public:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FrameNodeImplBackForwardCacheBrowserTest,
                       Bind_BackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL kTestUrl =
      embedded_test_server()->GetURL("/form_interaction.html");
  const GURL kOtherUrl = embedded_test_server()->GetURL("/title1.html");

  // Navigation to the test URL.
  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), kTestUrl);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Navigate to some other URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kOtherUrl));
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Back to the test URL.
  EXPECT_TRUE(content::HistoryGoBack(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Check that a form interaction notification is received through the bound
  // receiver.
  MockFrameNodeObserver obs;
  RunInGraph([&](GraphImpl* graph) { graph->AddFrameNodeObserver(&obs); });

  base::RunLoop run_loop;
  EXPECT_CALL(obs, OnHadFormInteractionChanged(_)).WillOnce([&]() {
    run_loop.Quit();
  });

  // After HistoryGoBack(), the text field is no longer focused so we explicly
  // re-focus it.
  EXPECT_TRUE(ExecJs(rfh, "FocusTextField();"));
  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents());
  run_loop.Run();

  // Clean up.
  RunInGraph([&](GraphImpl* graph) { graph->RemoveFrameNodeObserver(&obs); });
}

class FrameNodeImplPrerenderBrowserTest : public FrameNodeImplBrowserTest {
 public:
  FrameNodeImplPrerenderBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &FrameNodeImplPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 public:
  content::test::ScopedPrerenderFeatureList scoped_prerender_feature_list_;
  content::test::PrerenderTestHelper prerender_test_helper_;
};

// TODO(362360274): Fix this flaky test.
IN_PROC_BROWSER_TEST_F(FrameNodeImplPrerenderBrowserTest,
                       DISABLED_Bind_PrerenderNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderUrl =
      embedded_test_server()->GetURL("/form_interaction.html");

  // Initial navigation. Needed so we can add prerendered frames.
  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), kInitialUrl);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Create the prerendered frame.
  content::FrameTreeNodeId host_id =
      prerender_test_helper_.AddPrerender(kPrerenderUrl);
  content::RenderFrameHost* prerender_rfh =
      prerender_test_helper_.GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(prerender_rfh);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Navigate to the prerendered frame.
  prerender_test_helper_.NavigatePrimaryPage(kPrerenderUrl);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  // Check that a form interaction notification is received through the bound
  // receiver.
  MockFrameNodeObserver obs;
  RunInGraph([&](GraphImpl* graph) { graph->AddFrameNodeObserver(&obs); });

  base::RunLoop run_loop;
  EXPECT_CALL(obs, OnHadFormInteractionChanged(_)).WillOnce([&]() {
    run_loop.Quit();
  });

  // After activating the prerender, the text field is no longer focused so we
  // explicly re-focus it.
  EXPECT_TRUE(ExecJs(prerender_rfh, "FocusTextField();"));
  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents());
  run_loop.Run();

  // Clean up.
  RunInGraph([&](GraphImpl* graph) { graph->RemoveFrameNodeObserver(&obs); });
}

}  // namespace performance_manager
