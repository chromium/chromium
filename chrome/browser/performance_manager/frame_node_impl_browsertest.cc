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
class IntersectsViewportChangedObserver
    : public GraphOwned,
      public FrameNode::ObserverDefaultImpl {
 public:
  // Needed to filter OnIntersectsViewportChanged() notifications for frames
  // that aren't under test. Since the frame node does not exist before the
  // navigation, it is not possible to directly compare the frame node pointer.
  // Note: The URL of the frame does not work because the initialization of the
  // viewport intersection can happen before the document URL is known.
  using FrameNodeMatcher = base::RepeatingCallback<bool(const FrameNode*)>;

  IntersectsViewportChangedObserver(FrameNodeMatcher frame_node_matcher,
                                    bool expected_intersects_viewport,
                                    base::OnceClosure quit_closure)
      : frame_node_matcher_(std::move(frame_node_matcher)),
        expected_intersects_viewport_(expected_intersects_viewport),
        quit_closure_(std::move(quit_closure)) {}
  ~IntersectsViewportChangedObserver() override = default;

  IntersectsViewportChangedObserver(const IntersectsViewportChangedObserver&) =
      delete;
  IntersectsViewportChangedObserver& operator=(
      const IntersectsViewportChangedObserver&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddFrameNodeObserver(this);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveFrameNodeObserver(this);
  }

  // FrameNodeObserver:
  void OnIntersectsViewportChanged(const FrameNode* frame_node) override {
    if (!frame_node_matcher_.Run(frame_node))
      return;

    EXPECT_EQ(frame_node->IntersectsViewport().value(),
              expected_intersects_viewport_);
    std::move(quit_closure_).Run();
  }

 private:
  const FrameNodeMatcher frame_node_matcher_;
  const bool expected_intersects_viewport_;
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
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<IntersectsViewportChangedObserver>(
          std::move(frame_node_matcher), false, run_loop.QuitClosure()));

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
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<IntersectsViewportChangedObserver>(
          std::move(frame_node_matcher), false, run_loop.QuitClosure()));

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
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<IntersectsViewportChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

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
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<IntersectsViewportChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

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
  base::RunLoop run_loop;
  PerformanceManagerImpl::PassToGraph(
      FROM_HERE,
      std::make_unique<IntersectsViewportChangedObserver>(
          std::move(frame_node_matcher), true, run_loop.QuitClosure()));

  // Navigate.
  const GURL main_frame_url(
      embedded_test_server()->GetURL("/iframe_rotated.html"));
  browser()->OpenURL(content::OpenURLParams(main_frame_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  run_loop.Run();
}

}  // namespace performance_manager
