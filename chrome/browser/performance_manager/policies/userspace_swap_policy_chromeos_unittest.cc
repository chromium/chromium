// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/userspace_swap_policy_chromeos.h"

#include "base/allocator/buildflags.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

namespace {
using ::ash::memory::userspace_swap::UserspaceSwapConfig;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class MockUserspaceSwapPolicy : public UserspaceSwapPolicy {
 public:
  MockUserspaceSwapPolicy() : UserspaceSwapPolicy(InitTestConfig()) {}

  MockUserspaceSwapPolicy(const MockUserspaceSwapPolicy&) = delete;
  MockUserspaceSwapPolicy& operator=(const MockUserspaceSwapPolicy&) = delete;

  ~MockUserspaceSwapPolicy() override {}

  MOCK_METHOD0(SwapNodesOnGraph, void(void));
  MOCK_METHOD1(InitializeProcessNode, bool(const ProcessNode*));
  MOCK_METHOD2(IsEligibleToSwap, bool(const ProcessNode*, const PageNode*));
  MOCK_METHOD1(SwapProcessNode, void(const ProcessNode*));
  MOCK_METHOD0(GetSwapDeviceFreeSpaceBytes, uint64_t(void));
  MOCK_METHOD0(GetTotalSwapFileUsageBytes, uint64_t(void));
  MOCK_METHOD1(GetProcessNodeSwapFileUsageBytes, uint64_t(const ProcessNode*));
  MOCK_METHOD1(IsPageNodeAudible, bool(const PageNode*));
  MOCK_METHOD1(IsPageNodeVisible, bool(const PageNode*));
  MOCK_METHOD1(IsPageNodeLoading, bool(const PageNode*));
  MOCK_METHOD1(GetTimeSinceLastVisibilityChange,
               base::TimeDelta(const PageNode*));

  // Allow our mock to dispatch to default implementations.
  bool DefaultIsEligibleToSwap(const ProcessNode* process_node,
                               const PageNode* page_node) {
    return UserspaceSwapPolicy::IsEligibleToSwap(process_node, page_node);
  }

  bool DefaultInitializeProcessNode(const ProcessNode* process_node) {
    return UserspaceSwapPolicy::InitializeProcessNode(process_node);
  }

  void DefaultSwapNodesOnGraph() {
    return UserspaceSwapPolicy::SwapNodesOnGraph();
  }

  base::TimeTicks get_last_graph_walk() { return last_graph_walk_; }
  void set_last_graph_walk(base::TimeTicks t) { last_graph_walk_ = t; }

  // We allow tests to modify the config for testing individual behaviors.
  UserspaceSwapConfig& config() { return test_config_; }

 private:
  const UserspaceSwapConfig& InitTestConfig() {
    // Create a simple starting config that can be modified as needed for tests.
    // NOTE: We only initialize the configuration options which are used by the
    // policy.
    memset(&test_config_, 0, sizeof(test_config_));

    test_config_.enabled = true;
    test_config_.graph_walk_frequency = base::Seconds(10);
    test_config_.invisible_time_before_swap = base::Seconds(30);
    test_config_.process_swap_frequency = base::Seconds(60);
    test_config_.swap_on_freeze = true;
    test_config_.swap_on_moderate_pressure = true;
    return test_config_;
  }

  UserspaceSwapConfig test_config_ = {};
};

class UserspaceSwapPolicyTest : public ::testing::Test {
 public:
  UserspaceSwapPolicyTest()
      : browser_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  UserspaceSwapPolicyTest(const UserspaceSwapPolicyTest&) = delete;
  UserspaceSwapPolicyTest& operator=(const UserspaceSwapPolicyTest&) = delete;

  ~UserspaceSwapPolicyTest() override {}

  void SetUp() override {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      GTEST_SKIP() << "Skip test on chromeos-linux";
    }

    graph_ = std::make_unique<TestGraphImpl>();
    graph_->SetUp();

    CreateAndPassMockPolicy();

    // Create a simple graph.
    process_node_ = CreateNode<ProcessNodeImpl>();
    page_node_ = CreateNode<PageNodeImpl>();
    frame_node_ =
        graph()->CreateFrameNodeAutoId(process_node().get(), page_node().get());
    system_node_ = std::make_unique<TestNodeWrapper<SystemNodeImpl>>(
        TestNodeWrapper<SystemNodeImpl>::Create(graph()));
  }

