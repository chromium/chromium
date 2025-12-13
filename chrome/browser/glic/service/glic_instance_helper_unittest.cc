// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

class GlicInstanceHelperTest : public testing::Test {
 public:
  GlicInstanceHelperTest() = default;
  ~GlicInstanceHelperTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_tab_, GetContents())
        .WillRepeatedly(testing::Return(nullptr));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  tabs::MockTabInterface mock_tab_;
  base::HistogramTester histogram_tester_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(GlicInstanceHelperTest, DestructionLogsNothingIfEmpty) {
  // Scope to ensure destruction afterwards.
  {
    GlicInstanceHelper helper(&mock_tab_);
  }
  histogram_tester_.ExpectTotalCount("Glic.Tab.InstanceBindCount", 0);
  histogram_tester_.ExpectTotalCount("Glic.Tab.InstancePinCount", 0);
}

TEST_F(GlicInstanceHelperTest, LogsBindCount) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    InstanceId id1 = base::Uuid::GenerateRandomV4();
    InstanceId id2 = base::Uuid::GenerateRandomV4();
    helper.SetInstanceId(id1);
    helper.SetInstanceId(id2);
    // Duplicate should be ignored in count
    helper.SetInstanceId(id1);
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstanceBindCount", 2, 1);
}

TEST_F(GlicInstanceHelperTest, LogsPinCount) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.OnPinnedByInstance(base::Uuid::GenerateRandomV4());
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstancePinCount", 1, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeNoAction) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained();
    // Destructor triggers logging.
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel",
      DaisyChainFirstAction::kNoAction, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeInputSubmitted) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained();
    helper.OnDaisyChainAction(DaisyChainFirstAction::kInputSubmitted);
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel",
      DaisyChainFirstAction::kInputSubmitted, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeSidePanelClosedDelayed) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained();
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Should not log immediately.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel", 0);
    // Fast forward to trigger timeout.
    task_environment_.FastForwardBy(base::Seconds(6));
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel",
      DaisyChainFirstAction::kSidePanelClosed, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeRaceCondition) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained();
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Ambiguous action start timer.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel", 0);

    // Terminal action happens before timeout.
    helper.OnDaisyChainAction(DaisyChainFirstAction::kRecursiveDaisyChain);
  }
  // Should log terminal action immediately and ignore side panel closed.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel",
      DaisyChainFirstAction::kRecursiveDaisyChain, 1);
}

TEST_F(GlicInstanceHelperTest,
       LogsDaisyChainOutcomeSidePanelClosedDestruction) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained();
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Should not log immediately.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel", 0);
    // Destruction happens before timeout (timer was 5s).
    task_environment_.FastForwardBy(base::Seconds(2));
  }
  // Should log on destruction.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel",
      DaisyChainFirstAction::kSidePanelClosed, 1);
}

}  // namespace glic
