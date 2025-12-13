// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_tracing_recorder.h"

#include <optional>

#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;
using TraceId = EventMetrics::TraceId;

using QueryResult = base::test::TestTraceProcessor::QueryResult;
using ::testing::ElementsAreArray;

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

constexpr const char kSlicesQuery[] = R"(
  SELECT name, ts, dur
  FROM slices
  ORDER BY ts ASC, dur DESC
  )";

constexpr const char kScrollJankV4ArgsQuery[] = R"(
    SELECT key, display_value
    FROM slices
    JOIN args USING (arg_set_id)
    WHERE slices.name = 'ScrollJankV4'
      AND key LIKE 'scroll_jank_v4.%'
    ORDER BY key ASC
    )";

class ScrollJankV4RecorderTest : public testing::Test {
 protected:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TestTraceProcessor trace_processor_;

  QueryResult QueryTraceProcessor(const char* query) {
    auto result = trace_processor_.RunQuery(query);
    if (!result.has_value()) {
      ADD_FAILURE() << result.error();
      return {};
    }
    return result.value();
  }

  void ExpectThatAllTraceEventsAreOnTheSameTrack() {
    // Query which returns a single row with the number of distrinct
    // `slices.track_id` values in the trace.
    auto result = QueryTraceProcessor(R"(
        SELECT COUNT(DISTINCT track_id) AS track_count
        FROM slices
        )");
    EXPECT_THAT(result, ElementsAreArray(QueryResult{{"track_count"}, {"1"}}));
  }

  void ExpectThatAllSubEventsAreDescendantsOfMainTraceEvent() {
    auto result = QueryTraceProcessor(R"(
        SELECT
          EXISTS (
            SELECT 1
            FROM ancestor_slice(descendant.id) AS ancestor
            WHERE ancestor.name = 'ScrollJankV4'
          ) AS is_descendant_of_main_trace_event
        FROM slices AS descendant
        WHERE name != 'ScrollJankV4'
        GROUP BY 1
        )");
    EXPECT_THAT(result, ElementsAreArray(QueryResult{
                            {"is_descendant_of_main_trace_event"}, {"1"}}));
  }
};

TEST_F(ScrollJankV4RecorderTest, IrrelevantTracingCategory) {
  trace_processor_.StartTrace("elephant");

  ScrollJankV4TracingRecorder::RecordTraceEvents(
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(20),
                         .last_input_generation_ts = MillisSinceEpoch(30),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 4.0f,
                         .first_input_trace_id = TraceId(99)},
                    /* synthetic= */ std::nullopt),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(60)},
      BeginFrameArgsForScrollJank{.frame_time = MillisSinceEpoch(50),
                                  .interval = base::Milliseconds(16)},
      ScrollJankV4Result{
          .missed_vsyncs_per_reason = {7, 8, 0, 0},
          .vsyncs_since_previous_frame = 9,
          .running_delivery_cutoff = base::Milliseconds(11),
          .adjusted_delivery_cutoff = base::Milliseconds(12),
          .current_delivery_cutoff = base::Milliseconds(13),
          .first_scroll_update =
              ScrollJankV4Result::RealFirstScrollUpdate{
                  .actual_input_generation_ts = MillisSinceEpoch(20)},
          .presentation =
              ScrollJankV4Result::DamagingPresentation{
                  .actual_presentation_ts = MillisSinceEpoch(60)},
      });

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(QueryTraceProcessor(kSlicesQuery), ElementsAreArray(QueryResult{
                                                     {"name", "ts", "dur"},
                                                 }));
}

