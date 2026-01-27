// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_loading_browser_network_policy.h"

#include <windows.h>

#include <memory>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/priority_boost_helpers.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/scenarios/process_performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/common/process_type.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

class PriorityBoostLoadingBrowserNetworkPolicyTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(
        std::make_unique<PriorityBoostLoadingBrowserNetworkPolicy>());

    // Since we use base::Process::Current() to instantiate the process nodes in
    // the tests, make sure it is in the default expected state.
    ::SetProcessPriorityBoost(base::Process::Current().Handle(),
                              /*bDisablePriorityBoost=*/false);
  }

  void TearDown() override {
    GraphTestHarness::TearDown();

    // Since we use base::Process::Current() to instantiate the process nodes in
    // the tests, make sure we leave it in the default expected state.
    ::SetProcessPriorityBoost(base::Process::Current().Handle(),
                              /*bDisablePriorityBoost=*/false);
  }
};

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest, BrowserProcess) {
  auto process_node = CreateBrowserProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should not be disabled for the browser process.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest, NetworkProcess) {
  auto network_process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  network_process_node->SetProcessMetricsName(
      network::mojom::NetworkService::Name_);
  network_process_node->SetProcess(base::Process::Current(),
                                   base::TimeTicks::Now());
  ASSERT_TRUE(network_process_node->GetProcess().IsValid());

  // Boost should be disabled for the network process by default when no
  // renderers are loading.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(network_process_node.get()));

  // Simulate a renderer process starting to load.
  auto renderer_process_node = CreateRendererProcessNode();
  renderer_process_node->SetProcess(base::Process::Current(),
                                    base::TimeTicks::Now());
  ASSERT_TRUE(renderer_process_node->GetProcess().IsValid());

  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kBackgroundPageLoading,
      renderer_process_node.get());

  // Boost should now be enabled for the network process.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(network_process_node.get()));

  // Simulate the renderer process finishing loading.
  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kNoPageLoading,
      renderer_process_node.get());

  // Boost should be disabled again for the network process.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(network_process_node.get()));
}

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest, GpuProcess) {
  auto process_node = CreateBrowserChildProcessNode(content::PROCESS_TYPE_GPU);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled for the GPU process.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest, LoadingRendererProcess) {
  auto process_node = CreateRendererProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Priority boost should be disabled now.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));

  // Simulate a loading scenario.
  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kBackgroundPageLoading,
      process_node.get());

  // Priority boost should NOT be disabled now.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(process_node.get()));

  // Simulate the completion of the loading scenario.
  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kNoPageLoading, process_node.get());

  // Priority boost should be disabled again.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest,
       LoadingRendererProcess_ProcessValidDuringLoad) {
  // Have the Network process ready.
  auto network_process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  network_process_node->SetProcessMetricsName(
      network::mojom::NetworkService::Name_);
  network_process_node->SetProcess(base::Process::Current(),
                                   base::TimeTicks::Now());
  ASSERT_TRUE(network_process_node->GetProcess().IsValid());

  auto renderer_process_node = CreateRendererProcessNode();
  // Renderer process is not set yet.
  ASSERT_FALSE(renderer_process_node->GetProcess().IsValid());

  // Simulate a loading scenario.
  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kBackgroundPageLoading,
      renderer_process_node.get());

  // Now set the process, making it valid.
  renderer_process_node->SetProcess(base::Process::Current(),
                                    base::TimeTicks::Now());
  ASSERT_TRUE(renderer_process_node->GetProcess().IsValid());

  // Priority boost should NOT be disabled now. Same for the Network process.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(renderer_process_node.get()));
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(network_process_node.get()));

  // Simulate the completion of the loading scenario.
  SetLoadingScenarioForProcessNode(
      performance_manager::LoadingScenario::kNoPageLoading,
      renderer_process_node.get());

  // Priority boost should be disabled again. Same for the Network process.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(renderer_process_node.get()));
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(network_process_node.get()));
}

TEST_F(PriorityBoostLoadingBrowserNetworkPolicyTest, OtherProcess) {
  auto process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  process_node->SetProcessMetricsName("NonNetworkUtilityProcess");
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled for other process types.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

}  // namespace performance_manager::policies
