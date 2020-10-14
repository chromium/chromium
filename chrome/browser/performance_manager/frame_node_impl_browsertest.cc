// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace performance_manager {

namespace {

class FrameNodeImplBrowserTest : public InProcessBrowserTest {
 public:
  FrameNodeImplBrowserTest() = default;
  ~FrameNodeImplBrowserTest() override = default;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       ViewportIntersection_OutOfView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_out_of_view.html"));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Ensure that loading is complete.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that the viewport intersection has been set correctly on the graph.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    auto children = page_node.get()->GetMainFrameNode()->GetChildFrameNodes();
    EXPECT_EQ(1U, children.size());
    auto* iframe =
        performance_manager::FrameNodeImpl::FromNode(*children.begin());
    EXPECT_TRUE(iframe->viewport_intersection().IsEmpty());
    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Hidden) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_hidden.html"));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Ensure that loading is complete.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that the viewport intersection has been set correctly on the graph.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    auto children = page_node.get()->GetMainFrameNode()->GetChildFrameNodes();
    EXPECT_EQ(1U, children.size());
    auto* iframe =
        performance_manager::FrameNodeImpl::FromNode(*children.begin());
    EXPECT_TRUE(iframe->viewport_intersection().IsEmpty());
    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       ViewportIntersection_PartiallyVisible) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_partially_visible.html"));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Ensure that loading is complete.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that the viewport intersection has been set correctly on the graph.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    auto children = page_node.get()->GetMainFrameNode()->GetChildFrameNodes();
    EXPECT_EQ(1U, children.size());
    auto* iframe =
        performance_manager::FrameNodeImpl::FromNode(*children.begin());

    // The frame is a 100x100 px square centered on the origin of the viewport.
    // Thus only the bottom right quarter is visible
    const gfx::Rect kExpectedViewportIntersection(0, 0, 50, 50);
    EXPECT_EQ(iframe->viewport_intersection(), kExpectedViewportIntersection);
    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Scaled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_scaled.html"));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Ensure that loading is complete.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that the viewport intersection has been set correctly on the graph.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    auto children = page_node.get()->GetMainFrameNode()->GetChildFrameNodes();
    EXPECT_EQ(1U, children.size());
    auto* iframe =
        performance_manager::FrameNodeImpl::FromNode(*children.begin());

    // The iframe is a 200x200 px square centered at (200, 200) scaled to 1.5x
    // its size from it's center.

    // The size should be 50% larger.
    EXPECT_EQ(iframe->viewport_intersection().size(), gfx::Size(300, 300));

    // Because the resulting square is still centered at (200, 200), its origin
    // is (200-width/2, 200-height/2) = (50, 50)
    EXPECT_EQ(iframe->viewport_intersection().origin(), gfx::Point(50, 50));

    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest, ViewportIntersection_Rotated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_rotated.html"));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate.
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Ensure that loading is complete.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that the viewport intersection has been set correctly on the graph.
  base::RunLoop run_loop;
  auto call_on_graph_cb = base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    auto children = page_node.get()->GetMainFrameNode()->GetChildFrameNodes();
    EXPECT_EQ(1U, children.size());
    auto* iframe =
        performance_manager::FrameNodeImpl::FromNode(*children.begin());

    // The iframe is a 100x100 px square centered at (150, 150) rotated by 45
    // degree around its center.

    // This results in a diamond shape also centered at (150, 150), whose width
    // can be calculated with the pythagorean theorem.
    const float width = sqrt(100 * 100 + 100 * 100);
    const float start = 150 - width / 2;
    const gfx::RectF enclosing_rectf(start, start, width, width);

    // Thus the expectation for the viewport intersection is to be equal to the
    // smallest Rect that encloses the |enclosing_rectf|.
    const gfx::Rect expected_viewport_intersection =
        ToEnclosingRect(enclosing_rectf);

    EXPECT_EQ(iframe->viewport_intersection(), expected_viewport_intersection);

    run_loop.Quit();
  });
  PerformanceManager::CallOnGraph(FROM_HERE, call_on_graph_cb);
  run_loop.Run();
}

}  // namespace performance_manager
