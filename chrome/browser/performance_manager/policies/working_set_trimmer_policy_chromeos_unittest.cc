// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/process.mojom.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace mechanism {
class MockWorkingSetTrimmerChromeOS : public WorkingSetTrimmerChromeOS {
 public:
  MockWorkingSetTrimmerChromeOS() {
    ON_CALL(*this, TrimArcVmWorkingSet)
        .WillByDefault(testing::Invoke(
            this, &MockWorkingSetTrimmerChromeOS::DefaultTrimArcVmWorkingSet));
  }
  MOCK_METHOD3(TrimArcVmWorkingSet,
               void(WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback,
                    ArcVmReclaimType,
                    int));

 private:
  void DefaultTrimArcVmWorkingSet(TrimArcVmWorkingSetCallback callback,
                                  ArcVmReclaimType reclaim_type,
                                  int page_limit) {
    std::move(callback).Run(true, "");
  }
};

}  // namespace mechanism

namespace policies {

namespace {
constexpr auto kNotFirstReclaimPostBoot = performance_manager::policies::
    WorkingSetTrimmerPolicyArcVm::kNotFirstReclaimPostBoot;
constexpr auto kYesFirstReclaimPostBoot = performance_manager::policies::
    WorkingSetTrimmerPolicyArcVm::kYesFirstReclaimPostBoot;
using testing::_;
using testing::Exactly;
using testing::Expectation;
using testing::InSequence;
using testing::Invoke;
using testing::Return;

// This method as it describes will get the milliseconds since system boot for
// some past time this is necessary because of the JVM using int64 ms since
// system update.
int64_t GetSystemTimeInPastAsMsSinceUptime(base::TimeDelta delta) {
  const base::Time cur_time = base::Time::NowFromSystemTime();
  return (cur_time - delta).InMillisecondsSinceUnixEpoch();
}

class ScopedTestArcVmDelegate
    : public WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate {
 public:
  ScopedTestArcVmDelegate(WorkingSetTrimmerPolicyChromeOS* policy,
                          mechanism::ArcVmReclaimType eligibility,
                          bool is_first_trim_post_boot)
      : policy_(policy),
        eligibility_(eligibility),
        is_first_trim_post_boot_(is_first_trim_post_boot) {
    policy_->set_arcvm_delegate_for_testing(this);
  }
  ~ScopedTestArcVmDelegate() override {
    policy_->set_arcvm_delegate_for_testing(nullptr);
  }

  ScopedTestArcVmDelegate(const ScopedTestArcVmDelegate&) = delete;
  ScopedTestArcVmDelegate& operator=(const ScopedTestArcVmDelegate&) = delete;

  // WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate overrides:
  mechanism::ArcVmReclaimType IsEligibleForReclaim(
      const base::TimeDelta& arcvm_inactivity_time,
      mechanism::ArcVmReclaimType trim_once_type_after_arcvm_boot,
      bool* is_first_trim_post_boot) override {
    if (is_first_trim_post_boot)
      *is_first_trim_post_boot = is_first_trim_post_boot_;
    return eligibility_;
  }

  void set_eligibility(mechanism::ArcVmReclaimType eligibility) {
    eligibility_ = eligibility;
  }

  void set_is_first_trim_post_boot(bool is_first_trim_post_boot) {
    is_first_trim_post_boot_ = is_first_trim_post_boot;
  }

 private:
  const raw_ptr<WorkingSetTrimmerPolicyChromeOS> policy_;
  mechanism::ArcVmReclaimType eligibility_;
  bool is_first_trim_post_boot_;
};

}  // namespace

class MockWorkingSetTrimmerPolicyChromeOS
    : public WorkingSetTrimmerPolicyChromeOS {
 public:
  MockWorkingSetTrimmerPolicyChromeOS() : WorkingSetTrimmerPolicyChromeOS() {
    // Setup our default configuration
    set_trim_on_freeze(true);
    set_trim_arc_on_memory_pressure(false);
    set_trim_arcvm_on_memory_pressure(false);

    params().graph_walk_backoff_time = base::Seconds(30);
    params().node_invisible_time = base::Seconds(30);
    params().node_trim_backoff_time = base::Seconds(30);
    params().arc_process_trim_backoff_time = base::TimeDelta::Min();
    params().arc_process_inactivity_time = base::TimeDelta::Min();
    params().trim_arc_aggressive = false;
    params().arcvm_inactivity_time = base::TimeDelta::Min();
    params().arcvm_trim_backoff_time = base::TimeDelta::Min();
    params().trim_arcvm_on_critical_pressure = false;
    params().trim_arcvm_on_first_memory_pressure_after_arcvm_boot = false;

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

    ON_CALL(*this, TrimArcVmProcesses)
        .WillByDefault(Invoke(
            this,
            &MockWorkingSetTrimmerPolicyChromeOS::DefaultTrimArcVmProcesses));

    ON_CALL(*this, OnTrimArcVmProcesses)
        .WillByDefault(Invoke(
            this,
            &MockWorkingSetTrimmerPolicyChromeOS::DefaultOnTrimArcVmProcesses));

    ON_CALL(*this, OnArcVmTrimEnded)
        .WillByDefault(Invoke(
            this,
            &MockWorkingSetTrimmerPolicyChromeOS::DefaultOnArcVmTrimEnded));

    ON_CALL(*this, GetTrimmer)
        .WillByDefault(Invoke(
            this, &MockWorkingSetTrimmerPolicyChromeOS::DefaultGetTrimmer));
  }

  MockWorkingSetTrimmerPolicyChromeOS(
      const MockWorkingSetTrimmerPolicyChromeOS&) = delete;
  MockWorkingSetTrimmerPolicyChromeOS& operator=(
      const MockWorkingSetTrimmerPolicyChromeOS&) = delete;

  ~MockWorkingSetTrimmerPolicyChromeOS() override {}

  base::MemoryPressureListener& listener() {
    return memory_pressure_listener_.value();
  }

  base::TimeTicks get_last_graph_walk() {
    return last_graph_walk_ ? *last_graph_walk_ : base::TimeTicks();
  }

  // Allows us to tweak the tests parameters per test.
  features::TrimOnMemoryPressureParams& params() { return params_; }

  // Mock methods related to tab (renderer) per process reclaim.
  MOCK_METHOD1(TrimWorkingSet, void(const ProcessNode*));
  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

  // Mock methods related to ARC process trimming.
  MOCK_METHOD0(TrimArcProcesses, void(void));
  MOCK_METHOD2(TrimReceivedArcProcesses,
               void(int, arc::ArcProcessService::OptionalArcProcessList));
  MOCK_METHOD1(IsArcProcessEligibleForReclaim, bool(const arc::ArcProcess&));
  MOCK_METHOD1(TrimArcProcess, void(const base::ProcessId));

  // Mock methods related to ARCVM process trimming.
  MOCK_METHOD1(TrimArcVmProcesses,
               void(base::MemoryPressureListener::MemoryPressureLevel));
  MOCK_METHOD4(OnTrimArcVmProcesses,
               void(mechanism::ArcVmReclaimType, bool, int, int));
  MOCK_METHOD2(OnArcVmTrimEnded, void(mechanism::ArcVmReclaimType, bool));
  MOCK_METHOD0(GetTrimmer, mechanism::WorkingSetTrimmerChromeOS*(void));

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

  void DefaultTrimArcVmProcesses(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    WorkingSetTrimmerPolicyChromeOS::TrimArcVmProcesses(level);
  }

  void DefaultOnTrimArcVmProcesses(mechanism::ArcVmReclaimType reclaim_type,
                                   bool is_first_trim_post_boot,
                                   int pages_per_minute,
                                   int max_pages_per_iteration) {
    WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmProcesses(
        reclaim_type, is_first_trim_post_boot, pages_per_minute,
        max_pages_per_iteration);
  }

  void DefaultOnArcVmTrimEnded(mechanism::ArcVmReclaimType reclaim_type,
                               bool success) {
    WorkingSetTrimmerPolicyChromeOS::OnArcVmTrimEnded(reclaim_type, success);
  }

  mechanism::WorkingSetTrimmerChromeOS* DefaultGetTrimmer() {
    return WorkingSetTrimmerPolicyChromeOS::GetTrimmer();
  }

  void trim_arc_on_memory_pressure(bool enabled) {
    set_trim_arc_on_memory_pressure(enabled);
  }

  void trim_arcvm_on_memory_pressure(bool enabled) {
    set_trim_arcvm_on_memory_pressure(enabled);
  }
};

class PageNodeContext {
 public:
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> parent_frame_;

