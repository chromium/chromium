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

class FakeGlicInstance : public GlicInstanceHelper::Instance {
 public:
  explicit FakeGlicInstance(InstanceId id) : id_(id) {}
  ~FakeGlicInstance() = default;

  const InstanceId& id() const override { return id_; }
  std::optional<std::string> conversation_id() const override {
    return "test_conversation_id";
  }

 private:
  InstanceId id_;
};

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
    InstanceId id1_val = base::Uuid::GenerateRandomV4();
    InstanceId id2_val = base::Uuid::GenerateRandomV4();
    FakeGlicInstance instance1(id1_val);
    FakeGlicInstance instance2(id2_val);

    helper.SetBoundInstance(&instance1);
    helper.SetBoundInstance(&instance2);
    // Duplicate should be ignored in count (if it were the same instance ID,
    // although here we are checking unique instance IDs bound).
    // The metric logic counts unique IDs.
    helper.SetBoundInstance(&instance1);

    // Clean up to prevent UAF in helper destructor
    helper.SetBoundInstance(nullptr);
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstanceBindCount", 2, 1);
}

TEST_F(GlicInstanceHelperTest, LogsPinCount) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    FakeGlicInstance instance(base::Uuid::GenerateRandomV4());
    helper.OnPinnedByInstance(&instance);

    // Cleanup
    helper.OnUnpinnedByInstance(&instance);
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstancePinCount", 1, 1);
}

TEST_F(GlicInstanceHelperTest, GettersWork) {
  GlicInstanceHelper helper(&mock_tab_);
  InstanceId id = base::Uuid::GenerateRandomV4();
  FakeGlicInstance instance(id);

  helper.SetBoundInstance(&instance);
  EXPECT_EQ(helper.GetInstanceId(), id);
  EXPECT_EQ(helper.GetConversationId(), "test_conversation_id");

  helper.OnPinnedByInstance(&instance);
  EXPECT_THAT(helper.GetPinnedInstances(), testing::ElementsAre(&instance));

  helper.OnUnpinnedByInstance(&instance);
  EXPECT_TRUE(helper.GetPinnedInstances().empty());

  helper.SetBoundInstance(nullptr);
  EXPECT_FALSE(helper.GetInstanceId().has_value());
  EXPECT_FALSE(helper.GetConversationId().has_value());
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeNoAction) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kTabContents);
    // Destructor triggers logging.
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.TabContents",
      DaisyChainFirstAction::kNoAction, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeInputSubmitted) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kGlicContents);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kInputSubmitted);
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.GlicContents",
      DaisyChainFirstAction::kInputSubmitted, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeSidePanelClosedDelayed) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kActorAddTab);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Should not log immediately.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel.ActorAddTab", 0);
    // Fast forward to trigger timeout.
    task_environment_.FastForwardBy(base::Seconds(6));
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.ActorAddTab",
      DaisyChainFirstAction::kSidePanelClosed, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeRaceCondition) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kTabContents);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Ambiguous action start timer.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel.TabContents", 0);

    // Terminal action happens before timeout.
    helper.OnDaisyChainAction(DaisyChainFirstAction::kRecursiveDaisyChain);
  }
  // Should log terminal action immediately and ignore side panel closed.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.TabContents",
      DaisyChainFirstAction::kRecursiveDaisyChain, 1);
}

TEST_F(GlicInstanceHelperTest,
       LogsDaisyChainOutcomeSidePanelClosedDestruction) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kNewTab);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Should not log immediately.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel.NewTab", 0);
    // Destruction happens before timeout (timer was 5s).
    task_environment_.FastForwardBy(base::Seconds(2));
  }
  // Should log on destruction.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.NewTab",
      DaisyChainFirstAction::kSidePanelClosed, 1);
}

TEST_F(GlicInstanceHelperTest, LogsNewTabOutcome) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kNewTab);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kInputSubmitted);
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.NewTab",
      DaisyChainFirstAction::kInputSubmitted, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeNoActionOverwrite) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kTabContents);
    helper.OnDaisyChainAction(DaisyChainFirstAction::kSidePanelClosed);
    // Ambiguous action start timer.
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel.TabContents", 0);

    // NoAction should be ignored if we already have an action.
    helper.OnDaisyChainAction(DaisyChainFirstAction::kNoAction);
    histogram_tester_.ExpectTotalCount(
        "Glic.Instance.FirstActionInDaisyChainPanel.TabContents", 0);

    // Fast forward to trigger timeout.
    task_environment_.FastForwardBy(base::Seconds(6));
  }
  // Should log SidePanelClosed.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.TabContents",
      DaisyChainFirstAction::kSidePanelClosed, 1);
}

TEST_F(GlicInstanceHelperTest, LogsDaisyChainOutcomeNoActionOnDestruction) {
  {
    GlicInstanceHelper helper(&mock_tab_);
    helper.SetIsDaisyChained(DaisyChainSource::kTabContents);
    // No action performed.
    // Destructor triggers logging.
  }
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.FirstActionInDaisyChainPanel.TabContents",
      DaisyChainFirstAction::kNoAction, 1);
}

}  // namespace glic
