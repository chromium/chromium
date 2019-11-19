// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/dynamic_tcmalloc_policy_linux.h"

#include "base/allocator/buildflags.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
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

// We can only run our unit tests when built with TCMALLOC.
#if BUILDFLAG(USE_TCMALLOC)

namespace {
// The following are the constants used in the tcmalloc policy.
constexpr uint32_t kMinThreadCacheSize = (4 << 20);      // 4MB
constexpr uint32_t kMaxThreadCacheSize = (4 << 20) * 8;  // 32MB

}  // namespace

using testing::Invoke;
using testing::Return;

class MockDynamicTcmallocPolicy : public DynamicTcmallocPolicy {
 public:
  MockDynamicTcmallocPolicy() {}
  ~MockDynamicTcmallocPolicy() override {}

  MOCK_METHOD0(CheckAndUpdateTunables, void(void));
  MOCK_METHOD0(CalculateFreeMemoryRatio, float(void));
  MOCK_METHOD1(
      EnsureTcmallocTunablesForProcess,
      mojo::Remote<tcmalloc::mojom::TcmallocTunables>*(const ProcessNode*));

  // Allow our mock to dispatch to the original implementation.
  void DefaultCheckAndUpdateTunables() {
    DynamicTcmallocPolicy::CheckAndUpdateTunables();
  }

  float DefaultCalculateFreeMemoryRatio() {
    return DynamicTcmallocPolicy::CalculateFreeMemoryRatio();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDynamicTcmallocPolicy);
};

class MockTcmallocTunablesImpl : public tcmalloc::mojom::TcmallocTunables {
 public:
  explicit MockTcmallocTunablesImpl(
      mojo::PendingReceiver<tcmalloc::mojom::TcmallocTunables> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockTcmallocTunablesImpl() override {}

  MOCK_METHOD1(SetMaxTotalThreadCacheBytes, void(uint32_t));

 private:
  mojo::Receiver<tcmalloc::mojom::TcmallocTunables> receiver_{this};
  DISALLOW_COPY_AND_ASSIGN(MockTcmallocTunablesImpl);
};

class DynamicTcmallocPolicyTest : public ::testing::Test {
 public:
  DynamicTcmallocPolicyTest()
      : browser_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DynamicTcmallocPolicyTest() override {}

  void SetUp() override {
    CreateAndPassMockPolicy();

    // Create a simple graph.
    process_node_ = CreateNode<ProcessNodeImpl>();
    page_node_ = CreateNode<PageNodeImpl>();
    frame_node_ =
        graph()->CreateFrameNodeAutoId(process_node().get(), page_node().get());

    // Create a Process so this process node doesn't bail on Process.IsValid();
    const base::Process self = base::Process::Current();
    auto duplicate = self.Duplicate();
    ASSERT_TRUE(duplicate.IsValid());
    process_node()->SetProcess(std::move(duplicate), base::Time::Now());
  }

