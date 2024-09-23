// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/proactive_nudge_tracker.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace compose {
namespace {

using base::test::TestFuture;
using testing::_;
using testing::Pair;
const autofill::FieldRendererId kFieldRendererId(123);
const autofill::FieldRendererId kFieldRendererId2(4);
constexpr float kSegmentationForceShowResult = 0.01;

segmentation_platform::TrainingRequestId TrainingRequestId(
    int request_number = 0) {
  return segmentation_platform::TrainingRequestId(request_number + 456);
}

autofill::FormFieldData CreateTestFormFieldData(
    autofill::FieldRendererId renderer_id = kFieldRendererId) {
  autofill::FormFieldData f;
  f.set_max_length(524);
  f.set_host_frame(autofill::test::MakeLocalFrameToken());
  f.set_renderer_id(renderer_id);
  f.set_value(u"FormFieldDataInitialValue");
  f.set_selected_text(u"selected text");
  f.set_placeholder(u"Please enter your value");
  f.set_bounds(gfx::RectF(300, 100));
  f.set_form_control_type(autofill::FormControlType::kTextArea);
  f.set_label(u"field label");
  f.set_aria_label(u"aria label");
  f.set_aria_description(u"aria description");

  return f;
}

autofill::FormData CreateFormData() {
  // Set up a form with two fields, the compose-eligible field is the first one.
  autofill::FormFieldData other_field;
  other_field.set_form_control_type(autofill::FormControlType::kInputMonth);
  autofill::FormData form;
  form.set_fields({CreateTestFormFieldData(), std::move(other_field)});
  return form;
}

GURL TestURL() {
  return GURL("https://example.com/test");
}
url::Origin TestOrigin() {
  return url::Origin::Create(TestURL());
}

class MockProactiveNudgeTrackerDelegate
    : public ProactiveNudgeTracker::Delegate {
 public:
  MOCK_METHOD(void,
              ShowProactiveNudge,
              (autofill::FormGlobalId,
               autofill::FieldGlobalId,
               compose::ComposeEntryPoint));
  MOCK_METHOD(float, SegmentationFallbackShowResult, ());
  float SegmentationForceShowResult() override {
    return kSegmentationForceShowResult;
  }
  compose::PageUkmTracker* GetPageUkmTracker() override {
    return page_ukm_tracker_.get();
  }

  compose::ComposeHintMetadata GetComposeHintMetadata() override {
    return compose_metadata_;
  }

  void SetComposeHintMetadata(compose::ComposeHintMetadata metadata) {
    compose_metadata_ = metadata;
  }

 private:
  const ukm::SourceId valid_test_source_id_{1};
  std::unique_ptr<compose::PageUkmTracker> page_ukm_tracker_ =
      std::make_unique<compose::PageUkmTracker>(valid_test_source_id_);
  compose::ComposeHintMetadata compose_metadata_;
};

class ProactiveNudgeTrackerTestBase : public testing::Test {
 public:
  ProactiveNudgeTrackerTestBase() = default;

  ProactiveNudgeTrackerTestBase(const ProactiveNudgeTrackerTestBase&) = delete;
  ProactiveNudgeTrackerTestBase& operator=(
      const ProactiveNudgeTrackerTestBase&) = delete;

  ~ProactiveNudgeTrackerTestBase() override = default;

  void SetUpNudgeTrackerTest(bool use_segmentation) {
    compose::Config& config = compose::GetMutableConfigForTesting();
    config.proactive_nudge_enabled = true;
    config.proactive_nudge_segmentation = use_segmentation;
    config.proactive_nudge_focus_delay = base::Microseconds(4);
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
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), result));
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

ProactiveNudgeTracker::Signals TestSignals(
    autofill::FormFieldData field = CreateTestFormFieldData(),
    base::TimeTicks page_change_time = base::TimeTicks::Now()) {
  ProactiveNudgeTracker::Signals signals;
  signals.ukm_source_id = ukm::kInvalidSourceId;
  signals.field = field;
  signals.form = CreateFormData();
  signals.page_change_time = page_change_time;
  signals.page_origin = TestOrigin();
  signals.page_url = TestURL();
  return signals;
}

