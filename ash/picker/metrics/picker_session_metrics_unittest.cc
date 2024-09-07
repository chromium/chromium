// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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
  void SetUp() override {
    metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    metrics_recorder_->Initialize();
  }

  void TearDown() override { metrics_recorder_.reset(); }

 protected:
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      metrics_recorder_;
};

TEST_F(PickerSessionMetricsTest, RecordsUmaSessionOutcomeOnce) {
  base::HistogramTester histogram;
  {
    PickerSessionMetrics metrics;

    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kAbandoned);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kUnknown);
  }

  histogram.ExpectUniqueSample(
      "Ash.Picker.Session.Outcome",
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied, 1);
}

TEST_F(PickerSessionMetricsTest, RecordsUmaUnknownOutcomeOnDestruction) {
  base::HistogramTester histogram;
  { PickerSessionMetrics metrics; }

  histogram.ExpectUniqueSample("Ash.Picker.Session.Outcome",
                               PickerSessionMetrics::SessionOutcome::kUnknown,
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
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
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
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, OnStartSessionMetricsForNullTextInputClient) {
  PickerSessionMetrics metrics;

  metrics.OnStartSession(nullptr);

  EXPECT_EQ(metrics_recorder_->GetEvents().size(), 1U);
  cros_events::Picker_StartSession expected_event;
  expected_event.SetInputFieldType(cros_events::PickerInputFieldType::NONE)
      .SetSelectionLength(0);
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, RecordsDefaultFinishSessionEvent) {
  { PickerSessionMetrics metrics; }

  cros_events::Picker_FinishSession expected_event;
  expected_event.SetOutcome(cros_events::PickerSessionOutcome::UNKNOWN)
      .SetAction(cros_events::PickerAction::UNKNOWN)
      .SetResultSource(cros_events::PickerResultSource::UNKNOWN)
      .SetResultType(cros_events::PickerResultType::UNKNOWN)
      .SetTotalEdits(0)
      .SetFinalQuerySize(0)
      .SetResultIndex(-1);
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, RecordsFinishSessionEventForInsert) {
  {
    PickerSessionMetrics metrics;
    metrics.SetSelectedCategory(PickerCategory::kDatesTimes);
    metrics.UpdateSearchQuery(u"abc");
    metrics.UpdateSearchQuery(u"abcdef");
    metrics.UpdateSearchQuery(u"abcde");
    metrics.SetSelectedResult(
        PickerTextResult(u"primary", PickerTextResult::Source::kDate), 3);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event
      .SetOutcome(cros_events::PickerSessionOutcome::INSERTED_OR_COPIED)
      .SetAction(cros_events::PickerAction::OPEN_DATES_TIMES)
      .SetResultSource(cros_events::PickerResultSource::DATES_TIMES)
      .SetResultType(cros_events::PickerResultType::TEXT)
      .SetTotalEdits(7)
      .SetFinalQuerySize(5)
      .SetResultIndex(3);
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, RecordsFinishSessionEventForCaseTransform) {
  {
    PickerSessionMetrics metrics;
    metrics.SetSelectedResult(
        PickerCaseTransformResult(PickerCaseTransformResult::Type::kUpperCase),
        0);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kFormat);
  }

  cros_events::Picker_FinishSession expected_event;
  expected_event.SetOutcome(cros_events::PickerSessionOutcome::FORMAT)
      .SetAction(cros_events::PickerAction::UNKNOWN)
      .SetResultSource(cros_events::PickerResultSource::CASE_TRANSFORM)
      .SetResultType(cros_events::PickerResultType::TEXT)
      .SetTotalEdits(0)
      .SetFinalQuerySize(0)
      .SetResultIndex(0);
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_THAT(events, ContainsEvent(expected_event));
}

TEST_F(PickerSessionMetricsTest, UpdatesCapsLockPrefsWhenNotSelected) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockDislayedCountPrefName, 2);
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockSelectedCountPrefName, 1);

  {
    PickerSessionMetrics metrics(&prefs);
    metrics.SetCapsLockDisplayed(true);
    metrics.SetSelectedResult(
        PickerCaseTransformResult(PickerCaseTransformResult::Type::kUpperCase),
        0);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kFormat);
  }

  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockDislayedCountPrefName), 3);
  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockSelectedCountPrefName), 1);
}

TEST_F(PickerSessionMetricsTest, UpdatesCapsLockPrefsWhenSelected) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockDislayedCountPrefName, 2);
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockSelectedCountPrefName, 1);

  {
    PickerSessionMetrics metrics(&prefs);
    metrics.SetCapsLockDisplayed(true);
    metrics.SetSelectedResult(
        PickerCapsLockResult(
            /*enabled=*/true, PickerCapsLockResult::Shortcut::kAltSearch),
        0);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kFormat);
  }

  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockDislayedCountPrefName), 3);
  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockSelectedCountPrefName), 2);
}

TEST_F(PickerSessionMetricsTest, DoesNotUpdateCapsLockPrefsWhenNotDisplayed) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockDislayedCountPrefName, 2);
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockSelectedCountPrefName, 1);

  {
    PickerSessionMetrics metrics(&prefs);
    metrics.SetSelectedResult(
        PickerCaseTransformResult(PickerCaseTransformResult::Type::kUpperCase),
        0);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kFormat);
  }

  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockDislayedCountPrefName), 2);
  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockSelectedCountPrefName), 1);
}

TEST_F(PickerSessionMetricsTest, HalvesCapsLockPrefs) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockDislayedCountPrefName, 19);
  prefs.registry()->RegisterIntegerPref(
      prefs::kPickerCapsLockSelectedCountPrefName, 9);

  {
    PickerSessionMetrics metrics(&prefs);
    metrics.SetCapsLockDisplayed(true);
    metrics.SetSelectedResult(
        PickerCapsLockResult(
            /*enabled=*/true, PickerCapsLockResult::Shortcut::kAltSearch),
        0);
    metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kFormat);
  }

  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockDislayedCountPrefName), 10);
  EXPECT_EQ(prefs.GetInteger(prefs::kPickerCapsLockSelectedCountPrefName), 5);
}

}  // namespace
}  // namespace ash
