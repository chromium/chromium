// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/common/chrome_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using ::testing::Return;

class MockDataProvider : public GlicInstanceCoordinatorMetrics::DataProvider {
 public:
  MOCK_METHOD(
      std::vector<
          GlicInstanceCoordinatorMetrics::DataProvider::InstanceWebContents>,
      GetAllUnhibernatedWebContents,
      (),
      (override));
  MOCK_METHOD(int, GetVisibleInstanceCount, (), (const, override));
  MOCK_METHOD(std::vector<glic::mojom::ConversationInfoPtr>,
              GetRecentlyActiveConversations,
              (size_t),
              (override));
  MOCK_METHOD(std::vector<GlicInstanceImpl*>, GetInstances, (), (override));
  MOCK_METHOD(Profile*, GetProfile, (), (const, override));
};

class GlicInstanceCoordinatorMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    metrics_ = std::make_unique<GlicInstanceCoordinatorMetrics>(&provider_);
  }

 protected:
  std::unique_ptr<MockGlicInstance> CreateMockInstance(
      const std::optional<std::string>& conversation_id) {
    auto instance = std::make_unique<MockGlicInstance>();
    EXPECT_CALL(*instance, conversation_id())
        .WillRepeatedly(testing::Return(conversation_id));
    return instance;
  }

  std::vector<glic::mojom::ConversationInfoPtr> CreateRecentConversationList(
      const std::vector<std::string>& ids) {
    std::vector<glic::mojom::ConversationInfoPtr> result;
    for (const auto& id : ids) {
      auto info = glic::mojom::ConversationInfo::New();
      info->conversation_id = id;
      result.push_back(std::move(info));
    }
    return result;
  }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  testing::NiceMock<MockDataProvider> provider_;
  std::unique_ptr<GlicInstanceCoordinatorMetrics> metrics_;
};

