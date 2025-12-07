// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_tracing_recorder.h"

#include <variant>

#include "base/check.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace cc {
namespace {

using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;

constexpr char kTracingCategory[] = "cc,benchmark,input,input.scrolling";

bool IsTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTracingCategory, &enabled);
  return enabled;
}

constexpr perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::JankReason
ToProtoEnum(JankReason reason) {
#define CASE(reason, proto_reason)                                      \
  case JankReason::reason:                                              \
    return perfetto::protos::pbzero::EventLatency::ScrollJankV4Result:: \
        JankReason::proto_reason
  switch (reason) {
    CASE(kMissedVsyncDueToDeceleratingInputFrameDelivery,
         MISSED_VSYNC_DUE_TO_DECELERATING_INPUT_FRAME_DELIVERY);
    CASE(kMissedVsyncDuringFastScroll, MISSED_VSYNC_DURING_FAST_SCROLL);
    CASE(kMissedVsyncAtStartOfFling, MISSED_VSYNC_AT_START_OF_FLING);
    CASE(kMissedVsyncDuringFling, MISSED_VSYNC_DURING_FLING);
  }
#undef CASE
}

void PopulateScrollUpdatesRealProto(
    const ScrollUpdates::Real& real,
    perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::ScrollUpdates::
        Real& out) {
  if (real.first_input_trace_id.has_value()) {
    out.set_first_event_latency_id(real.first_input_trace_id->value());
  }
  out.set_abs_total_raw_delta_pixels(real.abs_total_raw_delta_pixels);
  if (real.has_inertial_input) {
    out.set_max_abs_inertial_raw_delta_pixels(
        real.max_abs_inertial_raw_delta_pixels);
  }
}

void PopulateScrollUpdatesSyntheticProto(
    const ScrollUpdates::Synthetic& synthetic,
    perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::ScrollUpdates::
        Synthetic& out) {
  if (synthetic.first_input_trace_id.has_value()) {
    out.set_first_event_latency_id(synthetic.first_input_trace_id->value());
  }
}

void PopulateScrollUpdatesProto(
    const ScrollUpdates& updates,
    const ScrollJankV4Result& result,
    perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::ScrollUpdates&
        out) {
  if (updates.real().has_value()) {
    PopulateScrollUpdatesRealProto(*updates.real(), *out.set_real());
  }
  if (updates.synthetic().has_value()) {
    PopulateScrollUpdatesSyntheticProto(*updates.synthetic(),
                                        *out.set_synthetic());
  }
  out.set_first_scroll_update_type(std::visit(
      absl::Overload{
          [&](const ScrollJankV4Result::RealFirstScrollUpdate& real) {
            return perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::
                ScrollUpdates::FirstScrollUpdateType::REAL;
          },
          [&](const ScrollJankV4Result::SyntheticFirstScrollUpdate& synthetic) {
            if (synthetic.extrapolated_input_generation_ts.has_value()) {
              return perfetto::protos::pbzero::EventLatency::
                  ScrollJankV4Result::ScrollUpdates::FirstScrollUpdateType::
                      SYNTHETIC_WITH_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP;
            }
            return perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::
                ScrollUpdates::FirstScrollUpdateType::
                    SYNTHETIC_WITHOUT_EXTRAPOLATED_INPUT_GENERATION_TIMESTAMP;
          }},
      result.first_scroll_update));
}

void PopulateScrollJankV4ResultProto(
    const ScrollUpdates& updates,
    const ScrollDamage& damage,
    const BeginFrameArgsForScrollJank& args,
    const ScrollJankV4Result& result,
    perfetto::protos::pbzero::EventLatency::ScrollJankV4Result& out) {
  PopulateScrollUpdatesProto(updates, result, *out.set_updates());

  out.set_damage_type(std::visit(
      absl::Overload{
          [&](const ScrollJankV4Result::DamagingPresentation& damaging) {
            return perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::
                DamageType::DAMAGING;
          },
          [&](const ScrollJankV4Result::NonDamagingPresentation& non_damaging) {
            if (non_damaging.extrapolated_presentation_ts.has_value()) {
              return perfetto::protos::pbzero::EventLatency::
                  ScrollJankV4Result::DamageType::
                      NON_DAMAGING_WITH_EXTRAPOLATED_PRESENTATION_TIMESTAMP;
            }
            return perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::
                DamageType::
                    NON_DAMAGING_WITHOUT_EXTRAPOLATED_PRESENTATION_TIMESTAMP;
          }},
      result.presentation));

  out.set_vsync_interval_us(args.interval.InNanoseconds());

  if (result.vsyncs_since_previous_frame.has_value()) {
    out.set_vsyncs_since_previous_frame(*result.vsyncs_since_previous_frame);
  }

  bool is_janky = false;
  for (int i = 0; i <= static_cast<int>(JankReason::kMaxValue); i++) {
    int missed_vsyncs_for_reason = result.missed_vsyncs_per_reason[i];
    if (missed_vsyncs_for_reason == 0) {
      continue;
    }
    is_janky = true;
    auto* entry = out.add_missed_vsyncs_per_jank_reason();
    entry->set_jank_reason(ToProtoEnum(static_cast<JankReason>(i)));
    entry->set_missed_vsyncs(missed_vsyncs_for_reason);
  }
  out.set_is_janky(is_janky);

  if (result.running_delivery_cutoff.has_value()) {
    out.set_running_delivery_cutoff_us(
        result.running_delivery_cutoff->InNanoseconds());
  }
  if (result.adjusted_delivery_cutoff.has_value()) {
    out.set_adjusted_delivery_cutoff_us(
        result.adjusted_delivery_cutoff->InNanoseconds());
  }
  if (result.current_delivery_cutoff.has_value()) {
    out.set_current_delivery_cutoff_us(
        result.current_delivery_cutoff->InNanoseconds());
  }
}

