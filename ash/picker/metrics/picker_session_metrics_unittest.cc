// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Property;

class PickerSessionMetricsTest : public testing::Test {
 public:
  void SetUp() override { structured_metrics_recorder_.Initialize(); }

 protected:
  metrics::structured::TestStructuredMetricsRecorder
      structured_metrics_recorder_;
};

TEST_F(PickerSessionMetricsTest, RecordsSessionOutcomeOnce) {
  base::HistogramTester histogram;
  PickerSessionMetrics metrics;

  metrics.RecordOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  metrics.RecordOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kAbandoned);
  metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kUnknown);

  histogram.ExpectUniqueSample(
      "Ash.Picker.Session.Outcome",
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied, 1);
}

TEST_F(PickerSessionMetricsTest, RecordsUnknownOutcomeOnDestruction) {
  base::HistogramTester histogram;
  { PickerSessionMetrics metrics; }

  histogram.ExpectUniqueSample("Ash.Picker.Session.Outcome",
                               PickerSessionMetrics::SessionOutcome::kUnknown,
                               1);
}

TEST_F(PickerSessionMetricsTest,
       DoesNotRecordUnknownOutcomeOnDestructionIfOutcomeWasRecorded) {
  base::HistogramTester histogram;
  {
    PickerSessionMetrics metrics;
    metrics.RecordOutcome(PickerSessionMetrics::SessionOutcome::kAbandoned);
  }

  histogram.ExpectUniqueSample("Ash.Picker.Session.Outcome",
                               PickerSessionMetrics::SessionOutcome::kAbandoned,
                               1);
}

auto ContainsEvent(const metrics::structured::Event& event) {
  return Contains(AllOf(
      Property("event name", &metrics::structured::Event::event_name,
               Eq(event.event_name())),
      Property("metric values", &metrics::structured::Event::metric_values,
               Eq(std::ref(event.metric_values())))));
}

TEST_F(PickerSessionMetricsTest, OnStartSessionMetricsOnPlainTextField) {
  ui::FakeTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 1));
  PickerSessionMetrics metrics;

  metrics.OnStartSession(&client);

  cros_events::Picker_StartSession expected_event;
  expected_event
      .SetInputFieldType(cros_events::PickerInputFieldType::PLAIN_TEXT)
      .SetSelectionLength(0);
  EXPECT_THAT(structured_metrics_recorder_.GetEvents(),
              ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, OnStartSessionMetricsOnRichTextField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  client.SetTextAndSelection(u"abcd", gfx::Range(1, 4));
  PickerSessionMetrics metrics;

  metrics.OnStartSession(&client);

  cros_events::Picker_StartSession expected_event;
  expected_event.SetInputFieldType(cros_events::PickerInputFieldType::RICH_TEXT)
      .SetSelectionLength(3);
  EXPECT_THAT(structured_metrics_recorder_.GetEvents(),
              ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, OnStartSessionMetricsForNullTextInputClient) {
  PickerSessionMetrics metrics;

  metrics.OnStartSession(nullptr);

  cros_events::Picker_StartSession expected_event;
  expected_event.SetInputFieldType(cros_events::PickerInputFieldType::NONE)
      .SetSelectionLength(0);
  EXPECT_THAT(structured_metrics_recorder_.GetEvents(),
              ContainsEvent(expected_event));
}

}  // namespace
}  // namespace ash
