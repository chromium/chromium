// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace performance_manager {

using PageDiscarderBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PageDiscarderBrowserTest, DiscardPageNodesUrgent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::RenderFrameHost* frame_host = NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(frame_host);
  auto* contents = content::WebContents::FromRenderFrameHost(frame_host);

  uint64_t total = 0;
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([&page_node, &quit_closure, &total] {
        EXPECT_TRUE(page_node);

        // Simulate that there are PMF estimates available for the frames in
        // this page.
        performance_manager::GraphOperations::VisitFrameTreePreOrder(
            page_node.get(), [&total](const FrameNode* frame_node) {
              total += 1;
              FrameNodeImpl::FromNode(frame_node)
                  ->SetPrivateFootprintKbEstimate(1);
              return true;
            });

        mechanism::PageDiscarder discarder;
        discarder.DiscardPageNodes(
            {page_node.get()}, ::mojom::LifecycleUnitDiscardReason::URGENT,
            base::BindLambdaForTesting([&quit_closure](bool success) {
              EXPECT_TRUE(success);
              std::move(quit_closure).Run();
            }));
      }));
  run_loop.Run();

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(new_contents);
  EXPECT_TRUE(pre_discard_resource_usage);
  EXPECT_EQ(total, pre_discard_resource_usage->memory_footprint_estimate_kb());
}

IN_PROC_BROWSER_TEST_F(PageDiscarderBrowserTest, DiscardPageNodesProactive) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::RenderFrameHost* frame_host = NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(frame_host);
  auto* contents = content::WebContents::FromRenderFrameHost(frame_host);

  uint64_t total = 0;
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([&page_node, &quit_closure, &total] {
        EXPECT_TRUE(page_node);

        // Simulate that there are PMF estimates available for the frames in
        // this page.
        performance_manager::GraphOperations::VisitFrameTreePreOrder(
            page_node.get(), [&total](const FrameNode* frame_node) {
              total += 1;
              FrameNodeImpl::FromNode(frame_node)
                  ->SetPrivateFootprintKbEstimate(1);
              return true;
            });

        mechanism::PageDiscarder discarder;
        discarder.DiscardPageNodes(
            {page_node.get()}, ::mojom::LifecycleUnitDiscardReason::PROACTIVE,
            base::BindLambdaForTesting([&quit_closure](bool success) {
              EXPECT_TRUE(success);
              std::move(quit_closure).Run();
            }));
      }));
  run_loop.Run();

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());
  auto* pre_discard_resource_usage =
      performance_manager::user_tuning::UserPerformanceTuningManager::
          PreDiscardResourceUsage::FromWebContents(new_contents);
  EXPECT_TRUE(pre_discard_resource_usage);
  EXPECT_EQ(total, pre_discard_resource_usage->memory_footprint_estimate_kb());
}

}  // namespace performance_manager