TEST_P(ProactiveNudgeTrackerTest, TestWait) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  BindFutureToSegmentationRequest(future);

  auto field = CreateTestFormFieldData();

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  // Should not nudge if nudge is requested too soon.
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));

  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  if (uses_segmentation()) {
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
        TestSignals(field)));
    auto result = segmentation_platform::ClassificationResult(
        segmentation_platform::PredictionStatus::kSucceeded);
    result.ordered_labels = {"Show"};
    future.Take().Run(result);
  }
  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangePreventsNudge) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(0);

  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  nudge_tracker().FocusChangedInPage();

  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_P(ProactiveNudgeTrackerTest, TestTrackingDifferentFormField) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(0);

  auto field2 = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field2.renderer_form_id(), field2.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field2)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_P(ProactiveNudgeTrackerTest, TestFocusChangeInUninitializedState) {
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(0);

  nudge_tracker().FocusChangedInPage();
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
}

TEST_P(ProactiveNudgeTrackerTest, TestNoNudgeDelay) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_focus_delay = base::Milliseconds(0);

  auto field = CreateTestFormFieldData();
  if (uses_segmentation()) {
    base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
        future;
    BindFutureToSegmentationRequest(future);
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                   compose::ComposeEntryPoint::kProactiveNudge))
        .Times(0);
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
        TestSignals(field)));
    // Wait just in case the timer could be pending.
    task_environment().FastForwardBy(
        GetComposeConfig().proactive_nudge_focus_delay);
  } else {
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                   compose::ComposeEntryPoint::kProactiveNudge))
        .Times(0);
    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
        TestSignals(field)));
    // Wait just in case the timer could be pending.
    task_environment().FastForwardBy(
        GetComposeConfig().proactive_nudge_focus_delay);
  }
}

TEST_P(ProactiveNudgeTrackerTest, TestOneNudgeUntilCleared) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_field_per_navigation = true;
  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  ASSERT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  nudge_tracker().FocusChangedInPage();
  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  nudge_tracker().Clear();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  ASSERT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  nudge_tracker().FocusChangedInPage();
  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
}

TEST_P(ProactiveNudgeTrackerTest, TestOneNudgePerFocus) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_field_per_navigation = false;

  auto field = CreateTestFormFieldData();
  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(2);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  ASSERT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  nudge_tracker().FocusChangedInPage();
  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProactiveNudgeTrackerTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "SegmentationON"
                                             : "SegmentationOFF";
                         });

class ProactiveNudgeTrackerSegmentationTest
    : public ProactiveNudgeTrackerTestBase {
 public:
  void SetUp() override {
    ProactiveNudgeTrackerTestBase::SetUpNudgeTrackerTest(true);
  }
};