  PageNodeContext() = default;
  ~PageNodeContext() = default;
  PageNodeContext(const PageNodeContext&) = delete;
  PageNodeContext& operator=(const PageNodeContext&) = delete;
};

class WorkingSetTrimmerPolicyChromeOSTest : public GraphTestHarness {
 public:
  WorkingSetTrimmerPolicyChromeOSTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        run_loop_(std::make_unique<base::RunLoop>()) {}

  WorkingSetTrimmerPolicyChromeOSTest(
      const WorkingSetTrimmerPolicyChromeOSTest&) = delete;
  WorkingSetTrimmerPolicyChromeOSTest& operator=(
      const WorkingSetTrimmerPolicyChromeOSTest&) = delete;

  ~WorkingSetTrimmerPolicyChromeOSTest() override {}

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    CreateTrimmer();
    GraphTestHarness::SetUp();
    RecreatePolicy(base::BindLambdaForTesting(
        [](MockWorkingSetTrimmerPolicyChromeOS*) {}));
  }

  void TearDown() override {
    // Fix flakiness due to WorkingSetTrimmerPolicyChromeOS's weak ptr factory
    // getting destroyed and causing a weak ptr to get invalidated on a
    // different sequenced thread from where it was bound.
    task_env().RunUntilIdle();
    TakePolicyFromGraph();
    GraphTestHarness::TearDown();
    chromeos::PowerManagerClient::Shutdown();
  }

