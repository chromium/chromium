// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_METRICS_H_
#define CC_METRICS_FRAME_SEQUENCE_METRICS_H_

#include <bitset>
#include <cmath>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_info.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {
struct BeginFrameArgs;
}  // namespace viz

namespace cc {
struct FrameInfo;

enum class FrameSequenceTrackerType {
  // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
  // add them at the end and increment kMaxType.
  kCompositorAnimation = 0,
  kMainThreadAnimation = 1,
  kPinchZoom = 2,
  kRAF = 3,
  kTouchScroll = 4,
  kVideo = 6,
  kWheelScroll = 7,
  kScrollbarScroll = 8,
  kCustom = 9,  // Note that the metrics for kCustom are not reported on UMA,
                // and instead are dispatched back to the LayerTreeHostClient.
  kCanvasAnimation = 10,
  kJSAnimation = 11,
  kSETMainThreadAnimation = 12,
  kSETCompositorAnimation = 13,
  kMaxType
};

using ActiveTrackers =
    std::bitset<static_cast<size_t>(FrameSequenceTrackerType::kMaxType)>;

inline bool IsScrollActive(const ActiveTrackers& trackers) {
  return trackers.test(
             static_cast<size_t>(FrameSequenceTrackerType::kWheelScroll)) ||
         trackers.test(
             static_cast<size_t>(FrameSequenceTrackerType::kTouchScroll)) ||
         trackers.test(
             static_cast<size_t>(FrameSequenceTrackerType::kScrollbarScroll));
}

inline bool HasMainThreadAnimation(const ActiveTrackers& trackers) {
  return trackers.test(static_cast<size_t>(
             FrameSequenceTrackerType::kMainThreadAnimation)) ||
         trackers.test(
             static_cast<size_t>(FrameSequenceTrackerType::kCanvasAnimation)) ||
         trackers.test(
             static_cast<size_t>(FrameSequenceTrackerType::kJSAnimation)) ||
         trackers.test(static_cast<size_t>(FrameSequenceTrackerType::kRAF));
}

inline bool HasCompositorThreadAnimation(const ActiveTrackers& trackers) {
  return trackers.test(
      static_cast<size_t>(FrameSequenceTrackerType::kCompositorAnimation));
}

class CC_EXPORT FrameSequenceMetrics {
 public:
  explicit FrameSequenceMetrics(FrameSequenceTrackerType type);
  ~FrameSequenceMetrics();

  FrameSequenceMetrics(const FrameSequenceMetrics&) = delete;
  FrameSequenceMetrics& operator=(const FrameSequenceMetrics&) = delete;

  void SetScrollingThread(FrameInfo::SmoothEffectDrivingThread thread);

  struct Jank {
    // The start time of a jank.
    base::TimeTicks start_time;
    // The duration of a jank.
    base::TimeDelta duration;
  };

  struct CC_EXPORT CustomReportData {
    CustomReportData();
    CustomReportData(uint32_t frames_expected,
                     uint32_t frames_dropped,
                     uint32_t jank_count,
                     std::vector<Jank> janks);

    CustomReportData(const CustomReportData&);
    CustomReportData& operator=(const CustomReportData&);

    ~CustomReportData();

    uint32_t frames_expected_v3 = 0;
    uint32_t frames_dropped_v3 = 0;
    uint32_t jank_count_v3 = 0;
    std::vector<Jank> janks;
  };
  using CustomReporter = base::OnceCallback<void(const CustomReportData& data)>;
  // Sets reporter callback for kCustom typed sequence.
  void SetCustomReporter(CustomReporter custom_reporter);

  // Returns the 'effective thread' for the metrics (i.e. the thread most
  // relevant for this metric).
  FrameInfo::SmoothEffectDrivingThread GetEffectiveThread() const;

  void Merge(std::unique_ptr<FrameSequenceMetrics> metrics);
  bool HasEnoughDataForReporting() const;
  bool HasDataLeftForReporting() const;
  // Report related metrics: throughput, checkboarding...
  void ReportMetrics();

  void AddSortedFrame(const viz::BeginFrameArgs& args,
                      const FrameInfo& frame_info);

  FrameSequenceTrackerType type() const { return type_; }

  // Must be called before destructor.
  void ReportLeftoverData();

  void AdoptTrace(FrameSequenceMetrics* adopt_from);

 private:
  friend class FrameSequenceMetricsTest;
  friend class FrameSequenceTrackerTest;
  // FrameInfo is a merger of two threads' frame production. We should only look
  // at the `final_state`, `last_presented_termination_time` and
  // `termination_time` for the GetEffectiveThread.
  void CalculateJankV3(const viz::BeginFrameArgs& args,
                       const FrameInfo& frame_info,
                       FrameInfo::FrameFinalState final_state,
                       base::TimeTicks last_presented_termination_time,
                       base::TimeTicks termination_time);
  void CalculateCheckerboarding(const FrameInfo& frame_info,
                                FrameInfo::FrameFinalState final_state);
  void IncrementJankIdleTimeV3(base::TimeTicks last_presented_termination_time,
                               base::TimeTicks termination_time);
  void TraceJankV3(uint64_t sequence_number,
                   base::TimeTicks last_termination_time,
                   base::TimeTicks termination_time);

  const FrameSequenceTrackerType type_;

  // Track state for measuring the various Graphics.Smoothness V3 metrics.
  struct V3 {
    V3();
    ~V3();
    uint32_t frames_expected = 0;
    uint32_t frames_dropped = 0;
    uint32_t frames_missing_content = 0;
    uint32_t no_update_count = 0;
    uint32_t jank_count = 0;
    viz::BeginFrameArgs last_begin_frame_args;
    FrameInfo last_frame;
    FrameInfo last_presented_frame;
    base::TimeDelta last_frame_delta;
    base::TimeDelta no_update_duration;
    // Note: janks are only recorded for kCustom types sequences
    std::vector<Jank> janks;
  } v3_;

  struct V4 {
    uint32_t frames_checkerboarded = 0;
    uint32_t frames_checkerboarded_need_raster = 0;
    uint32_t frames_checkerboarded_need_record = 0;
  } v4_;

  // Tracks some data to generate useful trace events.
  struct TraceData {
    explicit TraceData(FrameSequenceMetrics* metrics);
    ~TraceData();
    raw_ptr<FrameSequenceMetrics> metrics;
    uint64_t last_presented_sequence_number = 0u;
    base::TimeTicks last_timestamp = base::TimeTicks::Now();
    int frame_count = 0;
    bool enabled = false;
    uint64_t trace_id = 0u;

    void Advance(base::TimeTicks start_timestamp,
                 base::TimeTicks new_timestamp,
                 uint32_t expected,
                 uint32_t dropped,
                 uint64_t sequence_number,
                 const char* histogram_name);
    void Terminate(const V3& v3,
                   const V4& v4,
                   FrameInfo::SmoothEffectDrivingThread effective_thread);
  } trace_data_{this};

  FrameInfo::SmoothEffectDrivingThread scrolling_thread_ =
      FrameInfo::SmoothEffectDrivingThread::kUnknown;

  // Callback invoked to report metrics for kCustom typed sequence.
  CustomReporter custom_reporter_;
};

bool ShouldReportForAnimation(FrameSequenceTrackerType sequence_type,
                              FrameInfo::SmoothEffectDrivingThread thread_type);

bool ShouldReportForInteraction(
    FrameSequenceTrackerType sequence_type,
    FrameInfo::SmoothEffectDrivingThread reporting_thread_type,
    FrameInfo::SmoothEffectDrivingThread metrics_effective_thread_type);

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_METRICS_H_