TEST_F(GlicInstanceCoordinatorMetricsTest,
       NoConcurrentVisibility_ZeroInstances) {
  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillRepeatedly(testing::Return(0));
  metrics_->OnInstanceVisibilityChanged();
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.PeakCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.ConcurrentVisibility.PeakCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.ConcurrentVisibility.PeakCount", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, NoConcurrentVisibility_OneInstance) {
  MockGlicInstance instance1;
  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillRepeatedly(testing::Return(1));

  metrics_->OnInstanceVisibilityChanged();
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.PeakCount", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, ConcurrentVisibility_TwoInstances) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillOnce(testing::Return(2));

  // Start concurrent visibility
  metrics_->OnInstanceVisibilityChanged();

  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillOnce(testing::Return(1));
  metrics_->OnInstanceVisibilityChanged();

  histogram_tester_.ExpectUniqueTimeSample("Glic.ConcurrentVisibility.Duration",
                                           base::Seconds(10), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.ConcurrentVisibility.Duration", base::Seconds(10), 1);
  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ConcurrentVisibility.PeakCount", 2, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, ConcurrentVisibility_PeakCount) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  MockGlicInstance instance3;

  // Start with 2 visible
  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillOnce(testing::Return(2))
      .WillOnce(testing::Return(3))
      .WillOnce(testing::Return(2))
      .WillOnce(testing::Return(1));
  metrics_->OnInstanceVisibilityChanged();

  // Increase to 3 visible

  metrics_->OnInstanceVisibilityChanged();

  // Decrease back to 2 visible

  metrics_->OnInstanceVisibilityChanged();

  // End concurrent visibility (go to 1 visible)

  metrics_->OnInstanceVisibilityChanged();

  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 3,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ConcurrentVisibility.PeakCount", 3, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       ConcurrentVisibility_DestructorEndsPeriod) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  EXPECT_CALL(provider_, GetVisibleInstanceCount())
      .WillRepeatedly(testing::Return(2));

  // Start concurrent visibility
  metrics_->OnInstanceVisibilityChanged();
  task_environment_.FastForwardBy(base::Seconds(5));

  // Destroy metrics object while period is active
  metrics_.reset();

  histogram_tester_.ExpectUniqueTimeSample("Glic.ConcurrentVisibility.Duration",
                                           base::Seconds(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.ConcurrentVisibility.Duration", base::Seconds(5), 1);
  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.ConcurrentVisibility.PeakCount", 2, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_New) {
  auto instance1 = CreateMockInstance("A");

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"A"}))));

  auto instanceNew = CreateMockInstance(std::nullopt);

  // instance1 is active
  metrics_->RecordSwitchConversationTarget(
      std::nullopt, instanceNew->conversation_id(), instance1.get());
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_Existing) {
  auto instance1 = CreateMockInstance("A");
  auto instance2 = CreateMockInstance("B");
  auto instance3 = CreateMockInstance("C");

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"C", "B"}))));

  // instance3 ("C") is more recent than instance2 ("B").
  // So last active (excluding instance1) is "C".
  // We request "B".
  metrics_->RecordSwitchConversationTarget("B", instance2->conversation_id(),
                                           instance1.get());
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       SwitchConversation_OtherExistingNoInstance) {
  auto instance1 = CreateMockInstance("A");
  auto instance2 = CreateMockInstance(std::nullopt);

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"A"}))));

  metrics_->RecordSwitchConversationTarget("B", instance2->conversation_id(),
                                           instance1.get());
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_Resume) {
  auto instance1 = CreateMockInstance("A");
  auto instanceNew = CreateMockInstance(std::nullopt);

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"A"}))));

  // 1. Initial state A (simulated by timestamps)
  // 2. User clicks "New Conversation".
  metrics_->RecordSwitchConversationTarget(
      std::nullopt, instanceNew->conversation_id(), instance1.get());
  histogram_tester_.ExpectBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);

  // 3. User is now on "New". Activation changed (simulated by making New more
  // recent).

  // 4. User clicks back to "A".
  metrics_->RecordSwitchConversationTarget("A", instance1->conversation_id(),
                                           instanceNew.get());
  histogram_tester_.ExpectBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_NewToOther) {
  auto instance1 = CreateMockInstance("A");
  auto instanceNew = CreateMockInstance(std::nullopt);
  auto instance2 = CreateMockInstance("B");

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"A", "B"}))));

  // Switching from New (active) to B (target).
  // Most recent (excluding New) is A (instance1).
  // Requested B != A.
  metrics_->RecordSwitchConversationTarget("B", instance2->conversation_id(),
                                           instanceNew.get());
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       SwitchConversation_InstanceSwitchResume) {
  auto instance1 = CreateMockInstance("A");
  auto instance2 = CreateMockInstance("B");

  EXPECT_CALL(provider_, GetRecentlyActiveConversations(2))
      .WillRepeatedly(testing::Return(
          testing::ByMove(CreateRecentConversationList({"B", "A"}))));

  // User switches conversation back to A.
  metrics_->RecordSwitchConversationTarget("A", instance1->conversation_id(),
                                           instance2.get());
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, PeakCountOnShutdown_ZeroInstances) {
  metrics_->OnShutdown();
  histogram_tester_.ExpectTotalCount("Glic.Instances.PeakCount.OnShutdown", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instances.PeakCount.OnShutdown", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instances.PeakCount.OnShutdown", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PeakCountOnShutdown_HighWaterMarkAndIdempotent) {
  std::vector<GlicInstanceImpl*> one_instance(1, nullptr);
  std::vector<GlicInstanceImpl*> three_instances(3, nullptr);

  // 1. One instance created.
  EXPECT_CALL(provider_, GetInstances())
      .WillOnce(testing::Return(one_instance));
  metrics_->RecordCountOnCreation();

  // 2. Scaled up to three instances.
  EXPECT_CALL(provider_, GetInstances())
      .WillOnce(testing::Return(three_instances));
  metrics_->RecordCountOnCreation();

  // 3. Coordinator shuts down after instances were removed. Peak should be 3.
  metrics_->OnShutdown();
  histogram_tester_.ExpectUniqueSample("Glic.Instances.PeakCount.OnShutdown", 3,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Instances.PeakCount.OnShutdown", 3, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instances.PeakCount.OnShutdown", 0);

  // 4. Subsequent OnShutdown() call should be idempotent and not double-emit.
  metrics_->OnShutdown();
  histogram_tester_.ExpectTotalCount("Glic.Instances.PeakCount.OnShutdown", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instances.PeakCount.OnShutdown", 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PeakCountOnShutdown_ClampsOverflowToMaxBucket) {
  std::vector<GlicInstanceImpl*> sixty_instances(60, nullptr);
  EXPECT_CALL(provider_, GetInstances())
      .WillOnce(testing::Return(sixty_instances));
  metrics_->RecordCountOnCreation();

  metrics_->OnShutdown();
  // kMaxInstanceCountExclusive is 51, so samples >= 51 land in overflow bucket
  // 51.
  histogram_tester_.ExpectUniqueSample("Glic.Instances.PeakCount.OnShutdown",
                                       51, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Instances.PeakCount.OnShutdown", 51, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PostShutdown_SuppressesCreationAndPeriodicMetrics) {
  std::vector<GlicInstanceImpl*> two_instances(2, nullptr);
  EXPECT_CALL(provider_, GetInstances())
      .WillOnce(testing::Return(two_instances));

  metrics_->StartPeriodicMetricsRecording();
  metrics_->RecordCountOnCreation();
  metrics_->OnShutdown();

  histogram_tester_.ExpectUniqueSample("Glic.Instances.Count.OnCreation", 2, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instances.PeakCount.OnShutdown", 2,
                                       1);

  // Subsequent creation calls, timer restarts, and elapsed 10m ticks after
  // OnShutdown must not emit additional samples.
  EXPECT_CALL(provider_, GetInstances()).Times(0);
  metrics_->RecordCountOnCreation();
  metrics_->StartPeriodicMetricsRecording();
  task_environment_.FastForwardBy(base::Minutes(10));

  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.OnCreation", 1);
  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.Periodic", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instances.PeakCount.OnShutdown", 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PeriodicInstanceCount_SkipsZeroAndIncludesHibernated) {
  metrics_->StartPeriodicMetricsRecording();

  // Zero instances: timer fires after 10 minutes, but nothing is recorded.
  EXPECT_CALL(provider_, GetInstances())
      .WillOnce(testing::Return(std::vector<GlicInstanceImpl*>{}));
  task_environment_.FastForwardBy(base::Minutes(10));
  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.Periodic", 0);
  histogram_tester_.ExpectTotalCount("Glic.Consumer.Instances.Count.Periodic",
                                     0);
  testing::Mock::VerifyAndClearExpectations(&provider_);

  // Two instances exist (hibernated): timer fires after 10 minutes, records
  // count 2.
  std::vector<GlicInstanceImpl*> two_instances(2, nullptr);
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(testing::Return(two_instances));
  task_environment_.FastForwardBy(base::Minutes(10));
  histogram_tester_.ExpectUniqueSample("Glic.Instances.Count.Periodic", 2, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Consumer.Instances.Count.Periodic",
                                       2, 1);

  // Another 10 minutes: records count 2 again.
  task_environment_.FastForwardBy(base::Minutes(10));
  histogram_tester_.ExpectBucketCount("Glic.Instances.Count.Periodic", 2, 2);
  histogram_tester_.ExpectBucketCount("Glic.Consumer.Instances.Count.Periodic",
                                      2, 2);
  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.Periodic", 2);

  // Destroying metrics flushes peak count observed via periodic ticks.
  metrics_.reset();
  histogram_tester_.ExpectUniqueSample("Glic.Instances.PeakCount.OnShutdown", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Consumer.Instances.PeakCount.OnShutdown", 2, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PeriodicInstanceCount_KillswitchDisablesTimer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kGlicRecordPeriodicInstanceCountMetrics);

  EXPECT_CALL(provider_, GetInstances()).Times(0);
  metrics_->StartPeriodicMetricsRecording();
  task_environment_.FastForwardBy(base::Minutes(20));

  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.Periodic", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       PeriodicInstanceCount_PeriodIndependentFromMemoryMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kGlicRecordMemoryFootprintMetrics,
                             {{"period", "5m"}}}},
      /*disabled_features=*/{});

  std::vector<GlicInstanceImpl*> two_instances(2, nullptr);
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(testing::Return(two_instances));

  metrics_->StartPeriodicMetricsRecording();

  // At 5 minutes, memory metrics period elapses, but periodic instance count
  // (hardcoded 10m) must not fire yet.
  task_environment_.FastForwardBy(base::Minutes(5));
  histogram_tester_.ExpectTotalCount("Glic.Instances.Count.Periodic", 0);

  // At 10 minutes total, periodic instance count fires once.
  task_environment_.FastForwardBy(base::Minutes(5));
  histogram_tester_.ExpectUniqueSample("Glic.Instances.Count.Periodic", 2, 1);
}

}  // namespace
}  // namespace glic