  void DefaultOnTrimArcVmProcessesAndQuit(
      mechanism::ArcVmReclaimType reclaim_type,
      bool is_first_trim_post_boot,
      int pages_per_minute,
      int max_pages_per_iteration) {
    policy()->DefaultOnTrimArcVmProcesses(reclaim_type, is_first_trim_post_boot,
                                          pages_per_minute,
                                          max_pages_per_iteration);
    run_loop()->Quit();
  }

  void DefaultOnArcVmTrimEndedAndQuit(mechanism::ArcVmReclaimType reclaim_type,
                                      bool success) {
    policy()->DefaultOnArcVmTrimEnded(reclaim_type, success);
    run_loop()->Quit();
  }

  // Creates a new policy and runs the |callback| with the policy before passing
  // it to the graph().
  void RecreatePolicy(
      base::OnceCallback<void(MockWorkingSetTrimmerPolicyChromeOS* policy)>
          callback) {
    if (policy_)
      graph()->TakeFromGraph(policy_);
    // Add our mock policy to the graph.
    auto mock_policy = std::make_unique<
        testing::NiceMock<MockWorkingSetTrimmerPolicyChromeOS>>();
    policy_ = mock_policy.get();
    std::move(callback).Run(policy_.get());
    graph()->PassToGraph(std::move(mock_policy));
  }

  void TakePolicyFromGraph() {
    graph()->TakeFromGraph(policy_);
    policy_ = nullptr;
  }

  void CreateTrimmer() {
    trimmer_ = std::make_unique<mechanism::MockWorkingSetTrimmerChromeOS>();
  }

  mechanism::MockWorkingSetTrimmerChromeOS* trimmer() const {
    return trimmer_.get();
  }

  void RecreateRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  MockWorkingSetTrimmerPolicyChromeOS* policy() { return policy_; }
  features::TrimOnMemoryPressureParams params() { return policy()->params(); }

  base::TimeTicks NowTicks() { return task_env().NowTicks(); }

  base::TimeTicks FastForwardBy(base::TimeDelta delta) {
    task_env().FastForwardBy(delta);
    return NowTicks();
  }