TEST_F(ScrollJankV4RecorderTest, RealDamagingFrame) {
  trace_processor_.StartTrace("input");

  ScrollJankV4TracingRecorder::RecordTraceEvents(
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(20),
                         .last_input_generation_ts = MillisSinceEpoch(30),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 4.0f,
                         .first_input_trace_id = TraceId(99)},
                    /* synthetic= */ std::nullopt),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(60)},
      BeginFrameArgsForScrollJank{.frame_time = MillisSinceEpoch(50),
                                  .interval = base::Milliseconds(16)},
      ScrollJankV4Result{
          .missed_vsyncs_per_reason = {7, 8, 0, 0},
          .vsyncs_since_previous_frame = 9,
          .running_delivery_cutoff = base::Milliseconds(11),
          .adjusted_delivery_cutoff = base::Milliseconds(12),
          .current_delivery_cutoff = base::Milliseconds(13),
          .first_scroll_update =
              ScrollJankV4Result::RealFirstScrollUpdate{
                  .actual_input_generation_ts = MillisSinceEpoch(20)},
          .presentation =
              ScrollJankV4Result::DamagingPresentation{
                  .actual_presentation_ts = MillisSinceEpoch(60)},
      });

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(
      QueryTraceProcessor(kSlicesQuery),
      ElementsAreArray(QueryResult{
          {"name", "ts", "dur"},
          {"ScrollJankV4", "20000000", "40000000"},
          {"Real scroll update input generation", "20000000", "10000000"},
          {"Begin frame", "50000000", "0"},
          {"Presentation", "60000000", "0"},
      }));
  EXPECT_THAT(
      QueryTraceProcessor(kScrollJankV4ArgsQuery),
      ElementsAreArray(QueryResult{
          {"key", "display_value"},
          {"scroll_jank_v4.adjusted_delivery_cutoff_us", "12000000"},
          {"scroll_jank_v4.current_delivery_cutoff_us", "13000000"},
          {"scroll_jank_v4.damage_type", "DAMAGING"},
          {"scroll_jank_v4.is_janky", "true"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].jank_reason",
           "MISSED_VSYNC_DUE_TO_DECELERATING_INPUT_FRAME_DELIVERY"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].missed_vsyncs",
           "7"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].jank_reason",
           "MISSED_VSYNC_DURING_FAST_SCROLL"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].missed_vsyncs",
           "8"},
          {"scroll_jank_v4.running_delivery_cutoff_us", "11000000"},
          {"scroll_jank_v4.updates.first_scroll_update_type", "REAL"},
          {"scroll_jank_v4.updates.real.abs_total_raw_delta_pixels", "5.0"},
          {"scroll_jank_v4.updates.real.first_event_latency_id", "99"},
          {"scroll_jank_v4.updates.real.max_abs_inertial_raw_delta_pixels",
           "4.0"},
          {"scroll_jank_v4.vsync_interval_us", "16000000"},
          {"scroll_jank_v4.vsyncs_since_previous_frame", "9"},
      }));
  ExpectThatAllTraceEventsAreOnTheSameTrack();
  ExpectThatAllSubEventsAreDescendantsOfMainTraceEvent();
}

TEST_F(ScrollJankV4RecorderTest,
       RealNonDamagingFrameWithExtrapolatedPresentationTimestamp) {
  trace_processor_.StartTrace("input");

  ScrollJankV4TracingRecorder::RecordTraceEvents(
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(20),
                         .last_input_generation_ts = MillisSinceEpoch(30),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 4.0f,
                         .first_input_trace_id = TraceId(99)},
                    /* synthetic= */ std::nullopt),
      NonDamagingFrame{},
      BeginFrameArgsForScrollJank{.frame_time = MillisSinceEpoch(50),
                                  .interval = base::Milliseconds(16)},
      ScrollJankV4Result{
          .missed_vsyncs_per_reason = {0, 0, 7, 8},
          .vsyncs_since_previous_frame = 9,
          .running_delivery_cutoff = base::Milliseconds(11),
          .adjusted_delivery_cutoff = base::Milliseconds(12),
          .current_delivery_cutoff = base::Milliseconds(13),
          .first_scroll_update =
              ScrollJankV4Result::RealFirstScrollUpdate{
                  .actual_input_generation_ts = MillisSinceEpoch(20)},
          .presentation =
              ScrollJankV4Result::NonDamagingPresentation{
                  .extrapolated_presentation_ts = MillisSinceEpoch(60)},
      });

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(
      QueryTraceProcessor(kSlicesQuery),
      ElementsAreArray(QueryResult{
          {"name", "ts", "dur"},
          {"ScrollJankV4", "20000000", "40000000"},
          {"Real scroll update input generation", "20000000", "10000000"},
          {"Begin frame", "50000000", "0"},
          {"Extrapolated presentation", "60000000", "0"},
      }));
  EXPECT_THAT(
      QueryTraceProcessor(kScrollJankV4ArgsQuery),
      ElementsAreArray(QueryResult{
          {"key", "display_value"},
          {"scroll_jank_v4.adjusted_delivery_cutoff_us", "12000000"},
          {"scroll_jank_v4.current_delivery_cutoff_us", "13000000"},
          {"scroll_jank_v4.damage_type",
           "NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP"},
          {"scroll_jank_v4.is_janky", "true"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].jank_reason",
           "MISSED_VSYNC_AT_START_OF_FLING"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].missed_vsyncs",
           "7"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].jank_reason",
           "MISSED_VSYNC_DURING_FLING"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].missed_vsyncs",
           "8"},
          {"scroll_jank_v4.running_delivery_cutoff_us", "11000000"},
          {"scroll_jank_v4.updates.first_scroll_update_type", "REAL"},
          {"scroll_jank_v4.updates.real.abs_total_raw_delta_pixels", "5.0"},
          {"scroll_jank_v4.updates.real.first_event_latency_id", "99"},
          {"scroll_jank_v4.updates.real.max_abs_inertial_raw_delta_pixels",
           "4.0"},
          {"scroll_jank_v4.vsync_interval_us", "16000000"},
          {"scroll_jank_v4.vsyncs_since_previous_frame", "9"},
      }));
  ExpectThatAllTraceEventsAreOnTheSameTrack();
  ExpectThatAllSubEventsAreDescendantsOfMainTraceEvent();
}

