// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_disabler.h"

#include <windows.h>

#include <memory>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/scenarios/process_performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/common/process_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PriorityBoostDisablerTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    // Pass the policy to the graph.
    graph()->PassToGraph(std::make_unique<PriorityBoostDisabler>());
  }
};

TEST_F(PriorityBoostDisablerTest, DisableBoostAfterLoading) {
  auto process_node = CreateRendererProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // By default, priority boost is enabled.
  BOOL boost_disabled = FALSE;
  ASSERT_TRUE(::GetProcessPriorityBoost(process_node->GetProcess().Handle(),
                                        &boost_disabled));
  EXPECT_FALSE(boost_disabled);

  // Simulate a loading scenario.
  SetLoadingScenarioForProcessNode(LoadingScenario::kBackgroundPageLoading,
                                   process_node.get());

  // Boost should still be enabled during loading.
  ASSERT_TRUE(::GetProcessPriorityBoost(process_node->GetProcess().Handle(),
                                        &boost_disabled));
  EXPECT_FALSE(boost_disabled);

  // After the loading scenario ends, the priority boost should be disabled.
  SetLoadingScenarioForProcessNode(LoadingScenario::kNoPageLoading,
                                   process_node.get());

  ASSERT_TRUE(::GetProcessPriorityBoost(process_node->GetProcess().Handle(),
                                        &boost_disabled));
  EXPECT_TRUE(boost_disabled);
}

TEST_F(PriorityBoostDisablerTest,
       DisableBoostAfterLoading_ProcessValidAfterLoad) {
  auto process_node = CreateRendererProcessNode();
  // Process is not set yet.
  ASSERT_FALSE(process_node->GetProcess().IsValid());

  // Simulate a loading scenario that ends before this process is valid..
  SetLoadingScenarioForProcessNode(LoadingScenario::kBackgroundPageLoading,
                                   process_node.get());
  SetLoadingScenarioForProcessNode(LoadingScenario::kNoPageLoading,
                                   process_node.get());

  // Now set the process, making it valid.
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Priority boost should be disabled now.
  BOOL boost_disabled = FALSE;
  ASSERT_TRUE(::GetProcessPriorityBoost(process_node->GetProcess().Handle(),
                                        &boost_disabled));
  EXPECT_TRUE(boost_disabled);
}

TEST_F(PriorityBoostDisablerTest, NonRendererProcesses) {
  auto process_node =
      CreateBrowserChildProcessNode(content::PROCESS_TYPE_UTILITY);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Boost should be disabled right away for non-renderer processes.
  BOOL boost_disabled = FALSE;
  ASSERT_TRUE(::GetProcessPriorityBoost(process_node->GetProcess().Handle(),
                                        &boost_disabled));
  EXPECT_TRUE(boost_disabled);
}

TEST_F(PriorityBoostDisablerTest, ProcessRemoval) {
  // Create a renderer process node.
  auto process_node = CreateRendererProcessNode();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  ASSERT_TRUE(process_node->GetProcess().IsValid());

  // Check that removing the node doesn't crash.
  process_node.reset();
}

}  // namespace performance_manager