  void ExpectNoReclaim();
  void ExpectFullReclaim(bool is_first_reclaim, int computed_page_limit);
  void ExpectDropPageCaches();

  std::unique_ptr<PageNodeContext> CreateInvisiblePage() {
    // Create a simple graph
    auto context = std::make_unique<PageNodeContext>();
    context->process_node_ = CreateNode<ProcessNodeImpl>();
    context->page_node_ = CreateNode<PageNodeImpl>();
    context->parent_frame_ = CreateFrameNodeAutoId(context->process_node_.get(),
                                                   context->page_node_.get());

    // Create a Process so this process node doesn't bail on Process.IsValid();
    context->process_node_->SetProcess(base::Process::Current().Duplicate(),
                                       /* launch_time=*/base::TimeTicks::Now());

    // Set it invisible using the current clock, then we will advance the clock
    // and it should result in a TrimWorkingSet since it's been invisible long
    // enough.
    context->page_node_->SetIsVisible(
        true);  // Reset visibility and then set invisible.
    context->page_node_->SetIsVisible(false);  // Uses the testing clock.

    return context;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<MockWorkingSetTrimmerPolicyChromeOS,
          DanglingUntriaged>
      policy_ = nullptr;  // Not owned.
  std::unique_ptr<mechanism::MockWorkingSetTrimmerChromeOS> trimmer_;
};

// Validate that we don't walk again before the backoff period has expired.
// TODO(crbug.com/40673488): Test is flaky
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DISABLED_GraphWalkBackoffPeriod) {
  // Since we've never walked the graph we should do so now.
  const base::TimeTicks initial_walk_time = policy()->get_last_graph_walk();
  ASSERT_EQ(initial_walk_time, base::TimeTicks());

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(1));

  // Since we have never walked we expect that we walked it now, we confirm by
  // checking the last walk time against the known clock.
  const base::TimeTicks last_walk_time = policy()->get_last_graph_walk();
  EXPECT_LT(initial_walk_time, last_walk_time);

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(1));

  // We will not have caused a walk as the clock has not advanced beyond the
  // backoff period.
  EXPECT_EQ(last_walk_time, policy()->get_last_graph_walk());
}

// Validate that we will walk the graph again after the backoff period is
// expired.
// TODO(crbug.com/40673488): Test is flaky
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       DISABLED_GraphWalkAfterBackoffPeriod) {
  // Since we've never walked the graph we should do so now.
  const base::TimeTicks initial_walk_time = policy()->get_last_graph_walk();
  ASSERT_EQ(initial_walk_time, base::TimeTicks());

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(1));

  // Since we have never walked we expect that we walked it now, we confirm by
  // checking the last walk time against the known clock.
  const base::TimeTicks last_walk_time = policy()->get_last_graph_walk();
  EXPECT_LT(initial_walk_time, last_walk_time);

  FastForwardBy(base::Days(1));

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Finally advance the clock beyond the backoff period and it should allow it
  // to walk again.
  FastForwardBy(base::Seconds(1));

  const base::TimeTicks final_walk_time = policy()->get_last_graph_walk();
  EXPECT_GT(final_walk_time, last_walk_time);
}

// This test will validate that we will NOT try to trim a node if it has not
// been invisible for long enough.
// TODO(crbug.com/40673488): Test is flaky
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

  FastForwardBy(base::Seconds(1));

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
  FastForwardBy(base::Days(1));

  // We should not be called because we don't have a frame node or process node.
  EXPECT_CALL(*policy(), TrimWorkingSet(testing::_)).Times(0);

  // Triger memory pressure and we should observe the walk.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Days(1));
}

// This test will validate that we WILL trim the working set if it has been
// invisible long enough.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, TrimIfInvisibleLongEnough) {
  auto page = CreateInvisiblePage();
  ASSERT_EQ(1u, graph()->GetAllPageNodes().size());

  const base::TimeTicks cur_time = FastForwardBy(base::Days(365));

  // We will attempt to trim to corresponding ProcessNode since we've been
  // invisible long enough.
  EXPECT_CALL(*policy(), TrimWorkingSet(page->process_node_.get())).Times(1);

  // Triger memory pressure and we should observe the walk since we've never
  // walked before.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(1));

  // We should have triggered the walk and it should have trimmed.
  EXPECT_EQ(cur_time, policy()->get_last_graph_walk());
}

TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DoNotTrimWhileSuspended) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDisableTrimmingWhileSuspended);
  RecreatePolicy(
      base::BindLambdaForTesting([](MockWorkingSetTrimmerPolicyChromeOS*) {}));
  auto page = CreateInvisiblePage();
  FastForwardBy(base::Minutes(15) + base::Seconds(1));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_CALL(*policy(), TrimWorkingSet(testing::_)).Times(0);

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  FastForwardBy(base::Seconds(1));
}

TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DoNotTrimJustAfterResumed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDisableTrimmingWhileSuspended);
  RecreatePolicy(
      base::BindLambdaForTesting([](MockWorkingSetTrimmerPolicyChromeOS*) {}));
  auto page = CreateInvisiblePage();
  FastForwardBy(base::Minutes(15) + base::Seconds(1));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  EXPECT_CALL(*policy(), TrimWorkingSet(testing::_)).Times(0);

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  FastForwardBy(base::Seconds(1));
}

TEST_F(WorkingSetTrimmerPolicyChromeOSTest, Trim15MinutesAfterResumed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDisableTrimmingWhileSuspended);
  RecreatePolicy(
      base::BindLambdaForTesting([](MockWorkingSetTrimmerPolicyChromeOS*) {}));
  auto page = CreateInvisiblePage();
  FastForwardBy(base::Minutes(15) + base::Seconds(1));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  FastForwardBy(base::Minutes(15) + base::Seconds(1));

  EXPECT_CALL(*policy(), TrimWorkingSet(page->process_node_.get())).Times(1);

  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  FastForwardBy(base::Seconds(1));
}

// This test is a simple smoke test to make sure that ARC process trimming
// doesn't run if it's not enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcDontTrimOnlyIfDisabled) {
  // The Mock defaults to ARC trimming disabled, so just validate that we're not
  // actually trimming.
  policy()->trim_arc_on_memory_pressure(false);
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(0);
  FastForwardBy(base::Seconds(1));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

// TODO(crbug.com/40748300) Re-enable test
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, DISABLED_ArcTrimOnlyIfEnabled) {
  policy()->trim_arc_on_memory_pressure(true);
  FastForwardBy(base::Seconds(1));
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
  policy()->params().arc_process_list_fetch_backoff_time = base::Seconds(60);

  // We're going to cause a moderate pressure notification twice, but we only
  // expect to attempt to fetch the ARC processes once because of our configured
  // backoff time.
  EXPECT_CALL(*policy(), TrimArcProcesses).Times(Exactly(1));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(12));
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
      GetSystemTimeInPastAsMsSinceUptime(base::Minutes(2)));

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
      GetSystemTimeInPastAsMsSinceUptime(base::Minutes(2)));

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
      GetSystemTimeInPastAsMsSinceUptime(base::Minutes(60)));

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
      GetSystemTimeInPastAsMsSinceUptime(base::Minutes(60)));

  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));
}

