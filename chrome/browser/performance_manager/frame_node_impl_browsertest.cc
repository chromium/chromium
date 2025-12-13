// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include <vector>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/viewport_intersection.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace performance_manager {

using testing::AllOf;
using testing::Not;
using testing::UnorderedElementsAre;

MATCHER(IsMainFrame, "") {
  return arg->IsMainFrame();
}

MATCHER_P(HasViewportIntersection, viewport_intersection, "") {
  return arg->GetViewportIntersection() == viewport_intersection;
}

namespace {

// Returns true if the mojom::DocumentCoordinationUnit connection associated
// with `render_frame_host` is bound.
bool IsDocumentCoordinatorUnitBound(
    content::RenderFrameHost* render_frame_host) {
  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(render_frame_host);
  if (!frame_node) {
    return false;
  }

  FrameNodeImpl* frame_node_impl = FrameNodeImpl::FromNode(frame_node.get());
  return frame_node_impl->IsDocumentCoordinationUnitBoundForTesting();
}

class FrameNodeImplBrowserTest : public InProcessBrowserTest {
 public:
  ~FrameNodeImplBrowserTest() override = default;

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
};

class ParameterizedFrameNodeImplBrowserTest
    : public FrameNodeImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedFrameNodeImplBrowserTest() {
    base::FieldTrialParams params = {
        {features::kRenderedOutOfViewIsNotVisible.name,
         GetParam() ? "true" : "false"}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPMProcessPriorityPolicy, params);
  }
  ~ParameterizedFrameNodeImplBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ParameterizedFrameNodeImplBrowserTest,
                       ViewportIntersection_OutOfView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const bool expects_intersects_viewport =
      !performance_manager::features::kRenderedOutOfViewIsNotVisible.Get();
  testing::Matcher<const FrameNode*> viewport_intersection_matcher =
      expects_intersects_viewport
          ? HasViewportIntersection(ViewportIntersection::kIntersecting)
          : HasViewportIntersection(ViewportIntersection::kNotIntersecting);

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  Graph* graph = PerformanceManager::GetGraph();
  auto all_frame_nodes = graph->GetAllFrameNodes().AsVector();

  EXPECT_THAT(
      all_frame_nodes,
      UnorderedElementsAre(
          // One main frame, intersects with the viewport.
          AllOf(IsMainFrame(),
                HasViewportIntersection(ViewportIntersection::kIntersecting)),
          // One child frame, intersects with the viewport depending on the
          // value of the kRenderedOutOfViewIsNotVisible feature.
          AllOf(Not(IsMainFrame()), viewport_intersection_matcher)));
}

INSTANTIATE_TEST_SUITE_P(,
                         ParameterizedFrameNodeImplBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Hidden) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_hidden.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  Graph* graph = PerformanceManager::GetGraph();
  auto all_frame_nodes = graph->GetAllFrameNodes().AsVector();

  EXPECT_THAT(
      all_frame_nodes,
      UnorderedElementsAre(
          // One main frame, intersects with the viewport.
          AllOf(IsMainFrame(),
                HasViewportIntersection(ViewportIntersection::kIntersecting)),
          // One child frame, does not intersect with the viewport.
          AllOf(Not(IsMainFrame()),
                HasViewportIntersection(
                    ViewportIntersection::kNotIntersecting))));
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       ViewportIntersection_PartiallyVisible) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_partially_visible.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  Graph* graph = PerformanceManager::GetGraph();
  auto all_frame_nodes = graph->GetAllFrameNodes().AsVector();

  EXPECT_THAT(
      all_frame_nodes,
      UnorderedElementsAre(
          // One main frame, intersects with the viewport.
          AllOf(IsMainFrame(),
                HasViewportIntersection(ViewportIntersection::kIntersecting)),
          // One child frame, also intersects with the viewport.
          AllOf(Not(IsMainFrame()),
                HasViewportIntersection(ViewportIntersection::kIntersecting))));
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Scaled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_scaled.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  Graph* graph = PerformanceManager::GetGraph();
  auto all_frame_nodes = graph->GetAllFrameNodes().AsVector();

  EXPECT_THAT(
      all_frame_nodes,
      UnorderedElementsAre(
          // One main frame, intersects with the viewport.
          AllOf(IsMainFrame(),
                HasViewportIntersection(ViewportIntersection::kIntersecting)),
          // One child frame, also intersects with the viewport.
          AllOf(Not(IsMainFrame()),
                HasViewportIntersection(ViewportIntersection::kIntersecting))));
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Rotated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_rotated.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  Graph* graph = PerformanceManager::GetGraph();
  auto all_frame_nodes = graph->GetAllFrameNodes().AsVector();

  EXPECT_THAT(
      all_frame_nodes,
      UnorderedElementsAre(
          // One main frame, intersects with the viewport.
          AllOf(IsMainFrame(),
                HasViewportIntersection(ViewportIntersection::kIntersecting)),
          // One child frame, also intersects with the viewport.
          AllOf(Not(IsMainFrame()),
                HasViewportIntersection(ViewportIntersection::kIntersecting))));
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, Bind_SimpleNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL kTestUrl = embedded_test_server()->GetURL("/title1.html");

  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), kTestUrl);
  ASSERT_TRUE(rfh);
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsDocumentCoordinatorUnitBound(rfh); }));
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

  const GURL kTestUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kOtherUrl = embedded_test_server()->GetURL("/title2.html");

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

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsDocumentCoordinatorUnitBound(rfh); }));
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

IN_PROC_BROWSER_TEST_F(FrameNodeImplPrerenderBrowserTest,
                       Bind_PrerenderNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderUrl = embedded_test_server()->GetURL("/title1.html");

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

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsDocumentCoordinatorUnitBound(prerender_rfh); }));
}

}  // namespace performance_manager
