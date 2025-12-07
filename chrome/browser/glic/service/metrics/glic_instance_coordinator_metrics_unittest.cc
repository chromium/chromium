// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using ::testing::Return;

class MockGlicInstance : public GlicInstance {
 public:
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(bool, IsActive, (), (override));
  MOCK_METHOD(bool, IsAttached, (), (override));
  MOCK_METHOD(void, AddStateObserver, (PanelStateObserver*), (override));
  MOCK_METHOD(void, RemoveStateObserver, (PanelStateObserver*), (override));
  MOCK_METHOD(mojom::PanelState, GetPanelState, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterStateChange,
              (StateChangeCallback),
              (override));
  MOCK_METHOD(Host&, host, (), (override));
  MOCK_METHOD(gfx::Size, GetPanelSize, (), (override));
  MOCK_METHOD(const InstanceId&, id, (), (const, override));
  MOCK_METHOD(std::optional<std::string>,
              conversation_id,
              (),
              (const, override));
  MOCK_METHOD(base::TimeTicks, GetLastActiveTime, (), (const, override));
  MOCK_METHOD(GlicInstanceMetrics*, instance_metrics, (), (override));
};

class MockDataProvider : public GlicInstanceCoordinatorMetrics::DataProvider {
 public:
  MOCK_METHOD(std::vector<GlicInstance*>, GetInstances, (), (override));
};

class GlicInstanceCoordinatorMetricsTest : public testing::Test {
 public:
  void SetUp() override {
    metrics_ = std::make_unique<GlicInstanceCoordinatorMetrics>(&provider_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  testing::NiceMock<MockDataProvider> provider_;
  std::unique_ptr<GlicInstanceCoordinatorMetrics> metrics_;
};

TEST_F(GlicInstanceCoordinatorMetricsTest,
       NoConcurrentVisibility_ZeroInstances) {
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(std::vector<GlicInstance*>{}));
  metrics_->OnInstanceVisibilityChanged();
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.PeakCount", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, NoConcurrentVisibility_OneInstance) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(std::vector<GlicInstance*>{&instance1}));

  metrics_->OnInstanceVisibilityChanged();
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.Duration", 0);
  histogram_tester_.ExpectTotalCount("Glic.ConcurrentVisibility.PeakCount", 0);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, ConcurrentVisibility_TwoInstances) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  EXPECT_CALL(instance1, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance2, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(
          Return(std::vector<GlicInstance*>{&instance1, &instance2}));

  // Start concurrent visibility
  metrics_->OnInstanceVisibilityChanged();

  task_environment_.FastForwardBy(base::Seconds(10));

  // End concurrent visibility by hiding one
  EXPECT_CALL(instance2, IsShowing()).WillRepeatedly(Return(false));
  metrics_->OnInstanceVisibilityChanged();

  histogram_tester_.ExpectUniqueTimeSample("Glic.ConcurrentVisibility.Duration",
                                           base::Seconds(10), 1);
  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 2,
                                       1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, ConcurrentVisibility_PeakCount) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  MockGlicInstance instance3;

