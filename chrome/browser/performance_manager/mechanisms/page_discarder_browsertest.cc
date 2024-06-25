// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace performance_manager {

using LifecycleUnitDiscardReason = ::mojom::LifecycleUnitDiscardReason;
using PageDiscarder = mechanism::PageDiscarder;

class PageDiscarderBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<LifecycleUnitDiscardReason> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PageDiscarderBrowserTest,
    ::testing::Values(LifecycleUnitDiscardReason::URGENT,
                      LifecycleUnitDiscardReason::PROACTIVE));

IN_PROC_BROWSER_TEST_P(PageDiscarderBrowserTest, DiscardPageNodes) {
  const LifecycleUnitDiscardReason discard_reason = GetParam();

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
  RunInGraph([&](base::OnceClosure quit_closure) {
    EXPECT_TRUE(page_node);

    // Simulate that there are PMF estimates available for the frames in
    // this page.
    GraphOperations::VisitFrameTreePreOrder(
        page_node.get(), [&total](const FrameNode* frame_node) {
          total += 1;
          FrameNodeImpl::FromNode(frame_node)->SetPrivateFootprintKbEstimate(1);
          return true;
        });

    PageDiscarder discarder;
    discarder.DiscardPageNodes(
        {page_node.get()}, discard_reason,
        base::BindOnce([](const std::vector<PageDiscarder::DiscardEvent>&
                              discard_events) {
          EXPECT_EQ(discard_events.size(), 1U);
        }).Then(std::move(quit_closure)));
  });

  auto* new_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents->WasDiscarded());
  auto* pre_discard_resource_usage = user_tuning::UserPerformanceTuningManager::
      PreDiscardResourceUsage::FromWebContents(new_contents);
  EXPECT_TRUE(pre_discard_resource_usage);
  EXPECT_EQ(total, pre_discard_resource_usage->memory_footprint_estimate_kb());
}

}  // namespace performance_manager
