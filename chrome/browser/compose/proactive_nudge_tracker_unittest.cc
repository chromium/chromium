// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace compose {
namespace {

using base::test::TestFuture;
using testing::_;
const autofill::FieldRendererId kFieldRendererId(123);
const autofill::FieldRendererId kFieldRendererId2(4);

segmentation_platform::TrainingRequestId TrainingRequestId(
    int request_number = 0) {
  return segmentation_platform::TrainingRequestId(request_number + 456);
}

autofill::FormFieldData CreateTestFormFieldData(
    autofill::FieldRendererId renderer_id = kFieldRendererId) {
  autofill::FormFieldData f;
  f.set_host_frame(autofill::test::MakeLocalFrameToken());
  f.set_renderer_id(renderer_id);
  f.set_value(u"FormFieldDataInitialValue");
  return f;
}

class MockProactiveNudgeTrackerDelegate
    : public ProactiveNudgeTracker::Delegate {
 public:
  MOCK_METHOD(void,
              ShowProactiveNudge,
              (autofill::FormGlobalId, autofill::FieldGlobalId));
};

class ProactiveNudgeTrackerTestBase : public testing::Test {
 public:
  ProactiveNudgeTrackerTestBase() = default;

  ProactiveNudgeTrackerTestBase(const ProactiveNudgeTrackerTestBase&) = delete;
  ProactiveNudgeTrackerTestBase& operator=(
      const ProactiveNudgeTrackerTestBase&) = delete;

  ~ProactiveNudgeTrackerTestBase() override = default;

  void SetUpNudgeTrackerTest(bool use_segmentation) {
    compose::GetMutableConfigForTesting().proactive_nudge_segmentation =
        use_segmentation;
    nudge_tracker_ = std::make_unique<ProactiveNudgeTracker>(
        &segmentation_service_, &delegate_);

    if (use_segmentation) {
      SetSegmentationResult();
    } else {
      EXPECT_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
          .Times(0);
    }
  }

  void TearDown() override {
    compose::ResetConfigForTesting();
  }

  segmentation_platform::MockSegmentationPlatformService&
  segmentation_service() {
    return segmentation_service_;
  }
  MockProactiveNudgeTrackerDelegate& delegate() { return delegate_; }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }
  ProactiveNudgeTracker& nudge_tracker() { return *nudge_tracker_; }

  void SetSegmentationResult(std::string label = "Show") {
    ON_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [label, this](
                segmentation_platform::ClassificationResultCallback callback) {
              auto result = segmentation_platform::ClassificationResult(
                  segmentation_platform::PredictionStatus::kSucceeded);
              result.request_id = TrainingRequestId(training_request_number_++);
              result.ordered_labels = {label};
              std::move(callback).Run(result);
            })));
  }

  // This helper function is a shortcut to adding a test future to listen for
  // compose responses.
  void BindFutureToSegmentationRequest(
      base::test::TestFuture<
          segmentation_platform::ClassificationResultCallback>& future) {
    ON_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [&](segmentation_platform::ClassificationResultCallback cb) {
              future.SetValue(std::move(cb));
            })));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  testing::NiceMock<MockProactiveNudgeTrackerDelegate> delegate_;

  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_service_;
  std::unique_ptr<ProactiveNudgeTracker> nudge_tracker_;
  int training_request_number_ = 0;
};

class ProactiveNudgeTrackerTest : public ProactiveNudgeTrackerTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  ProactiveNudgeTrackerTest() = default;

  ~ProactiveNudgeTrackerTest() override = default;

  void SetUp() override {
    ProactiveNudgeTrackerTestBase::SetUpNudgeTrackerTest(uses_segmentation());
  }

  bool uses_segmentation() { return GetParam(); }
};

TEST_P(ProactiveNudgeTrackerTest, TestWait) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  BindFutureToSegmentationRequest(future);

  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  // Should not nudge if nudge is requested too soon.
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));

  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
  if (uses_segmentation()) {
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
    auto result = segmentation_platform::ClassificationResult(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels = {"Show"};
    future.Take().Run(result);
  }
  EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestWaitSegmentationFirst) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  BindFutureToSegmentationRequest(future);

  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  if (uses_segmentation()) {
    auto result = segmentation_platform::ClassificationResult(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels = {"Show"};
    future.Take().Run(result);
  }
  // Should not nudge if nudge is requested too soon.
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));

  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
  EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangePreventsNudge) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  nudge_tracker().FocusChangedInPage();

  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestTrackingDifferentFormField) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  auto field2 = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field2.renderer_form_id(), field2.global_id()))
      .Times(1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field2));
  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangeInUninitializedState) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  nudge_tracker().FocusChangedInPage();
  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
}

TEST_P(ProactiveNudgeTrackerTest, TestNoNudgeDelay) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_delay = base::Milliseconds(0);

  auto field = CreateTestFormFieldData();
  if (uses_segmentation()) {
    base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
        future;
    BindFutureToSegmentationRequest(future);
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
        .Times(1);
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
    auto result = segmentation_platform::ClassificationResult(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels = {"Show"};
    future.Take().Run(result);
  } else {
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
        .Times(0);
    EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
    // Wait just in case the timer could be pending.
    task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
  }
}

