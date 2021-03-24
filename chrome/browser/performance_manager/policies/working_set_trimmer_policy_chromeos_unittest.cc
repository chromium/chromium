// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"

#include <memory>

#include "base/memory/memory_pressure_listener.h"

#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/arc/mojom/process.mojom.h"
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

namespace {
using testing::_;
using testing::Exactly;
using testing::Invoke;

// This method as it describes will get the milliseconds since system boot for
// some past time this is necessary because of the JVM using int64 ms since
// system update.
int64_t GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta delta) {
  const base::Time cur_time = base::Time::NowFromSystemTime();
  return (cur_time - delta).ToJavaTime();
}

}  // namespace

class MockWorkingSetTrimmerPolicyChromeOS
    : public WorkingSetTrimmerPolicyChromeOS {
 public:
  MockWorkingSetTrimmerPolicyChromeOS() : WorkingSetTrimmerPolicyChromeOS() {
    // Setup our default configuration
    set_trim_on_memory_pressure(true);
    set_trim_on_freeze(true);
    set_trim_arc_on_memory_pressure(false);

    params().graph_walk_backoff_time = base::TimeDelta::FromSeconds(30);
    params().node_invisible_time = base::TimeDelta::FromSeconds(30);
    params().node_trim_backoff_time = base::TimeDelta::FromSeconds(30);
    params().arc_process_trim_backoff_time = base::TimeDelta::Min();
    params().arc_process_inactivity_time = base::TimeDelta::Min();
    params().trim_arc_aggressive = false;

    // Setup some default invocations.
    ON_CALL(*this, OnMemoryPressure(_))
        .WillByDefault(Invoke(
            this,
            &MockWorkingSetTrimmerPolicyChromeOS::DefaultOnMemoryPressure));

    ON_CALL(*this, TrimArcProcesses)
        .WillByDefault(Invoke(
            this,
            &MockWorkingSetTrimmerPolicyChromeOS::DefaultTrimArcProcesses));

    ON_CALL(*this, TrimReceivedArcProcesses)
        .WillByDefault(Invoke(this, &MockWorkingSetTrimmerPolicyChromeOS::
                                        DefaultTrimReceivedArcProcesses));
  }

  ~MockWorkingSetTrimmerPolicyChromeOS() override {}

  base::MemoryPressureListener& listener() {
    return memory_pressure_listener_.value();
  }

  base::TimeTicks get_last_graph_walk() { return last_graph_walk_; }

  // Allows us to tweak the tests parameters per test.
  features::TrimOnMemoryPressureParams& params() { return params_; }

  // Mock methods related to tab (renderer) per process reclaim.
  MOCK_METHOD1(TrimWorkingSet, bool(const ProcessNode*));
  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

  // Mock methods related to ARC process trimming.
  MOCK_METHOD0(TrimArcProcesses, void(void));
  MOCK_METHOD2(TrimReceivedArcProcesses,
               void(int, arc::ArcProcessService::OptionalArcProcessList));
  MOCK_METHOD1(IsArcProcessEligibleForReclaim, bool(const arc::ArcProcess&));
  MOCK_METHOD1(TrimArcProcess, bool(const base::ProcessId));

  // Exposes the default implementations so they can be used in tests.
  void DefaultOnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure(level);
  }

  void DefaultTrimArcProcesses() {
    WorkingSetTrimmerPolicyChromeOS::TrimArcProcesses();
  }

  void DefaultTrimReceivedArcProcesses(
      int available_to_trim,
      arc::ArcProcessService::OptionalArcProcessList procs) {
    WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses(available_to_trim,
                                                              std::move(procs));
  }

  bool DefaultIsArcProcessEligibleForReclaim(const arc::ArcProcess& proc) {
    return WorkingSetTrimmerPolicyChromeOS::IsArcProcessEligibleForReclaim(
        proc);
  }

  void trim_on_memory_pressure(bool enabled) {
    set_trim_on_memory_pressure(enabled);
  }

  void trim_arc_on_memory_pressure(bool enabled) {
    set_trim_arc_on_memory_pressure(enabled);
  }

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
    auto mock_policy = std::make_unique<
        testing::NiceMock<MockWorkingSetTrimmerPolicyChromeOS>>();

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
// TODO(crbug.com/1051006): Test is flaky
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DISABLED_GraphWalkBackoffPeriod) {
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
// TODO(crbug.com/1051006): Test is flaky
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       DISABLED_GraphWalkAfterBackoffPeriod) {
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
// TODO(crbug.com/1051006): Test is flaky
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       DISABLED_DontTrimIfNotInvisibleLongEnough) {
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

// This test is a simple smoke test to make sure that ARC process trimming
// doesn't run if it's not enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcDontTrimOnlyIfDisabled) {
  // The Mock defaults to ARC trimming disabled, so just validate that we're not
  // actually trimming.
  policy()->trim_arc_on_memory_pressure(false);
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(0);
  FastForwardBy(base::TimeDelta::FromSeconds(1));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

// TODO(crbug.com/1177146) Re-enable test
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DISABLED_ArcTrimOnlyIfEnabled) {
  policy()->trim_arc_on_memory_pressure(true);
  FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(1);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

// This test will validate that we don't fetch the ARC process list at an
// interval that is greater than the configured value, regardless of memory
// pressure levels.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcFetchProcessesAtConfiguredInterval) {
  // Our test setup will validate that we don't attempt to try to fetch and look
  // for ARC processes more than the configured frequency (in this case 60s).
  policy()->trim_arc_on_memory_pressure(true);
  policy()->params().arc_process_list_fetch_backoff_time =
      base::TimeDelta::FromSeconds(60);

  // We're going to cause a moderate pressure notification twice, but we only
  // expect to attempt to fetch the ARC processes once because of our configured
  // backoff time.
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(Exactly(1));

  FastForwardBy(base::TimeDelta::FromSeconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::TimeDelta::FromSeconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Now as we pass through the backoff time we expect that we can be called
  // again.
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(Exactly(1));
  FastForwardBy(policy()->params().arc_process_list_fetch_backoff_time);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

// This test validates that a process which is focused is not eligible.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcProcessFocusedIsNotEligible) {
  policy()->trim_arc_on_memory_pressure(true);

  // This ARC process is focused and should not be eligible for reclaim.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/true,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(2)));

  EXPECT_FALSE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that a process which is focused is not eligible.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcBackgroundProtectIsNotEligible) {
  policy()->trim_arc_on_memory_pressure(true);

  // This validates that a background protected ARC app is not eligible for
  // reclaim, the IMPORTANT_BACKGROUND state is considered background protected.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::IMPORTANT_BACKGROUND,
      /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(2)));

  EXPECT_FALSE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test allows for reclaim of background protected when aggresive mode is
// enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcAggressiveAllowsBackgroundProtected) {
  policy()->trim_arc_on_memory_pressure(true);
  policy()->params().trim_arc_aggressive = true;

  // This validates that a background protected ARC app is eligible for
  // reclaim when trim ARC aggressive is enabled. the IMPORTANT_BACKGROUND state
  // is considered background protected.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::IMPORTANT_BACKGROUND,
      /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(60)));

  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that we can trim important apps when aggresive mode is
// enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcAggressiveAllowsImportant) {
  policy()->trim_arc_on_memory_pressure(true);
  policy()->params().trim_arc_aggressive = true;

  // This validates that an imporant ARC app is eligible for
  // reclaim, the IMPORANT_FOREGROUND state is considered imporant.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::IMPORTANT_FOREGROUND,
      /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(60)));

  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that we never trim an individual ARC process more
// frequently than configured.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcProcessNotTrimmedTooFrequently) {
  policy()->trim_arc_on_memory_pressure(true);

  // For this test we don't care about last activity time.
  policy()->params().arc_process_inactivity_time = base::TimeDelta::Min();

  // We will only allow trimming of an individual process once every 10 seconds.
  policy()->params().arc_process_trim_backoff_time =
      base::TimeDelta::FromSeconds(10);

  // Use a mock ARC process, this process is eligible to be reclaimed so the
  // only thing which would prevent it would be that it was reclaimed too
  // recently.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(2)));

  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));

  // Next, we set the last trim time as Now and confirm that it no
  // longer is eligible for reclaim.
  policy()->SetArcProcessLastTrimTime(mock_arc_proc.pid(),
                                      base::TimeTicks::Now());
  FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));

  // And finally, as we move through the backoff period we should be able to
  // reclaim that process again.
  FastForwardBy(policy()->params().arc_process_trim_backoff_time);
  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that we never trim an ARC process if its last activity
// time is less than the configured value.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcProcessDontTrimOnRecentActivity) {
  policy()->trim_arc_on_memory_pressure(true);

  // We won't trim when the last activity is less than 30s ago.
  policy()->params().arc_process_inactivity_time =
      base::TimeDelta::FromSeconds(30);

  // We don't care about the ARC process trim backoff time for this test.
  policy()->params().arc_process_trim_backoff_time = base::TimeDelta::Min();

  // This mock ARC process will not be eligible because its last activity was
  // only 10seconds ago and our configured last activity cutoff is 30s.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromSeconds(10)));

  // It was active too recently.
  EXPECT_FALSE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));

  // Now if we advance the clock, the last activity time will be beyond that
  // threshold and it'll now be eligible for reclaim.
  FastForwardBy(policy()->params().arc_process_inactivity_time);
  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that we don't trim more than the configured number of arc
// processes per fetch interval.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcDontTrimMoreProcessesPerRoundThanAllowed) {
  policy()->trim_arc_on_memory_pressure(true);

  // In this configuration we will not trim more than 2 ARC processes per round
  // regardless of how many are returned.
  policy()->params().arc_max_number_processes_per_trim = 2;

  // We don't care about the ARC process trim backoff time or last activity time
  // for this test.
  policy()->params().arc_process_inactivity_time = base::TimeDelta::Min();
  policy()->params().arc_process_trim_backoff_time = base::TimeDelta::Min();

  // We add 20 eligible ARC processes to validate that only 2 of them will
  // be reclaimed.
  std::vector<arc::ArcProcess> arc_process_list;
  for (int i = 0; i < 20; ++i) {
    arc_process_list.emplace_back(
        /*nspid=*/0, /*pid=*/1234 + i, "mock process",
        /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/false,
        /*last activity=*/
        GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta::FromMinutes(10)));
  }

  // We expect that only the first two processes will be trimmed because
  // otherwise we would exceed the limit.
  EXPECT_CALL(*policy(), IsArcProcessEligibleForReclaim(_))
      .WillRepeatedly(
          Invoke(policy(), &MockWorkingSetTrimmerPolicyChromeOS::
                               DefaultIsArcProcessEligibleForReclaim));
  EXPECT_CALL(*policy(), TrimArcProcess(_)).Times(Exactly(2));

  policy()->DefaultTrimArcProcesses();
  policy()->DefaultTrimReceivedArcProcesses(
      policy()->params().arc_max_number_processes_per_trim,
      arc::ArcProcessService::OptionalArcProcessList(
          std::move(arc_process_list)));
}

}  // namespace policies
}  // namespace performance_manager
