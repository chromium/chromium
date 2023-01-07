// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

// Templated PassToGraph helper that also returns a pointer to the object.
template <typename DerivedType>
DerivedType* PassToPMGraph(std::unique_ptr<DerivedType> graph_owned) {
  DerivedType* object = graph_owned.get();
  PerformanceManagerImpl::PassToGraph(FROM_HERE, std::move(graph_owned));
  return object;
}

// A FrameNodeObserver that allows waiting until a frame's viewport intersection
// is initialized to a set value.
class ViewportIntersectionChangedObserver
    : public GraphOwned,
      public FrameNode::ObserverDefaultImpl {
 public:
  // Needed to filter OnViewportIntersectionChanged() notifications for frames
  // that aren't under test. Since the frame node does not exist before the
  // navigation, it is not possible to directly compare the frame node pointer.
  // Note: The URL of the frame does not work because the initialization of the
  // viewport intersection can happen before the document URL is known.
  using FrameNodeMatcher = base::RepeatingCallback<bool(const FrameNode*)>;

  ViewportIntersectionChangedObserver(
      FrameNodeMatcher frame_node_matcher,
      const gfx::Rect& expected_viewport_intersection,
      base::OnceClosure quit_closure)
      : frame_node_matcher_(std::move(frame_node_matcher)),
        expected_viewport_intersection_(expected_viewport_intersection),
        quit_closure_(std::move(quit_closure)) {}
  ~ViewportIntersectionChangedObserver() override = default;

  ViewportIntersectionChangedObserver(
      const ViewportIntersectionChangedObserver&) = delete;
  ViewportIntersectionChangedObserver& operator=(
      const ViewportIntersectionChangedObserver&) = delete;

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

    EXPECT_EQ(*frame_node->GetViewportIntersection(),
              expected_viewport_intersection_);
    std::move(quit_closure_).Run();
  }

 private:
  const FrameNodeMatcher frame_node_matcher_;
  const gfx::Rect expected_viewport_intersection_;
  base::OnceClosure quit_closure_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       DISABLED_ViewportIntersection_OutOfView) {
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
  const gfx::Rect kExpectedViewportIntersection(0, 0, 0, 0);
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE, std::make_unique<ViewportIntersectionChangedObserver>(
                     std::move(frame_node_matcher),
                     kExpectedViewportIntersection, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       DISABLED_ViewportIntersection_Hidden) {
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
  const gfx::Rect kExpectedViewportIntersection(0, 0, 0, 0);
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE, std::make_unique<ViewportIntersectionChangedObserver>(
                     std::move(frame_node_matcher),
                     kExpectedViewportIntersection, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_hidden.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       DISABLED_ViewportIntersection_PartiallyVisible) {
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
  // The frame is a 100x100 px square centered on the origin of the
  // viewport. Thus only the bottom right quarter is visible
  const gfx::Rect kExpectedViewportIntersection(0, 0, 50, 50);
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE, std::make_unique<ViewportIntersectionChangedObserver>(
                     std::move(frame_node_matcher),
                     kExpectedViewportIntersection, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_partially_visible.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       DISABLED_ViewportIntersection_Scaled) {
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
  const gfx::Rect kExpectedViewportIntersection = []() {
    gfx::Rect expected_viewport_intersection;

    // The iframe is a 200x200 px square centered at (200, 200) scaled
    // to 1.5x its size from it's center.

    // The size should be 50% larger.
    expected_viewport_intersection.set_size(gfx::Size(300, 300));

    // Because the resulting square is still centered at (200, 200), its
    // origin is (200-width/2, 200-height/2) = (50, 50)
    expected_viewport_intersection.set_origin(gfx::Point(50, 50));

    return expected_viewport_intersection;
  }();
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE, std::make_unique<ViewportIntersectionChangedObserver>(
                     std::move(frame_node_matcher),
                     kExpectedViewportIntersection, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_scaled.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameNodeImplBrowserTest,
                       DISABLED_ViewportIntersection_Rotated) {
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
  const gfx::Rect kExpectedViewportIntersection = []() {
    // The iframe is a 100x100 px square centered at (150, 150) rotated by
    // 45 degree around its center.

    // This results in a diamond shape also centered at (150, 150), whose
    // width can be calculated with the pythagorean theorem.
    const float width = sqrt(100 * 100 + 100 * 100);
    const float start = 150 - width / 2;
    const gfx::RectF enclosing_rectf(start, start, width, width);
    // Thus the expectation for the viewport intersection is to be equal to
    // the smallest Rect that encloses the |enclosing_rectf|.
    return ToEnclosingRect(enclosing_rectf);
  }();
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE, std::make_unique<ViewportIntersectionChangedObserver>(
                     std::move(frame_node_matcher),
                     kExpectedViewportIntersection, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_rotated.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

}  // namespace performance_manager