// This test validates that we never trim an individual ARC process more
// frequently than configured.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcProcessNotTrimmedTooFrequently) {
  policy()->trim_arc_on_memory_pressure(true);

  // For this test we don't care about last activity time.
  policy()->params().arc_process_inactivity_time = base::TimeDelta::Min();

  // We will only allow trimming of an individual process once every 10 seconds.
  policy()->params().arc_process_trim_backoff_time = base::Seconds(10);

  // Use a mock ARC process, this process is eligible to be reclaimed so the
  // only thing which would prevent it would be that it was reclaimed too
  // recently.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::Minutes(2)));

  EXPECT_TRUE(policy()->DefaultIsArcProcessEligibleForReclaim(mock_arc_proc));

  // Next, we set the last trim time as Now and confirm that it no
  // longer is eligible for reclaim.
  policy()->SetArcProcessLastTrimTime(mock_arc_proc.pid(),
                                      base::TimeTicks::Now());
  FastForwardBy(base::Seconds(5));
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
  policy()->params().arc_process_inactivity_time = base::Seconds(30);

  // We don't care about the ARC process trim backoff time for this test.
  policy()->params().arc_process_trim_backoff_time = base::TimeDelta::Min();

  // This mock ARC process will not be eligible because its last activity was
  // only 10seconds ago and our configured last activity cutoff is 30s.
  arc::ArcProcess mock_arc_proc(
      /*nspid=*/0, /*pid=*/1234, "mock process",
      /*state=*/arc::mojom::ProcessState::SERVICE, /*is_focused=*/false,
      /*last activity=*/
      GetSystemTimeInPastAsMsSinceUptime(base::Seconds(10)));

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
        GetSystemTimeInPastAsMsSinceUptime(base::Minutes(10)));
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

// This test is a simple smoke test to make sure that ARCVM process trimming
// doesn't run if it's not enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcVmDontTrimOnlyIfDisabled) {
  policy()->trim_arcvm_on_memory_pressure(false);
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(0);
  FastForwardBy(base::Seconds(1));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
}

// This test will validate that we do try to trim the ARCVM process on memory
// pressure when the feature is enabled.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcVmTrimOnlyIfEnabled) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot);

  policy()->trim_arcvm_on_memory_pressure(true);
  FastForwardBy(base::Seconds(1));
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(1);

  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
}

// Builds a list of expectations for a memory pressure event that results in
// no reclaim.
void WorkingSetTrimmerPolicyChromeOSTest::ExpectNoReclaim() {
  auto config_pages_per_minute = policy()->params().trim_arcvm_pages_per_minute;
  auto config_max_pages = policy()->params().trim_arcvm_max_pages_per_iteration;

  // Enforces all EXPECT_CALLS to occur in the order they are stated below.
  InSequence serialize_expected_calls;

  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));

  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot,
                                   config_pages_per_minute, config_max_pages))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));
}

// Builds a list of expectations for a memory pressure event that results in a
// full reclaim, specifying the reclaim parameters:
// |is_first_reclaim| whether or not this will be flagged as a first post-boot
//                    reclaim.
// |computed_page_limit| Expected limit for the number of pages to reclaim.
void WorkingSetTrimmerPolicyChromeOSTest::ExpectFullReclaim(
    bool is_first_reclaim,
    int computed_page_limit) {
  auto config_pages_per_minute = policy()->params().trim_arcvm_pages_per_minute;
  auto config_max_pages = policy()->params().trim_arcvm_max_pages_per_iteration;
  mechanism::ArcVmReclaimType full_reclaim =
      mechanism::ArcVmReclaimType::kReclaimAll;

  // Enforces all EXPECT_CALLS to occur in the order they are stated below.
  InSequence serialize_expected_calls;

  // 1. The request to trim.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));

  // 2. The intermediate forwarding operation. Checking that parameters are
  //    carried forward.
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(full_reclaim, is_first_reclaim,
                                   config_pages_per_minute, config_max_pages))
      .Times(Exactly(1));

  // 3. The call to the underlying trimmer. Validate the computed page limit.
  EXPECT_CALL(*trimmer(),
              TrimArcVmWorkingSet(_, full_reclaim, computed_page_limit))
      .Times(Exactly(1));

  // 4. Expect success and quit the run loop.
  EXPECT_CALL(*policy(), OnArcVmTrimEnded(full_reclaim, true))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnArcVmTrimEndedAndQuit));
}