TEST_F(ProactiveNudgeTrackerSegmentationTest, SegmentationDontShow) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();
  BindFutureToSegmentationRequest(future);

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(0);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {
      segmentation_platform::kComposePrmotionLabelDontShow};
  future.Take().Run(result);

  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest,
       SegmentationShowWithoutCollectingTrainingData) {
  EXPECT_CALL(segmentation_service(), CollectTrainingData(_, _, _, _, _))
      .Times(0);
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();
  BindFutureToSegmentationRequest(future);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {segmentation_platform::kComposePrmotionLabelShow};
  future.Take().Run(result);

  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest,
       SegmentationShowWithAlwaysCollectTrainingData) {
  EXPECT_CALL(segmentation_service(), CollectTrainingData(_, _, _, _, _))
      .Times(1);
  compose::GetMutableConfigForTesting()
      .proactive_nudge_always_collect_training_data = true;

  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();
  BindFutureToSegmentationRequest(future);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {segmentation_platform::kComposePrmotionLabelShow};
  future.Take().Run(result);

  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

// Test that when the segmentation result is kComposePrmotionLabelDontShow, the
// nudge can still be shown due to proactive_nudge_force_show_probability.
TEST_F(ProactiveNudgeTrackerSegmentationTest, SegmentationRandomForceShow) {
  compose::GetMutableConfigForTesting().proactive_nudge_force_show_probability =
      kSegmentationForceShowResult + 1e-6;
  EXPECT_CALL(segmentation_service(), CollectTrainingData(_, _, _, _, _))
      .Times(1);
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();
  BindFutureToSegmentationRequest(future);

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {
      segmentation_platform::kComposePrmotionLabelDontShow};
  future.Take().Run(result);

  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest,
       SegmentationNotReadyFallbackDontShow) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();

  // Cause fallback to set DontShow.
  EXPECT_CALL(delegate(), SegmentationFallbackShowResult)
      .WillOnce(testing::Return(1.0f));
  BindFutureToSegmentationRequest(future);

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(0);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kNotReady);
  future.Take().Run(result);

  EXPECT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest,
       SegmentationNotReadyFallbackShow) {
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();

  // Cause fallback to set Show.
  EXPECT_CALL(delegate(), SegmentationFallbackShowResult)
      .WillOnce(testing::Return(0.0f));
  BindFutureToSegmentationRequest(future);

  EXPECT_CALL(delegate(),
              ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                 compose::ComposeEntryPoint::kProactiveNudge))
      .Times(1);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);

  ASSERT_FALSE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kNotReady);
  future.Take().Run(result);

  EXPECT_TRUE(
      nudge_tracker().ProactiveNudgeRequestedForFormField(TestSignals(field)));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest, InputContextNoModelParams) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  // Need a longer delay for this test to measure time on page in seconds.
  config.proactive_nudge_focus_delay = base::Seconds(2);

  scoped_refptr<segmentation_platform::InputContext> input_context;
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();

  ON_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
      .WillByDefault(testing::Invoke(
          [&](auto, auto, scoped_refptr<segmentation_platform::InputContext> ic,
              segmentation_platform::ClassificationResultCallback cb) {
            input_context = ic;
            future.SetValue(std::move(cb));
          }));
  auto page_load_time = base::TimeTicks::Now();
  task_environment().FastForwardBy(base::Seconds(63));
  ASSERT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
      TestSignals(field, page_load_time)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  ASSERT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
      TestSignals(field, page_load_time)));

  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {
      segmentation_platform::kComposePrmotionLabelDontShow};
  future.Take().Run(result);
  ASSERT_TRUE(input_context);

  using Val = segmentation_platform::processing::ProcessedValue;
  EXPECT_THAT(
      input_context->metadata_args,
      testing::UnorderedElementsAre(
          Pair("field_max_length", Val(524.0f)),
          Pair("field_width_pixels", Val(300.0f)),
          Pair("field_height_pixels", Val(100.0f)),
          Pair("field_form_control_type",
               Val(static_cast<float>(autofill::FormControlType::kTextArea))),
          Pair("total_field_count", Val(2.0f)),
          Pair("multiline_field_count", Val(1.0f)),
          Pair("time_spent_on_page",
               Val(63.0f +
                   GetComposeConfig().proactive_nudge_focus_delay.InSeconds())),
          Pair("field_signature",
               Val(autofill::HashFieldSignature(
                   autofill::CalculateFieldSignatureForField(field)))),
          Pair("form_signature",
               Val(autofill::HashFormSignature(
                   autofill::CalculateFormSignature(CreateFormData())))),
          Pair("page_url", Val(GURL("https://example.com/test"))),
          Pair("origin", Val(GURL("https://example.com"))),
          Pair("field_value", Val(std::string("FormFieldDataInitialValue"))),
          Pair("field_selected_text", Val(std::string("selected text"))),
          Pair("field_placeholder_text",
               Val(std::string("Please enter your value"))),
          Pair("field_label", Val(std::string("field label"))),
          Pair("field_aria_label", Val(std::string("aria label"))),
          Pair("field_aria_description",
               Val(std::string("aria description")))));
}