void RecordSubEvents(const ScrollUpdates& updates,
                     const ScrollDamage& damage,
                     const BeginFrameArgsForScrollJank& args,
                     const ScrollJankV4Result& result,
                     const perfetto::Track& trace_track) {
  // Scroll updates.
  if (const auto* synthetic =
          std::get_if<ScrollJankV4Result::SyntheticFirstScrollUpdate>(
              &result.first_scroll_update);
      synthetic && synthetic->extrapolated_input_generation_ts.has_value()) {
    TRACE_EVENT_INSTANT(
        kTracingCategory,
        "Extrapolated first synthetic scroll update input generation",
        trace_track, *synthetic->extrapolated_input_generation_ts);
  }
  if (updates.real().has_value()) {
    TRACE_EVENT_BEGIN(kTracingCategory, "Real scroll update input generation",
                      trace_track, updates.real()->first_input_generation_ts);
    TRACE_EVENT_END(kTracingCategory, trace_track,
                    updates.real()->last_input_generation_ts);
  }
  if (updates.synthetic().has_value()) {
    TRACE_EVENT_INSTANT(
        kTracingCategory, "First synthetic scroll update original begin frame",
        trace_track, updates.synthetic()->first_input_begin_frame_ts);
  }

  // Begin frame.
  TRACE_EVENT_INSTANT(kTracingCategory, "Begin frame", trace_track,
                      args.frame_time);

  // Presentation.
  std::visit(
      absl::Overload{
          [&](const ScrollJankV4Result::DamagingPresentation& damaging) {
            TRACE_EVENT_INSTANT(kTracingCategory, "Presentation", trace_track,
                                damaging.actual_presentation_ts);
          },
          [&](const ScrollJankV4Result::NonDamagingPresentation& non_damaging) {
            if (non_damaging.extrapolated_presentation_ts.has_value()) {
              TRACE_EVENT_INSTANT(kTracingCategory, "Extrapolated presentation",
                                  trace_track,
                                  *non_damaging.extrapolated_presentation_ts);
            }
          }},
      result.presentation);
}

}  // namespace

// static
void ScrollJankV4TracingRecorder::RecordTraceEvents(
    const ScrollUpdates& updates,
    const ScrollDamage& damage,
    const BeginFrameArgsForScrollJank& args,
    const ScrollJankV4Result& result) {
  if (!IsTracingEnabled()) {
    return;
  }

  // Consecutive "ScrollJankV4" trace events are likely to overlap, so create a
  // new track for each event.
  const perfetto::Track trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());

  base::TimeTicks start_ts = std::visit(
      absl::Overload{
          [](const ScrollJankV4Result::RealFirstScrollUpdate& real) {
            return real.actual_input_generation_ts;
          },
          [&](const ScrollJankV4Result::SyntheticFirstScrollUpdate& synthetic) {
            if (synthetic.extrapolated_input_generation_ts.has_value()) {
              return *synthetic.extrapolated_input_generation_ts;
            }
            // `result.first_scroll_update` can only be empty if this is a
            // synthetic frame.
            CHECK(updates.synthetic().has_value());
            return updates.synthetic()->first_input_begin_frame_ts;
          }},
      result.first_scroll_update);
  TRACE_EVENT_BEGIN(
      kTracingCategory, "ScrollJankV4", trace_track, start_ts,
      [&](perfetto::EventContext context) {
        auto* event =
            context.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        PopulateScrollJankV4ResultProto(updates, damage, args, result,
                                        *event->set_scroll_jank_v4());
      });

  RecordSubEvents(updates, damage, args, result, trace_track);

  base::TimeTicks end_ts = std::visit(
      absl::Overload{
          [&](const ScrollJankV4Result::DamagingPresentation& damaging) {
            return damaging.actual_presentation_ts;
          },
          [&](const ScrollJankV4Result::NonDamagingPresentation& non_damaging) {
            if (non_damaging.extrapolated_presentation_ts.has_value()) {
              return *non_damaging.extrapolated_presentation_ts;
            }
            return args.frame_time;
          }},
      result.presentation);
  TRACE_EVENT_END(kTracingCategory, trace_track, end_ts);
}

}  // namespace cc
