// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy_chromeos.h"

#include "base/memory/memory_pressure_listener.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

class MockWorkingSetTrimmerPolicyChromeOS
    : public WorkingSetTrimmerPolicyChromeOS {
 public:
  MockWorkingSetTrimmerPolicyChromeOS() {
    trim_on_memory_pressure_enabled_ = true;
    trim_on_freeze_enabled_ = true;

    // Setup parameters for trimming on memory pressure.
    trim_on_memory_pressure_params_.graph_walk_backoff_time =
        base::TimeDelta::FromSeconds(30);

    trim_on_memory_pressure_params_.node_invisible_time =
        base::TimeDelta::FromSeconds(30);

    trim_on_memory_pressure_params_.node_trim_backoff_time =
        base::TimeDelta::FromSeconds(30);
  }

  ~MockWorkingSetTrimmerPolicyChromeOS() override {}

  base::MemoryPressureListener& listener() {
    return memory_pressure_listener_.value();
  }

  base::TimeTicks get_last_graph_walk() { return last_graph_walk_; }

  MOCK_METHOD1(TrimWorkingSet, bool(const ProcessNode*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWorkingSetTrimmerPolicyChromeOS);
};

class WorkingSetTrimmerPolicyChromeOSTest : public GraphTestHarness {
 public:
  WorkingSetTrimmerPolicyChromeOSTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~WorkingSetTrimmerPolicyChromeOSTest() override {}

  void SetUp() override {
    GraphTestHarness::SetUp();

    // Add our mock policy to the graph.
    std::unique_ptr<MockWorkingSetTrimmerPolicyChromeOS> mock_policy(
        new MockWorkingSetTrimmerPolicyChromeOS);
    policy_ = mock_policy.get();
    graph()->PassToGraph(std::move(mock_policy));
  }

  void TearDown() override {
    policy_ = nullptr;
    GraphTestHarness::TearDown();
  }

  MockWorkingSetTrimmerPolicyChromeOS* policy() { return policy_; }

  base::TimeTicks NowTicks() { return task_env().NowTicks(); }

  base::TimeTicks FastForwardBy(base::TimeDelta delta) {
    task_env().FastForwardBy(delta);
    return NowTicks();
  }

 private:
  MockWorkingSetTrimmerPolicyChromeOS* policy_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerPolicyChromeOSTest);
};

// Validate that we don't walk again before the backoff period has expired.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, GraphWalkBackoffPeriod) {
  // Since we've never walked the graph we should do so now.
  const base::TimeTicks initial_walk_time = policy()->get_last_graph_walk();
  ASSERT_EQ(initial_walk_time, base::TimeTicks());

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Since we have never walked we expect that we walked it now, we confirm by
  // checking the last walk time against the known clock.
  const base::TimeTicks last_walk_time = policy()->get_last_graph_walk();
  EXPECT_LT(initial_walk_time, last_walk_time);

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(1));

  // We will not have caused a walk as the clock has not advanced beyond the
  // backoff period.
  EXPECT_EQ(last_walk_time, policy()->get_last_graph_walk());
}

// Validate that we will walk the graph again after the backoff period is
// expired.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, GraphWalkAfterBackoffPeriod) {
  // Since we've never walked the graph we should do so now.
  const base::TimeTicks initial_walk_time = policy()->get_last_graph_walk();
  ASSERT_EQ(initial_walk_time, base::TimeTicks());

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Since we have never walked we expect that we walked it now, we confirm by
  // checking the last walk time against the known clock.
  const base::TimeTicks last_walk_time = policy()->get_last_graph_walk();
  EXPECT_LT(initial_walk_time, last_walk_time);

  FastForwardBy(base::TimeDelta::FromDays(1));

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Finally advance the clock beyond the backoff period and it should allow it
  // to walk again.
  FastForwardBy(base::TimeDelta::FromSeconds(1));

  const base::TimeTicks final_walk_time = policy()->get_last_graph_walk();
  EXPECT_GT(final_walk_time, last_walk_time);
}

// This test will validate that we will NOT try to trim a node if it has not
// been invisible for long enough.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DontTrimIfNotInvisibleLongEnough) {
  // Create a simple graph
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto parent_frame =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());

  // Since we've never walked the graph we should do so now.
  const base::TimeTicks clock_time = NowTicks();
  const base::TimeTicks initial_walk_time = policy()->get_last_graph_walk();

  // Set the PageNode to invisible but the state change time to now, since it
  // will not have been invisible long enough it will NOT trigger a call to
  // TrimWorkingSet.
  page_node->SetIsVisible(true);   // Reset visibility and set invisible Now.
  page_node->SetIsVisible(false);  // Uses the testing clock.
  EXPECT_CALL(*policy(), TrimWorkingSet(testing::_)).Times(0);

  // Triger memory pressure and we should observe the walk.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(1));

  const base::TimeTicks current_walk_time = policy()->get_last_graph_walk();
  EXPECT_EQ(clock_time, current_walk_time);
  EXPECT_NE(current_walk_time, initial_walk_time);
}

// This test will validate that we skip a page node that doesn't have a main
// frame node.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DontTrimIfNoMainFrame) {
  // Create a lone page node.
  auto page_node = CreateNode<PageNodeImpl>();

  // Make sure the node is not visible for 1 day.
  page_node->SetIsVisible(true);   // Reset visibility and set invisible Now.
  page_node->SetIsVisible(false);  // Uses the testing clock.
  FastForwardBy(base::TimeDelta::FromDays(1));

  // We should not be called because we don't have a frame node or process node.
  EXPECT_CALL(*policy(), TrimWorkingSet(testing::_)).Times(0);

  // Triger memory pressure and we should observe the walk.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromDays(1));
}

// This test will validate that we WILL trim the working set if it has been
// invisible long enough.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, TrimIfInvisibleLongEnough) {
  // Create a simple graph
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto parent_frame =
      CreateFrameNodeAutoId(process_node.get(), page_node.get());

  ASSERT_EQ(1u, graph()->GetAllPageNodes().size());

  // Create a Process so this process node doesn't bail on Process.IsValid();
  const base::Process self = base::Process::Current();
  auto duplicate = self.Duplicate();
  ASSERT_TRUE(duplicate.IsValid());
  process_node->SetProcess(std::move(duplicate), base::Time::Now());

  // Set it invisible using the current clock, then we will advance the clock
  // and it should result in a TrimWorkingSet since it's been invisible long
  // enough.
  page_node->SetIsVisible(true);   // Reset visibility and then set invisible.
  page_node->SetIsVisible(false);  // Uses the testing clock.
  const base::TimeTicks cur_time =
      FastForwardBy(base::TimeDelta::FromDays(365));

  // We will attempt to trim to corresponding ProcessNode since we've been
  // invisible long enough.
  EXPECT_CALL(*policy(), TrimWorkingSet(process_node.get())).Times(1);

  // Triger memory pressure and we should observe the walk since we've never
  // walked before.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(1));

  // We should have triggered the walk and it should have trimmed.
  EXPECT_EQ(cur_time, policy()->get_last_graph_walk());
}

}  // namespace policies
}  // namespace performance_manager
