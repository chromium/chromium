// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace compose {

class MockProactiveNudgeTrackerDelegate
    : public ProactiveNudgeTracker::Delegate {
 public:
  MOCK_METHOD(void,
              ShowProactiveNudge,
              (autofill::FormGlobalId, autofill::FieldGlobalId),
              (override));
};

class ProactiveNudgeTrackerTest : public testing::TestWithParam<bool> {
 public:
  ProactiveNudgeTrackerTest() = default;

  ProactiveNudgeTrackerTest(const ProactiveNudgeTrackerTest&) = delete;
  ProactiveNudgeTrackerTest& operator=(const ProactiveNudgeTrackerTest&) =
      delete;

  ~ProactiveNudgeTrackerTest() override = default;

  void SetUp() override {
    testing::TestWithParam<bool>::SetUp();
    compose::GetMutableConfigForTesting().proactive_nudge_segmentation =
        GetParam();
    nudge_tracker_ = std::make_unique<ProactiveNudgeTracker>(
        &segmentation_service_, &delegate_);

    if (uses_segmentation()) {
      SetSegmentationResult();
    } else {
      EXPECT_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
          .Times(0);
    }
  }

  void TearDown() override {
    testing::TestWithParam<bool>::TearDown();
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

  autofill::FormFieldData CreateTestFormFieldData() {
    autofill::FormFieldData f;
    f.set_host_frame(autofill::test::MakeLocalFrameToken());
    f.set_renderer_id(autofill::FieldRendererId(123));
    f.set_value(u"FormFieldDataInitialValue");
    return f;
  }

  void SetSegmentationResult(std::string label = "Show") {
    ON_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [label](
                segmentation_platform::ClassificationResultCallback callback) {
              auto result = segmentation_platform::ClassificationResult(
                  segmentation_platform::PredictionStatus::kSucceeded);
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

  bool uses_segmentation() { return GetParam(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  testing::NiceMock<MockProactiveNudgeTrackerDelegate> delegate_;

  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_service_;
  std::unique_ptr<ProactiveNudgeTracker> nudge_tracker_;
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

  task_environment().FastForwardBy(base::Seconds(4));
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

  task_environment().FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangePreventsNudge) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
  nudge_tracker().FocusChangedInPage();

  task_environment().FastForwardBy(base::Seconds(4));
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
  task_environment().FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(field));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangeInUninitializedState) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id()))
      .Times(0);

  nudge_tracker().FocusChangedInPage();
  task_environment().FastForwardBy(base::Seconds(4));
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
    task_environment().FastForwardBy(base::Seconds(4));
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
  task_environment().FastForwardBy(base::Seconds(4));

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

INSTANTIATE_TEST_SUITE_P(ProactiveNudgeTrackerTest,
                         ProactiveNudgeTrackerTest,
                         ::testing::Bool());
}  // namespace compose