// Similarly, builds a list of expectations for dropping guest page caches.
void WorkingSetTrimmerPolicyChromeOSTest::ExpectDropPageCaches() {
  // Enforces all EXPECT_CALLS to occur in the order they are stated below.
  InSequence serialize_expected_calls;

  // 1. The request to trim.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));

  // 2. The intermediate forwarding operation. Checking that parameters are
  //    carried forward.
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(
                  mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
                  /*is_first_reclaim=*/true,
                  /*config_pages_per_minute=*/_, /*config_max_pages=*/_))
      .Times(Exactly(1));

  // 3. The call to the underlying trimmer. Validate the computed page limit.
  EXPECT_CALL(
      *trimmer(),
      TrimArcVmWorkingSet(/*callback=*/_,
                          mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
                          /*computed_page_limit=*/_))
      .Times(Exactly(1));

  // 4. Expect success and quit the run loop.
  EXPECT_CALL(*policy(),
              OnArcVmTrimEnded(
                  mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, true))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnArcVmTrimEndedAndQuit));
}

// This test validates the calculation of trim limits.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcVmTrimPageLimits) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot);

  // Set up parameters.
  int config_pages_per_minute = 3500;
  int config_max_pages = 20000;
  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().trim_arcvm_on_first_memory_pressure_after_arcvm_boot =
      true;
  policy()->params().trim_arcvm_on_critical_pressure = false;
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);
  policy()->params().trim_arcvm_pages_per_minute = config_pages_per_minute;
  policy()->params().trim_arcvm_max_pages_per_iteration = config_max_pages;
  policy()->params().arcvm_inactivity_time = base::Seconds(30);

  // Replace the default trimmer with a mock one, so we can verify parameters
  // to it and control success/failure return values.
  EXPECT_CALL(*policy(), GetTrimmer).WillRepeatedly(Return(trimmer()));

  // -------------------------
  // Step 1 of 5:  No reclaim.

  // Tell the fake Arc Delegate to respond as if it is not booted yet.
  delegate.set_eligibility(mechanism::ArcVmReclaimType::kReclaimNone);
  delegate.set_is_first_trim_post_boot(kNotFirstReclaimPostBoot);

  ExpectNoReclaim();

  FastForwardBy(base::Seconds(2));  // Still early in boot.

  // Trigger pressure event and wait until last expectation is met.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
  RecreateRunLoop();

  // -------------------------------------------------------------
  // Step 2 of 5:  First reclaim post-boot, drop guest page caches only.

  // Tell fake Arc delegate to respond with first boot information.
  delegate.set_eligibility(
      mechanism::ArcVmReclaimType::kReclaimGuestPageCaches);
  delegate.set_is_first_trim_post_boot(kYesFirstReclaimPostBoot);

  ExpectDropPageCaches();

  // Advance time just past the back-off setting.
  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time);

  // Trigger pressure event and wait until last expectation is met.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
  RecreateRunLoop();

  // ------------------------------------------------------------------------
  // Step 3 of 5:  Full reclaim after a few minutes

  // Tell fake Arc delegate to respond saying it is NOT first boot.
  delegate.set_eligibility(mechanism::ArcVmReclaimType::kReclaimAll);
  delegate.set_is_first_trim_post_boot(kNotFirstReclaimPostBoot);

  // The very first full reclaim is always done with |config_max_pages|.
  ExpectFullReclaim(kNotFirstReclaimPostBoot, config_max_pages);

  // Advance two times the back-off
  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time * 2);

  // Trigger pressure event and wait until last expectation is met.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
  RecreateRunLoop();

  // ------------------------------------------------------------------------
  // Step 4 of 5:  Full reclaim after a few minutes, confirm per-minute rate.

  // Tell fake Arc delegate to respond saying it is NOT first boot.
  delegate.set_eligibility(mechanism::ArcVmReclaimType::kReclaimAll);
  delegate.set_is_first_trim_post_boot(kNotFirstReclaimPostBoot);

  // Subsequent full reclaims are controlled by |config_pages_per_minute|.
  constexpr int kMinutes = 2;
  ExpectFullReclaim(kNotFirstReclaimPostBoot,
                    config_pages_per_minute * kMinutes);

  // Advance two times the back-off
  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time * kMinutes);

  // Trigger pressure event and wait until last expectation is met.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
  RecreateRunLoop();

  // -----------------------------------------------------------------------
  // Step 5 of 5:  Full reclaim after a long time, confirm cap at max pages.

  // Tell fake Arc delegate to respond saying it is NOT first boot.
  delegate.set_eligibility(mechanism::ArcVmReclaimType::kReclaimAll);
  delegate.set_is_first_trim_post_boot(kNotFirstReclaimPostBoot);

  ExpectFullReclaim(kNotFirstReclaimPostBoot, config_max_pages);

  // Advance quite far into the future, to exceed the maximum reclaim.
  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time * 10);

  // Trigger pressure event and wait until last expectation is met.
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
}