TEST_F(ProactiveNudgeTrackerSegmentationTest, InputContextWithModelParams) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  // Need a longer delay for this test to measure time on page in seconds.
  config.proactive_nudge_focus_delay = base::Seconds(2);

  scoped_refptr<segmentation_platform::InputContext> input_context;
  base::test::TestFuture<segmentation_platform::ClassificationResultCallback>
      future;
  auto field = CreateTestFormFieldData();

  compose::ComposeHintMetadata metadata;
  (*metadata.mutable_model_params())["name1"] = 1.1;
  (*metadata.mutable_model_params())["name2"] = 2.2;
  (*metadata.mutable_model_params())["name3"] = 3.3;
  delegate().SetComposeHintMetadata(metadata);

  ON_CALL(segmentation_service(), GetClassificationResult(_, _, _, _))
      .WillByDefault(testing::Invoke(
          [&](auto, auto, scoped_refptr<segmentation_platform::InputContext> ic,
              segmentation_platform::ClassificationResultCallback cb) {
            input_context = ic;
            future.SetValue(std::move(cb));
          }));
  auto page_load_time = base::TimeTicks::Now();
  task_environment().FastForwardBy(base::Seconds(63));
  ASSERT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
      TestSignals(field, page_load_time)));
  task_environment().FastForwardBy(
      GetComposeConfig().proactive_nudge_focus_delay);
  ASSERT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
      TestSignals(field, page_load_time)));

  auto result = segmentation_platform::ClassificationResult(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels = {
      segmentation_platform::kComposePrmotionLabelDontShow};
  future.Take().Run(result);
  ASSERT_TRUE(input_context);

  using Val = segmentation_platform::processing::ProcessedValue;
  EXPECT_THAT(
      input_context->metadata_args,
      testing::UnorderedElementsAre(
          Pair("field_max_length", Val(524.0f)),
          Pair("field_width_pixels", Val(300.0f)),
          Pair("field_height_pixels", Val(100.0f)),
          Pair("field_form_control_type",
               Val(static_cast<float>(autofill::FormControlType::kTextArea))),
          Pair("total_field_count", Val(2.0f)),
          Pair("multiline_field_count", Val(1.0f)),
          Pair("time_spent_on_page",
               Val(63.0f +
                   GetComposeConfig().proactive_nudge_focus_delay.InSeconds())),
          Pair("field_signature",
               Val(autofill::HashFieldSignature(
                   autofill::CalculateFieldSignatureForField(field)))),
          Pair("form_signature",
               Val(autofill::HashFormSignature(
                   autofill::CalculateFormSignature(CreateFormData())))),
          Pair("page_url", Val(GURL("https://example.com/test"))),
          Pair("origin", Val(GURL("https://example.com"))),
          Pair("field_value", Val(std::string("FormFieldDataInitialValue"))),
          Pair("field_selected_text", Val(std::string("selected text"))),
          Pair("field_placeholder_text",
               Val(std::string("Please enter your value"))),
          Pair("field_label", Val(std::string("field label"))),
          Pair("field_aria_label", Val(std::string("aria label"))),
          Pair("field_aria_description", Val(std::string("aria description"))),
          Pair("name1", Val(1.1f)), Pair("name2", Val(2.2f)),
          Pair("name3", Val(3.3f))));
}