  void AttachProcess() {
    // Create a process so this process node doesn't bail on Process.IsValid();
    process_node()->SetProcess(base::Process::Current(),
                               /* launch_time=*/base::TimeTicks::Now());
  }

  void TearDown() override {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      // Also skip TearDown() if SetUp() was skipped.
      return;
    }
    base::RunLoop().RunUntilIdle();

    policy_ = nullptr;
    frame_node_.reset();
    page_node_.reset();
    process_node_.reset();
    system_node_.reset();
    graph_->TearDown();
    graph_ = nullptr;
  }

  void CreateAndPassMockPolicy() {
    // Add our mock policy to the graph.
    std::unique_ptr<StrictMock<MockUserspaceSwapPolicy>> mock_policy(
        new StrictMock<MockUserspaceSwapPolicy>);
    policy_ = mock_policy.get();
    graph()->PassToGraph(std::move(mock_policy));
  }

  MockUserspaceSwapPolicy* policy() { return policy_; }

  // "borrowed" helper methods from the GraphTestHarness.
  template <class NodeClass, typename... Args>
  TestNodeWrapper<NodeClass> CreateNode(Args&&... args) {
    return TestNodeWrapper<NodeClass>::Create(graph(),
                                              std::forward<Args>(args)...);
  }

  TestGraphImpl* graph() { return graph_.get(); }
  content::BrowserTaskEnvironment* browser_env() { return &browser_env_; }
  TestNodeWrapper<ProcessNodeImpl>& process_node() { return process_node_; }
  TestNodeWrapper<PageNodeImpl>& page_node() { return page_node_; }
  TestNodeWrapper<FrameNodeImpl>& frame_node() { return frame_node_; }
  TestNodeWrapper<SystemNodeImpl>& system_node() {
    return *(system_node_.get());
  }

  void FastForwardBy(base::TimeDelta delta) {
    browser_env()->FastForwardBy(delta);
  }

  void RunUntilIdle() { browser_env()->RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment browser_env_;
  std::unique_ptr<TestGraphImpl> graph_;
  raw_ptr<MockUserspaceSwapPolicy> policy_ = nullptr;  // Not owned.

  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> frame_node_;
  std::unique_ptr<TestNodeWrapper<SystemNodeImpl>> system_node_;
};

// This test validates that we only initialize a ProcessNode once.
TEST_F(UserspaceSwapPolicyTest, ValidateInitializeProcessOnlyOnce) {
  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get())).Times(1);

  // Attaching is a lifecycle state change.
  AttachProcess();

  // And now force another life cycle state change.
  process_node()->SetProcessExitStatus(0);
}

// This test validates that we only walk the graph under moderate pressure.
TEST_F(UserspaceSwapPolicyTest, ValidateGraphWalkFrequencyNoPressure) {
  auto last_walk_time = base::TimeTicks::Now();
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().swap_on_moderate_pressure = true;
  policy()->set_last_graph_walk(last_walk_time);

  // SwapNodesOnGraph shouldn't be called.
  EXPECT_CALL(*policy(), SwapNodesOnGraph()).Times(0);

  // We will fast forward by 100 graph walk frequencies, but since we're not
  // under pressure we will expect no calls.
  FastForwardBy(100 * policy()->config().graph_walk_frequency);

  // Confirm through the last_graph_walk time that we didn't walk.
  ASSERT_EQ(last_walk_time, policy()->get_last_graph_walk());
}