  void TearDown() override {
    policy_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void CreateAndPassMockPolicy() {
    // Add our mock policy to the graph.
    std::unique_ptr<MockDynamicTcmallocPolicy> mock_policy(
        new MockDynamicTcmallocPolicy);
    policy_ = mock_policy.get();
    graph()->PassToGraph(std::move(mock_policy));
  }

  MockDynamicTcmallocPolicy* policy() { return policy_; }

  // "borrowed" helper methods from the GraphTestHarness.
  template <class NodeClass, typename... Args>
  TestNodeWrapper<NodeClass> CreateNode(Args&&... args) {
    return TestNodeWrapper<NodeClass>::Create(graph(),
                                              std::forward<Args>(args)...);
  }

  TestGraphImpl* graph() { return &graph_; }
  content::BrowserTaskEnvironment* browser_env() { return &browser_env_; }
  TestNodeWrapper<ProcessNodeImpl>& process_node() { return process_node_; }
  TestNodeWrapper<PageNodeImpl>& page_node() { return page_node_; }
  TestNodeWrapper<FrameNodeImpl>& frame_node() { return frame_node_; }

  void FastForwardBy(base::TimeDelta delta) {
    browser_env()->FastForwardBy(delta);
  }

 private:
  content::BrowserTaskEnvironment browser_env_;
  TestGraphImpl graph_;
  MockDynamicTcmallocPolicy* policy_ = nullptr;  // Not owned.

  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> frame_node_;

  DISALLOW_COPY_AND_ASSIGN(DynamicTcmallocPolicyTest);
};

TEST_F(DynamicTcmallocPolicyTest, PeriodicPressureCheck) {
  // Advance through two intervals to confirm that we see our periodic checks.
  EXPECT_CALL(*policy(), CheckAndUpdateTunables()).Times(2);
  FastForwardBy(base::TimeDelta::FromSeconds(
      2 * features::kDynamicTuningTimeSec.Get() + 1));
}

// This test will validate that for a mocked value of memory available we see a
// mojo message with a corresponding value.
TEST_F(DynamicTcmallocPolicyTest, DynamicallyAdjustThreadCacheSize) {
  // Dispatch to the default implementation so that we can test with a mocked
  // CalculateFreeMemoryRatio().
  EXPECT_CALL(*policy(), CheckAndUpdateTunables())
      .WillOnce(Invoke(
          policy(), &MockDynamicTcmallocPolicy::DefaultCheckAndUpdateTunables));

  // We will report 100% memory available so we expect to see the max value.
  constexpr float kMemAvail = 1.0;
  constexpr uint32_t kAbsoluteThreadCacheSize = kMaxThreadCacheSize * kMemAvail;
  EXPECT_CALL(*policy(), CalculateFreeMemoryRatio())
      .WillOnce(Return(kMemAvail));

  mojo::Remote<tcmalloc::mojom::TcmallocTunables> tunables;
  EXPECT_CALL(*policy(), EnsureTcmallocTunablesForProcess(process_node().get()))
      .WillOnce(Return(&tunables));

  MockTcmallocTunablesImpl mock_tcmalloc_impl(
      tunables.BindNewPipeAndPassReceiver());

  EXPECT_CALL(mock_tcmalloc_impl,
              SetMaxTotalThreadCacheBytes(kAbsoluteThreadCacheSize))
      .Times(1);

  // Advancing beyond our interval will cause a periodic pressure check.
  FastForwardBy(
      base::TimeDelta::FromSeconds(features::kDynamicTuningTimeSec.Get() + 1));
}

// This test validates that process nodes with no frame nodes are skipped.
TEST_F(DynamicTcmallocPolicyTest, SkipProcessNodesWithNoFrameNodes) {
  EXPECT_CALL(*policy(), CheckAndUpdateTunables())
      .WillOnce(Invoke(
          policy(), &MockDynamicTcmallocPolicy::DefaultCheckAndUpdateTunables));

  // Blow away the frame node associated with this process node, this will cause
  // this process node to be skipped.
  frame_node().reset();
  page_node().reset();

  // The value here is irrelevant because we should never update this process
  // node as it has no frame nodes.
  EXPECT_CALL(*policy(), CalculateFreeMemoryRatio()).WillOnce(Return(0.0));

  // We should not try to get the TcmallocTunables as it's not needed since we
  // didn't have frame nodes.
  EXPECT_CALL(*policy(), EnsureTcmallocTunablesForProcess(testing::_)).Times(0);

  // Advancing beyond our interval will cause a periodic pressure check.
  FastForwardBy(
      base::TimeDelta::FromSeconds(features::kDynamicTuningTimeSec.Get() + 1));
}

// This test will validate that the minimum size is enforced and we won't
// attempt to shrink below the min.
TEST_F(DynamicTcmallocPolicyTest,
       DynamicallyAdjustThreadCacheSizeDoesnGoBelowMin) {
  // Dispatch to the default implementation so that we can test with a mocked
  // CalculateFreeMemoryRatio().
  EXPECT_CALL(*policy(), CheckAndUpdateTunables())
      .WillOnce(Invoke(
          policy(), &MockDynamicTcmallocPolicy::DefaultCheckAndUpdateTunables));

  // We will report 0% memory available so we expect to see the min value, it
  // should never fall below this value.
  constexpr float kMemAvail = 0.0;
  constexpr uint32_t kExpectedThreadCacheSize = kMinThreadCacheSize;
  EXPECT_CALL(*policy(), CalculateFreeMemoryRatio())
      .WillOnce(Return(kMemAvail));

  mojo::Remote<tcmalloc::mojom::TcmallocTunables> tunables;
  EXPECT_CALL(*policy(), EnsureTcmallocTunablesForProcess(process_node().get()))
      .WillOnce(Return(&tunables));

  MockTcmallocTunablesImpl mock_tcmalloc_impl(
      tunables.BindNewPipeAndPassReceiver());

  EXPECT_CALL(mock_tcmalloc_impl,
              SetMaxTotalThreadCacheBytes(kExpectedThreadCacheSize))
      .Times(1);

  // Advancing beyond our interval will cause a periodic pressure check.
  FastForwardBy(
      base::TimeDelta::FromSeconds(features::kDynamicTuningTimeSec.Get() + 1));
}

// This test will validate that we apply the invisible scale factor only when
// the frame has been invisible for longer than the cutoff time.
TEST_F(DynamicTcmallocPolicyTest, OnlyApplyInvisibleScaleFactorAfterCutoff) {
  // Switch our experimental state so we can test certain behavior.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kDynamicTcmallocTuning,
      {{"DynamicTcmallocScaleInvisibleTimeSec", "240"},
       {"DynamicTcmallocTuneTimeSec", "120"}});