class ProactiveNudgeTrackerDerivedEngagementTest
    : public ProactiveNudgeTrackerTestBase {
 public:
  void SetUp() override {
    // CollectTrainingData is only called for force-shown nudges.
    compose::GetMutableConfigForTesting()
        .proactive_nudge_force_show_probability = 1.0;
    ProactiveNudgeTrackerTestBase::SetUpNudgeTrackerTest(true);
  }

  // Set up a scenario where the nudge is shown for a field.
  TestFuture<segmentation_platform::TrainingLabels>& TriggerNudgeForField(
      int request_number,
      const autofill::FormFieldData& field) {
    EXPECT_CALL(delegate(),
                ShowProactiveNudge(field.renderer_form_id(), field.global_id(),
                                   compose::ComposeEntryPoint::kProactiveNudge))
        .Times(1);

    EXPECT_FALSE(nudge_tracker().ProactiveNudgeRequestedForFormField(
        TestSignals(field)));
    task_environment().FastForwardBy(
        GetComposeConfig().proactive_nudge_focus_delay);
    EXPECT_TRUE(nudge_tracker().ProactiveNudgeRequestedForFormField(
        TestSignals(field)));

    TestFuture<segmentation_platform::TrainingLabels>& training_labels =
        training_labels_futures_.emplace_back();
    EXPECT_CALL(segmentation_service(),
                CollectTrainingData(
                    segmentation_platform::proto::SegmentId::
                        OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION,
                    TrainingRequestId(request_number), _, _, _))
        .Times(1)
        .WillOnce(testing::Invoke([&](auto, auto, auto, auto labels, auto) {
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
  auto form_field_data = CreateTestFormFieldData();
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, form_field_data);
  compose::ComposeSessionEvents events;
  nudge_tracker().ComposeSessionCompleted(
      form_field_data.global_id(),
      ComposeSessionCloseReason::kCloseButtonPressed, events);
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kOpenedComposeMinimalUse)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, SuggestionGenerated) {
  base::HistogramTester histograms;
  auto form_field_data = CreateTestFormFieldData();
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, form_field_data);
  compose::ComposeSessionEvents events;
  events.compose_requests_count = 1;
  nudge_tracker().ComposeSessionCompleted(
      form_field_data.global_id(),
      ComposeSessionCloseReason::kCloseButtonPressed, events);
  // This test should work with or without deleting the tracker.
  Reset();

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kGeneratedComposeSuggestion)));
  histograms.ExpectUniqueSample(
      "Compose.ProactiveNudge.DerivedEngagement",
      ProactiveNudgeDerivedEngagement::kGeneratedComposeSuggestion, 1);
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, AcceptedSuggestion) {
  auto form_field_data = CreateTestFormFieldData();
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, form_field_data);
  compose::ComposeSessionEvents events;
  events.compose_requests_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      form_field_data.global_id(), ComposeSessionCloseReason::kInsertedResponse,
      events);

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest,
       IgnoresSessionForDifferentField) {
  auto form_field_data = CreateTestFormFieldData();
  auto form_field_data_2 =
      CreateTestFormFieldData(autofill::FieldRendererId(999));
  TestFuture<segmentation_platform::TrainingLabels>& training_labels =
      TriggerNudgeForField(0, form_field_data);
  compose::ComposeSessionEvents events;
  // This one is ignored because it has the wrong field id.
  nudge_tracker().ComposeSessionCompleted(form_field_data_2.global_id(),
                                          ComposeSessionCloseReason::kAbandoned,
                                          events);

  events.compose_requests_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      form_field_data.global_id(), ComposeSessionCloseReason::kInsertedResponse,
      events);

  EXPECT_EQ(
      training_labels.Get().output_metric,
      std::make_pair(
          "Compose.ProactiveNudge.DerivedEngagement",
          static_cast<base::HistogramBase::Sample>(
              ProactiveNudgeDerivedEngagement::kAcceptedComposeSuggestion)));
}

TEST_F(ProactiveNudgeTrackerDerivedEngagementTest, TwoSessions) {
  auto form_field_data = CreateTestFormFieldData();
  TestFuture<segmentation_platform::TrainingLabels>& training_labels1 =
      TriggerNudgeForField(0, form_field_data);
  TestFuture<segmentation_platform::TrainingLabels>& training_labels2 =
      TriggerNudgeForField(1, CreateTestFormFieldData(kFieldRendererId2));
  compose::ComposeSessionEvents events;
  events.compose_requests_count = 1;
  events.inserted_results = true;
  nudge_tracker().ComposeSessionCompleted(
      form_field_data.global_id(), ComposeSessionCloseReason::kInsertedResponse,
      events);
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