  // Start with 2 visible
  EXPECT_CALL(instance1, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance2, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance3, IsShowing()).WillRepeatedly(Return(false));
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(
          std::vector<GlicInstance*>{&instance1, &instance2, &instance3}));
  metrics_->OnInstanceVisibilityChanged();

  // Increase to 3 visible
  EXPECT_CALL(instance3, IsShowing()).WillRepeatedly(Return(true));
  metrics_->OnInstanceVisibilityChanged();

  // Decrease back to 2 visible
  EXPECT_CALL(instance3, IsShowing()).WillRepeatedly(Return(false));
  metrics_->OnInstanceVisibilityChanged();

  // End concurrent visibility (go to 1 visible)
  EXPECT_CALL(instance2, IsShowing()).WillRepeatedly(Return(false));
  metrics_->OnInstanceVisibilityChanged();

  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 3,
                                       1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       ConcurrentVisibility_DestructorEndsPeriod) {
  MockGlicInstance instance1;
  MockGlicInstance instance2;
  EXPECT_CALL(instance1, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(instance2, IsShowing()).WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(
          Return(std::vector<GlicInstance*>{&instance1, &instance2}));

  // Start concurrent visibility
  metrics_->OnInstanceVisibilityChanged();
  task_environment_.FastForwardBy(base::Seconds(5));

  // Destroy metrics object while period is active
  metrics_.reset();

  histogram_tester_.ExpectUniqueTimeSample("Glic.ConcurrentVisibility.Duration",
                                           base::Seconds(5), 1);
  histogram_tester_.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 2,
                                       1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_New) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now()));

  // Setup DataProvider to return instance1
  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(std::vector<GlicInstance*>{&instance1}));

  MockGlicInstance instanceNew;
  EXPECT_CALL(instanceNew, conversation_id())
      .WillRepeatedly(Return(std::nullopt));

  // instance1 is active
  metrics_->RecordSwitchConversationTarget(
      std::nullopt, instanceNew.conversation_id(), &instance1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_Existing) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now()));

  MockGlicInstance instance2;
  EXPECT_CALL(instance2, conversation_id())
      .WillRepeatedly(Return(std::string("B")));
  EXPECT_CALL(instance2, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(10)));

  MockGlicInstance instance3;
  EXPECT_CALL(instance3, conversation_id())
      .WillRepeatedly(Return(std::string("C")));
  EXPECT_CALL(instance3, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(5)));

  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(
          std::vector<GlicInstance*>{&instance1, &instance2, &instance3}));

  // instance3 ("C") is more recent than instance2 ("B").
  // So last active (excluding instance1) is "C".
  // We request "B".
  metrics_->RecordSwitchConversationTarget("B", instance2.conversation_id(),
                                           &instance1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       SwitchConversation_OtherExistingNoInstance) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now()));

  MockGlicInstance instance2;
  // Instance 2 does not yet have the conversation B.
  EXPECT_CALL(instance2, conversation_id())
      .WillRepeatedly(Return(std::nullopt));

  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(
          Return(std::vector<GlicInstance*>{&instance1, &instance2}));

  metrics_->RecordSwitchConversationTarget("B", instance2.conversation_id(),
                                           &instance1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_Resume) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(10)));

  MockGlicInstance instanceNew;
  EXPECT_CALL(instanceNew, conversation_id())
      .WillRepeatedly(Return(std::nullopt));

  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(
          Return(std::vector<GlicInstance*>{&instance1, &instanceNew}));

  // 1. Initial state A (simulated by timestamps)
  // 2. User clicks "New Conversation".
  metrics_->RecordSwitchConversationTarget(
      std::nullopt, instanceNew.conversation_id(), &instance1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kStartNewConversation, 1);

  // 3. User is now on "New". Activation changed (simulated by making New more
  // recent).
  EXPECT_CALL(instanceNew, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now()));

  // 4. User clicks back to "A".
  metrics_->RecordSwitchConversationTarget("A", instance1.conversation_id(),
                                           &instanceNew);
  histogram_tester_.ExpectBucketCount(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest, SwitchConversation_NewToOther) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  // Make instance1 ("A") the most recent one.
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(5)));

  MockGlicInstance instanceNew;
  EXPECT_CALL(instanceNew, conversation_id())
      .WillRepeatedly(Return(std::nullopt));
  // instanceNew is the current active one (implicitly, as we pass it to the
  // method).

  MockGlicInstance instance2;
  EXPECT_CALL(instance2, conversation_id())
      .WillRepeatedly(Return(std::string("B")));
  // instance2 ("B") is older than instance1.
  EXPECT_CALL(instance2, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(10)));

  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(Return(
          std::vector<GlicInstance*>{&instance1, &instanceNew, &instance2}));

  // Switching from New (active) to B (target).
  // Most recent (excluding New) is A (instance1).
  // Requested B != A.
  metrics_->RecordSwitchConversationTarget("B", instance2.conversation_id(),
                                           &instanceNew);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToExistingInstance, 1);
}

TEST_F(GlicInstanceCoordinatorMetricsTest,
       SwitchConversation_InstanceSwitchResume) {
  MockGlicInstance instance1;
  EXPECT_CALL(instance1, conversation_id())
      .WillRepeatedly(Return(std::string("A")));
  EXPECT_CALL(instance1, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now() - base::Seconds(10)));

  MockGlicInstance instance2;
  EXPECT_CALL(instance2, conversation_id())
      .WillRepeatedly(Return(std::string("B")));
  // User switches windows to instance 2 (which has B).
  EXPECT_CALL(instance2, GetLastActiveTime())
      .WillRepeatedly(Return(base::TimeTicks::Now()));

  EXPECT_CALL(provider_, GetInstances())
      .WillRepeatedly(
          Return(std::vector<GlicInstance*>{&instance1, &instance2}));

  // User switches conversation back to A.
  metrics_->RecordSwitchConversationTarget("A", instance1.conversation_id(),
                                           &instance2);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToLastActive, 1);
}

}  // namespace
}  // namespace glic