// This test validates that we only call WalkGraph every graph walk frequency
// seconds when under moderate pressure.
TEST_F(UserspaceSwapPolicyTest, ValidateGraphWalkFrequencyModeratePressure) {
  policy()->config().graph_walk_frequency = base::Seconds(60);

  // We expect that we will call SwapNodesOnGraph only 2 times.
  EXPECT_CALL(*policy(), SwapNodesOnGraph()).Times(2);

  // Triger memory pressure and we should observe the walk since we've never
  // walked before.
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  auto initial_walk_time = base::TimeTicks::Now();
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(initial_walk_time, policy()->get_last_graph_walk());

  // We will fast forward less than the graph walk frequency and confirm we
  // don't walk again even when we receive another moderate pressure
  // notification.
  FastForwardBy(base::Seconds(1));
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // Since it's been less than the graph walk frequency we don't expect to walk.
  ASSERT_EQ(initial_walk_time, policy()->get_last_graph_walk());

  // Finally we will advance by a graph walk frequency and confirm we walk
  // again.
  FastForwardBy(policy()->config().graph_walk_frequency);
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  FastForwardBy(base::Seconds(1));
  ASSERT_NE(initial_walk_time, policy()->get_last_graph_walk());
}

// Validate we don't swap when not eligible.
TEST_F(UserspaceSwapPolicyTest, OnlySwapWhenEligibleToSwap) {
  policy()->config().graph_walk_frequency = base::Seconds(60);

  // Dispatch to the default swap nodes on graph implementation.
  EXPECT_CALL(*policy(), SwapNodesOnGraph())
      .WillOnce(
          Invoke(policy(), &MockUserspaceSwapPolicy::DefaultSwapNodesOnGraph));

  // We will say this node is not eligible to swap.
  EXPECT_CALL(*policy(),
              IsEligibleToSwap(process_node().get(), page_node().get()))
      .WillOnce(Return(false));

  // And we will expect that SwapProcessNode is NOT called.
  EXPECT_CALL(*policy(), SwapProcessNode(process_node().get())).Times(0);

  // Trigger moderate memory pressure to start the graph walk.
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  FastForwardBy(base::Seconds(1));
}

TEST_F(UserspaceSwapPolicyTest, OnlySwapWhenEligibleToSwapTrue) {
  // Dispatch to the default swap nodes on graph implementation.
  EXPECT_CALL(*policy(), SwapNodesOnGraph())
      .WillOnce(
          Invoke(policy(), &MockUserspaceSwapPolicy::DefaultSwapNodesOnGraph));

  // We will say this node is eligible to swap.
  EXPECT_CALL(*policy(),
              IsEligibleToSwap(process_node().get(), page_node().get()))
      .WillOnce(Return(true));

  // And we will expect that SwapProcessNode is called.
  EXPECT_CALL(*policy(), SwapProcessNode(process_node().get())).Times(1);

  // Trigger moderate memory pressure to start the graph walk.
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  FastForwardBy(base::Seconds(1));
}

// This test validates that we won't swap a node when it's visible.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenVisible) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  EXPECT_CALL(*policy(), IsPageNodeLoading(page_node().get()))
      .WillOnce(Return(false));

  // Because this page node is visible it should not be eligible for swap.
  EXPECT_CALL(*policy(), IsPageNodeVisible(page_node().get()))
      .WillOnce(Return(true));

  EXPECT_FALSE(policy()->DefaultIsEligibleToSwap(process_node().get(),
                                                 page_node().get()));
}

// This test validates that we won't swap a node when it's audible.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenAudible) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  EXPECT_CALL(*policy(), IsPageNodeLoading(page_node().get()))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy(), IsPageNodeVisible(page_node().get()))
      .WillOnce(Return(false));

  // Because this page node is audible it won't be swappable.
  EXPECT_CALL(*policy(), IsPageNodeAudible(page_node().get()))
      .WillOnce(Return(true));

  EXPECT_FALSE(policy()->DefaultIsEligibleToSwap(process_node().get(),
                                                 page_node().get()));
}

// This test validates that we won't swap a node when it's loading.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenLoading) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  // Because this page node is loading it should not be eligible for swap.
  EXPECT_CALL(*policy(), IsPageNodeLoading(page_node().get()))
      .WillOnce(Return(true));

  EXPECT_FALSE(policy()->DefaultIsEligibleToSwap(process_node().get(),
                                                 page_node().get()));
}