// This test will validate that we don't trim the ARCVM process at an interval
// that is greater than the configured value, regardless of memory pressure
// levels.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcVmTrimProcessesAtConfiguredInterval) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot);

  // Our test setup will validate that we don't attempt to try to trim the ARCVM
  // processes more than the configured frequency (in this case 60s).
  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);

  // We're going to cause a moderate pressure notification twice, but we only
  // expect to attempt to trim ARCVM once because of our configured backoff
  // time.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Now as we pass through the backoff time we expect that we can be called
  // again.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(policy()->params().arcvm_trim_backoff_time);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
}

// Tests the same but with MEMORY_PRESSURE_LEVEL_CRITICAL. The behavior should
// be the same regardless of the pressure level.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcVmTrimProcessesAtConfiguredInterval_Critical) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot);

  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);

  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(policy()->params().arcvm_trim_backoff_time);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
}

// Tests that the actual reclaim is NOT performed when the delegate returns
// kReclaimNone.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcVmTrimProcessesIneligible) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot);

  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);

  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  run_loop()->Run();
  RecreateRunLoop();

  // Repeat the same with CRITICAL.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
}

// Tests that the actual reclaim is performed with the reclaim type the delegate
// returns.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest,
       ArcVmTrimProcessesDropCachesEligible) {
  ScopedTestArcVmDelegate delegate(
      policy(), mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
      kNotFirstReclaimPostBoot);

  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);

  // Verify that OnTrimArcVmProcesses is called with kReclaimGuestPageCaches.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(
      *policy(),
      OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
                           kNotFirstReclaimPostBoot,
                           params().trim_arcvm_pages_per_minute,
                           params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
  RecreateRunLoop();

  // Change the delegate's return value to kReclaimAll. This happens in
  // production too.
  delegate.set_eligibility(mechanism::ArcVmReclaimType::kReclaimAll);

  // Since dropping page caches is not an actual VM trim, a trimming can happen
  // without waiting for the |arcvm_trim_backoff_time|.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  run_loop()->Run();
}

// Tests that the actual reclaim is performed on LEVEL_CRITICAL when the
// delegate returns kReclaimNone but |trim_arcvm_on_critical_pressure| is
// set to true.
TEST_F(WorkingSetTrimmerPolicyChromeOSTest, ArcVmTrimProcessesForceTrim) {
  ScopedTestArcVmDelegate delegate(policy(),
                                   mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot);

  policy()->trim_arcvm_on_memory_pressure(true);
  policy()->params().trim_arcvm_on_critical_pressure = true;
  policy()->params().arcvm_trim_backoff_time = base::Seconds(60);

  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimNone,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(12));
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  run_loop()->Run();
  RecreateRunLoop();

  // Repeat the same with CRITICAL.
  EXPECT_CALL(*policy(), TrimArcVmProcesses).Times(Exactly(1));
  EXPECT_CALL(*policy(),
              OnTrimArcVmProcesses(mechanism::ArcVmReclaimType::kReclaimAll,
                                   kNotFirstReclaimPostBoot,
                                   params().trim_arcvm_pages_per_minute,
                                   params().trim_arcvm_max_pages_per_iteration))
      .Times(Exactly(1))
      .WillOnce(Invoke(this, &WorkingSetTrimmerPolicyChromeOSTest::
                                 DefaultOnTrimArcVmProcessesAndQuit));

  FastForwardBy(base::Seconds(1));
  FastForwardBy(policy()->params().arcvm_trim_backoff_time);
  policy()->listener().SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop()->Run();
}

}  // namespace policies
}  // namespace performance_manager