  const int dynamic_tuning_time_sec = features::kDynamicTuningTimeSec.Get();
  const int scaling_invisible_time_sec =
      features::kDynamicTuningScaleInvisibleTimeSec.Get();

  // Validate we're using the params we expect.
  ASSERT_EQ(dynamic_tuning_time_sec, 120);
  ASSERT_EQ(scaling_invisible_time_sec, 240);

  EXPECT_CALL(*policy(), CheckAndUpdateTunables())
      .WillRepeatedly(Invoke(
          policy(), &MockDynamicTcmallocPolicy::DefaultCheckAndUpdateTunables));

  // Make sure that our policy returns our mock tunables.
  mojo::Remote<tcmalloc::mojom::TcmallocTunables> tunables;
  MockTcmallocTunablesImpl mock_tcmalloc_impl(
      tunables.BindNewPipeAndPassReceiver());
  EXPECT_CALL(*policy(), EnsureTcmallocTunablesForProcess(process_node().get()))
      .WillRepeatedly(Return(&tunables));

  // Let's start by making sure the frame is invisible we make sure to swap from
  // visible to invisible to ensure the state change time is updated to now.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // We will report 50% memory available each time we check.
  constexpr float kMemAvail = 0.50;
  EXPECT_CALL(*policy(), CalculateFreeMemoryRatio())
      .WillRepeatedly(Return(kMemAvail));

  // We're expecting that we do not add an invisible scale factor on our first
  // call.
  constexpr uint32_t kExpectedThreadCacheSize = kMaxThreadCacheSize * kMemAvail;
  EXPECT_CALL(mock_tcmalloc_impl,
              SetMaxTotalThreadCacheBytes(kExpectedThreadCacheSize))
      .Times(1);

  // Advancing beyond our interval will cause two periodic checks and the second
  // will result in the additional scaling because the page node is invisible.
  FastForwardBy(base::TimeDelta::FromSeconds(dynamic_tuning_time_sec + 1));

  // And we're expecting that we do apply it on our second call as it will have
  // moved us beyond the invisible time cutoff time.
  constexpr float kInvisibleScaleRatio = 0.75;
  constexpr uint32_t kExpectedThreadCacheSizeScaledInvisible =
      kExpectedThreadCacheSize * kInvisibleScaleRatio;
  EXPECT_CALL(mock_tcmalloc_impl, SetMaxTotalThreadCacheBytes(
                                      kExpectedThreadCacheSizeScaledInvisible))
      .Times(1);

  // This second advance will also cause us to pass the invisible scale cutoff
  // time, so we will expect the second call with a scaled value this time.
  FastForwardBy(base::TimeDelta::FromSeconds(dynamic_tuning_time_sec + 1));
}

#endif  // BUILDFLAG(USE_TCMALLOC)

}  // namespace policies
}  // namespace performance_manager