TEST_P(ProactiveNudgeTrackerTest, SegmentationDoesNotSucceed) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();
  if (uses_segmentation()) {
    BindFutureToSegmentationRequest(future);
  }

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(uses_segmentation() ? 0 : 1);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);

  if (uses_segmentation()) {
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
    auto result = segmentation_platform::ClassificationResult(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels = {
        segmentation_platform::kComposePrmotionLabelDontShow};
    future.Take().Run(result);
  }

  EXPECT_NE(uses_segmentation(),
            nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

INSTANTIATE_TEST_SUITE_P(,
                         ProactiveNudgeTrackerTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "SegmentationON"
                                             : "SegmentationOFF";
                         });

class ProactiveNudgeTrackerDerivedEngagementTest
    : public ProactiveNudgeTrackerTestBase {
 public:
  void SetUp() override {
    ProactiveNudgeTrackerTestBase::SetUpNudgeTrackerTest(true);
  }

  // Set up a scenario where the nudge is shown for a field.
  TestFuture<segmentation_platform::TrainingLabels>& TriggerNudgeForField(
      int request_number,
      const autofill::FormFieldData& field) {
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
        .Times(1);

    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
    task_environment().FastForwardBy(GetComposeConfig().proactive_nudge_delay);
    EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));

    TestFuture<segmentation_platform::TrainingLabels>& training_labels =
        training_labels_futures_.emplace_back();
    EXPECT_CALL(segmentation_service(),
                CollectTrainingData(
                    segmentation_platform::proto::SegmentId::
                        OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION,
                    TrainingRequestId(request_number), _, _))
        .Times(1)
        .WillOnce(testing::Invoke([&](auto, auto, auto labels, auto) {
          training_labels.SetValue(labels);
        }));
    return training_labels;
  }

  // Destroy the nudge tracker. This triggers CollectTrainingData() if
  // necessary.
  void Reset() { SetUpNudgeTrackerTest(true); }

 private:
  // Just holds memory for futures created in TriggerNudgeForField(), not for
  // direct use.
  std::deque<TestFuture<segmentation_platform::TrainingLabels>>
      training_labels_futures_;
};

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, NoEngagement) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  Reset();

  EXPECT_EQ(training_labels.Get().output_metric,
            std::make_pair("Compose.ProactiveNudge.DerivedEngagement",
                           static_cast<base::HistogramBase::Sample>(
                               ProactiveNudgeDerivedEngagement::kIgnored)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, MinimalUse) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  compose::ComposeSessionEvents events;
  nudge_tracker().ComposeSessionCompleted(
      kFieldRendererId, ComposeSessionCloseReason::kCloseButtonPressed, events);
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kOpenedComposeMinimalUse)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, SuggestionGenerated) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  compose::ComposeSessionEvents events;
  events.compose_count = 1;
  nudge_tracker().ComposeSessionCompleted(
      kFieldRendererId, ComposeSessionCloseReason::kCloseButtonPressed, events);
  // This test should work with or without deleting the tracker.
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kGeneratedComposeSuggestion)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, AcceptedSuggestion) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  compose::ComposeSessionEvents events;
  events.compose_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      kFieldRendererId, ComposeSessionCloseReason::kAcceptedSuggestion, events);

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest,
       IgnoresSessionForDifferentField) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  compose::ComposeSessionEvents events;
  // This one is ignored because it has the wrong field id.
  nudge_tracker().ComposeSessionCompleted(
      autofill::FieldRendererId(999),
      ComposeSessionCloseReason::kEndedImplicitly, events);

  events.compose_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      kFieldRendererId, ComposeSessionCloseReason::kAcceptedSuggestion, events);

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, TwoSessions) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels1 =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  TestFuture<segmentation_platform::TrainingLabels>& training_labels2 =
      TriggerNudgeForField(1, CreateTestFormFieldData(kFieldRendererId2));
  compose::ComposeSessionEvents events;
  events.compose_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      kFieldRendererId, ComposeSessionCloseReason::kAcceptedSuggestion, events);
  Reset();

  EXPECT_EQ(
      training_labels1.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion)));
  EXPECT_EQ(training_labels2.Get().output_metric,
            std::make_pair("Compose.ProactiveNudge.DerivedEngagement",
                           static_cast<base::HistogramBase::Sample>(
                               ProactiveNudgeDerivedEngagement::kIgnored)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, NudgeDisabledSingleSite) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  nudge_tracker().OnUserDisabledNudge(/*single_site_only=*/true);
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kNudgeDisabledOnSingleSite)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, NudgeDisabledAllSites) {
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, CreateTestFormFieldData());
  nudge_tracker().OnUserDisabledNudge(/*single_site_only=*/false);
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kNudgeDisabledOnAllSites)));
}

}  // namespace
}  // namespace compose