// This test validates that we do not swap an individual process node more than
// the configuration allows.
TEST_F(UserspaceSwapPolicyTest, ValidateProcessSwapFrequency) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency =
      3 * policy()->config().graph_walk_frequency;

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  // Make sure this node is not visible, audible, or loading, and has been
  // invisible for a long time because this test isn't validating those things.
  EXPECT_CALL(*policy(), IsPageNodeAudible(page_node().get()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*policy(), IsPageNodeLoading(page_node().get()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*policy(), IsPageNodeVisible(page_node().get()))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*policy(), GetTimeSinceLastVisibilityChange(page_node().get()))
      .WillRepeatedly(Return(base::TimeDelta::Max()));

  EXPECT_CALL(*policy(), SwapNodesOnGraph())
      .WillRepeatedly(
          Invoke(policy(), &MockUserspaceSwapPolicy::DefaultSwapNodesOnGraph));

  // Invoke the standard IsEligibleToSwapChecks.
  EXPECT_CALL(*policy(),
              IsEligibleToSwap(process_node().get(), page_node().get()))
      .WillRepeatedly(
          Invoke(policy(), &MockUserspaceSwapPolicy::DefaultIsEligibleToSwap));

  // And although we will repeatedly walk the graph, we should only attempt to
  // swap this process node exactly one time.
  EXPECT_CALL(*policy(), SwapProcessNode(process_node().get())).Times(1);

  // We will walk the graph 3 times but this should only result in a single
  // swap.
  for (int i = 0; i < 3; ++i) {
    FastForwardBy(policy()->config().graph_walk_frequency);
    system_node()->OnMemoryPressureForTesting(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  }
}

// This test validates that we won't swap a node when the available swap space
// is below the limit.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenDiskSpaceTooLow) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  policy()->config().minimum_swap_disk_space_available = 1 << 30;  // 1 GB

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  // We will have our mock return that there is only 100MB of disk space
  // available. This should prevent swapping because it's below the minimum
  // value.
  EXPECT_CALL(*policy(), GetSwapDeviceFreeSpaceBytes())
      .WillOnce(Return(100 << 20));  // 100MB

  // Because GetSwapDeviceFreeSpaceBytes is less than the configured minimum it
  // should return false.
  EXPECT_FALSE(
      policy()->DefaultIsEligibleToSwap(process_node().get(), nullptr));
}

// This test will validate that we won't swap a renderer that is already
// exceeding the individual renderer limit.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenPerRendererSwapExceeded) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  policy()->config().renderer_maximum_disk_swap_file_size_bytes =
      128 << 20;  // 128MB

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  // We will have our mock return that there is only 100MB of disk space
  // available. This should prevent swapping because it's below the minimum
  // value.
  EXPECT_CALL(*policy(), GetProcessNodeSwapFileUsageBytes(process_node().get()))
      .WillOnce(Return(190 << 20));  // 190 MB

  // We're already using 190 MB which is more than the configured 128 MB so it
  // should not be eligible to swap.
  EXPECT_FALSE(
      policy()->DefaultIsEligibleToSwap(process_node().get(), nullptr));
}

// This test will validate that we don't swap renderers when we've exceeded the
// global limit.
TEST_F(UserspaceSwapPolicyTest, DontSwapWhenTotalRendererSwapExceeded) {
  // We will only swap a renderer once every 3 graph walks.
  policy()->config().graph_walk_frequency = base::Seconds(1);
  policy()->config().process_swap_frequency = base::Seconds(1);

  policy()->config().maximum_swap_disk_space_bytes = 1 << 30;  // 1 GB

  EXPECT_CALL(*policy(), InitializeProcessNode(process_node().get()))
      .WillOnce(Return(true));
  AttachProcess();

  // We will have our mock return that there is only 100MB of disk space
  // available. This should prevent swapping because it's below the minimum
  // value.
  EXPECT_CALL(*policy(), GetTotalSwapFileUsageBytes())
      .WillOnce(Return(500 << 20))    // 500 MB
      .WillOnce(Return(1200 << 20));  // 1.2 GB
  // Since we're now below the 1GB limit we expect it will succeed.
  EXPECT_TRUE(policy()->DefaultIsEligibleToSwap(process_node().get(), nullptr));

  // And on the second call we will expect that we will no longer be eligible
  // because we've exceeded 1GB.
  EXPECT_FALSE(
      policy()->DefaultIsEligibleToSwap(process_node().get(), nullptr));
}

}  // namespace
}  // namespace policies
}  // namespace performance_manager
