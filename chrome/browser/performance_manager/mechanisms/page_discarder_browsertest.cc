// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"

#include <map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
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
      public ::testing::WithParamInterface<LifecycleUnitDiscardReason> {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    RunInGraph([&](Graph* graph) {
      resource_attribution::MemoryMeasurementDelegate::
          SetDelegateFactoryForTesting(graph, &fake_memory_factory_);
    });
  }

  void TearDownOnMainThread() override {
    RunInGraph([](Graph* graph) {
      resource_attribution::MemoryMeasurementDelegate::
          SetDelegateFactoryForTesting(graph, nullptr);
    });
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

 protected:
  resource_attribution::FakeMemoryMeasurementDelegateFactory
      fake_memory_factory_;
};

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
        page_node.get(), [&](const FrameNode* frame_node) {
          total += 1;
          // The memory delegate assigns test data to processes, and Resource
          // Attribution splits that out to frames and pages.
          const auto process_context =
              frame_node->GetProcessNode()->GetResourceContext();
          fake_memory_factory_.memory_summaries()[process_context]
              .private_footprint_kb += 1;
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