TEST_F(ScrollJankV4RecorderTest,
       SyntheticDamagingFrameWithExtrapolatedInputTimestamp) {
  trace_processor_.StartTrace("input");

  ScrollJankV4TracingRecorder::RecordTraceEvents(
      ScrollUpdates(
          /* real= */ std::nullopt,
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(30),
                    .first_input_trace_id = TraceId(99)}),
      DamagingFrame{.presentation_ts = MillisSinceEpoch(60)},
      BeginFrameArgsForScrollJank{.frame_time = MillisSinceEpoch(50),
                                  .interval = base::Milliseconds(16)},
      ScrollJankV4Result{
          .missed_vsyncs_per_reason = {0, 7, 0, 8},
          .vsyncs_since_previous_frame = 9,
          .running_delivery_cutoff = base::Milliseconds(11),
          .adjusted_delivery_cutoff = base::Milliseconds(12),
          .current_delivery_cutoff = base::Milliseconds(13),
          .first_scroll_update =
              ScrollJankV4Result::SyntheticFirstScrollUpdate{
                  .extrapolated_input_generation_ts = MillisSinceEpoch(20)},
          .presentation =
              ScrollJankV4Result::DamagingPresentation{
                  .actual_presentation_ts = MillisSinceEpoch(60)},
      });

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(
      QueryTraceProcessor(kSlicesQuery),
      ElementsAreArray(QueryResult{
          {"name", "ts", "dur"},
          {"ScrollJankV4", "20000000", "40000000"},
          {"Extrapolated first synthetic scroll update input generation",
           "20000000", "0"},
          {"First synthetic scroll update original begin frame", "30000000",
           "0"},
          {"Begin frame", "50000000", "0"},
          {"Presentation", "60000000", "0"},
      }));
  EXPECT_THAT(
      QueryTraceProcessor(kScrollJankV4ArgsQuery),
      ElementsAreArray(QueryResult{
          {"key", "display_value"},
          {"scroll_jank_v4.adjusted_delivery_cutoff_us", "12000000"},
          {"scroll_jank_v4.current_delivery_cutoff_us", "13000000"},
          {"scroll_jank_v4.damage_type", "DAMAGING"},
          {"scroll_jank_v4.is_janky", "true"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].jank_reason",
           "MISSED_VSYNC_DURING_FAST_SCROLL"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[0].missed_vsyncs",
           "7"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].jank_reason",
           "MISSED_VSYNC_DURING_FLING"},
          {"scroll_jank_v4.missed_vsyncs_per_jank_reason[1].missed_vsyncs",
           "8"},
          {"scroll_jank_v4.running_delivery_cutoff_us", "11000000"},
          {"scroll_jank_v4.updates.first_scroll_update_type",
           "SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP"},
          {"scroll_jank_v4.updates.synthetic.first_event_latency_id", "99"},
          {"scroll_jank_v4.vsync_interval_us", "16000000"},
          {"scroll_jank_v4.vsyncs_since_previous_frame", "9"},
      }));
  ExpectThatAllTraceEventsAreOnTheSameTrack();
  ExpectThatAllSubEventsAreDescendantsOfMainTraceEvent();
}

// Minimum test case with as little data populated as possible.
TEST_F(ScrollJankV4RecorderTest,
       SyntheticNonDamagingFrameWithoutExtrapolatedTimestamps) {
  trace_processor_.StartTrace("input");

  ScrollJankV4TracingRecorder::RecordTraceEvents(
      ScrollUpdates(
          /* real= */ std::nullopt,
          Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(20)}),
      NonDamagingFrame{},
      BeginFrameArgsForScrollJank{.frame_time = MillisSinceEpoch(30),
                                  .interval = base::Milliseconds(16)},
      ScrollJankV4Result{
          .first_scroll_update =
              ScrollJankV4Result::SyntheticFirstScrollUpdate{
                  .extrapolated_input_generation_ts = std::nullopt},
          .presentation =
              ScrollJankV4Result::NonDamagingPresentation{
                  .extrapolated_presentation_ts = std::nullopt},
      });

  absl::Status status = trace_processor_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_THAT(QueryTraceProcessor(kSlicesQuery),
              ElementsAreArray(QueryResult{
                  {"name", "ts", "dur"},
                  {"ScrollJankV4", "20000000", "10000000"},
                  {"First synthetic scroll update original begin frame",
                   "20000000", "0"},
                  {"Begin frame", "30000000", "0"},
              }));
  EXPECT_THAT(QueryTraceProcessor(kScrollJankV4ArgsQuery),
              ElementsAreArray(QueryResult{
                  {"key", "display_value"},
                  {"scroll_jank_v4.damage_type",
                   "NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP"},
                  {"scroll_jank_v4.is_janky", "false"},
                  {"scroll_jank_v4.updates.first_scroll_update_type",
                   "SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP"},
                  {"scroll_jank_v4.updates.synthetic", "[NULL]"},
                  {"scroll_jank_v4.vsync_interval_us", "16000000"},
              }));
  ExpectThatAllTraceEventsAreOnTheSameTrack();
  ExpectThatAllSubEventsAreDescendantsOfMainTraceEvent();
}

}  // namespace
}  // namespace cc
