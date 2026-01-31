// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_browser_network_policy.h"

#include <windows.h>

#include <memory>

#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/priority_boost_helpers.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/common/process_type.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

class PriorityBoostBrowserNetworkPolicyTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(std::make_unique<PriorityBoostBrowserNetworkPolicy>());

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

TEST_F(PriorityBoostBrowserNetworkPolicyTest, BrowserProcess) {
  auto process_node = CreateBrowserProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should not be disabled for the browser process.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostBrowserNetworkPolicyTest, NetworkProcess) {
  auto process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  process_node->SetProcessMetricsName(network::mojom::NetworkService::Name_);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should not be disabled for the network process.
  EXPECT_TRUE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostBrowserNetworkPolicyTest, GpuProcess) {
  auto process_node = CreateBrowserChildProcessNode(content::PROCESS_TYPE_GPU);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled for the GPU process.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostBrowserNetworkPolicyTest, RendererProcess) {
  auto process_node = CreateRendererProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled for a renderer process.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

TEST_F(PriorityBoostBrowserNetworkPolicyTest, OtherProcess) {
  auto process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  process_node->SetProcessMetricsName("NonNetworkUtilityProcess");
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled for other process types.
  EXPECT_FALSE(IsProcessPriorityBoostEnabled(process_node.get()));
}

}  // namespace performance_manager::policies
