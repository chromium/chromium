// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_METRICS_H_
#define CC_METRICS_FRAME_SEQUENCE_METRICS_H_

#include <bitset>
#include <cmath>
#include <memory>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_info.h"

namespace viz {
struct BeginFrameArgs;
}  // namespace viz

namespace cc {
class JankMetrics;
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

  struct ThroughputData {
    static std::unique_ptr<base::trace_event::TracedValue> ToTracedValue(
        const ThroughputData& impl,
        const ThroughputData& main,
        FrameInfo::SmoothEffectDrivingThread effective_thred);

    static bool CanReportHistogram(
        FrameSequenceMetrics* metrics,
        FrameInfo::SmoothEffectDrivingThread thread_type,
        const ThroughputData& data);

    // Returns the missed deadline throughput in percent
    static int ReportMissedDeadlineFramePercentHistogram(
        FrameSequenceMetrics* metrics,
        FrameInfo::SmoothEffectDrivingThread thread_type,
        int metric_index,
        const ThroughputData& data);

    static void ReportCheckerboardingHistogram(
        FrameSequenceMetrics* metrics,
        FrameInfo::SmoothEffectDrivingThread thread_type,
        int percent);

    void Merge(const ThroughputData& data) {
      frames_expected += data.frames_expected;
      frames_produced += data.frames_produced;
      frames_ontime += data.frames_ontime;
    }

    int MissedDeadlineFramePercent() const {
      if (frames_produced == 0)
        return 0;
      return std::ceil(100 * (frames_produced - frames_ontime) /
                       static_cast<double>(frames_produced));
    }

    // Tracks the number of frames that were expected to be shown during this
    // frame-sequence.
    uint32_t frames_expected = 0;

    // Tracks the number of frames that were actually presented to the user
    // during this frame-sequence.
    uint32_t frames_produced = 0;

    // Tracks the number of frames that were actually presented to the user
    // that didn't miss the vsync deadline during this frame-sequence.
    uint32_t frames_ontime = 0;
  };

  void SetScrollingThread(FrameInfo::SmoothEffectDrivingThread thread);

  struct CustomReportData {
    uint32_t frames_expected = 0;
    uint32_t frames_produced = 0;
    int jank_count = 0;
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

  ThroughputData& impl_throughput() { return impl_throughput_; }
  ThroughputData& main_throughput() { return main_throughput_; }
  void add_checkerboarded_frames(int64_t frames) {
    frames_checkerboarded_ += frames;
  }
  uint32_t frames_checkerboarded() const { return frames_checkerboarded_; }

  FrameSequenceTrackerType type() const { return type_; }

  // Must be called before destructor.
  void ReportLeftoverData();

  void AdoptTrace(FrameSequenceMetrics* adopt_from);
  void AdvanceTrace(base::TimeTicks timestamp);

  void ComputeJank(FrameInfo::SmoothEffectDrivingThread thread_type,
                   uint32_t frame_token,
                   base::TimeTicks presentation_time,
                   base::TimeDelta frame_interval);

  void NotifySubmitForJankReporter(
      FrameInfo::SmoothEffectDrivingThread thread_type,
      uint32_t frame_token,
      uint32_t sequence_number);

  void NotifyNoUpdateForJankReporter(
      FrameInfo::SmoothEffectDrivingThread thread_type,
      uint32_t sequence_number,
      base::TimeDelta frame_interval);

 private:
  const FrameSequenceTrackerType type_;

  // Tracks some data to generate useful trace events.
  struct TraceData {
    explicit TraceData(FrameSequenceMetrics* metrics);
    ~TraceData();
    raw_ptr<FrameSequenceMetrics> metrics;
    base::TimeTicks last_timestamp = base::TimeTicks::Now();
    int frame_count = 0;
    bool enabled = false;
    raw_ptr<void> trace_id = nullptr;

    void Advance(base::TimeTicks new_timestamp,
                 uint32_t expected,
                 uint32_t dropped);
    void Terminate();
  } trace_data_{this};

  // Track state for measuring the PercentDroppedFrames v3 metrics.
  struct {
    uint32_t frames_expected = 0;
    uint32_t frames_dropped = 0;
  } v3_;

  ThroughputData impl_throughput_;
  ThroughputData main_throughput_;

  FrameInfo::SmoothEffectDrivingThread scrolling_thread_ =
      FrameInfo::SmoothEffectDrivingThread::kUnknown;

  // Tracks the number of produced frames that had some amount of
  // checkerboarding, and how many frames showed such checkerboarded frames.
  uint32_t frames_checkerboarded_ = 0;

  // Callback invoked to report metrics for kCustom typed sequence.
  CustomReporter custom_reporter_;

  std::unique_ptr<JankMetrics> jank_reporter_;
};

bool ShouldReportForAnimation(FrameSequenceTrackerType sequence_type,
                              FrameInfo::SmoothEffectDrivingThread thread_type);

bool ShouldReportForInteraction(
    FrameSequenceTrackerType sequence_type,
    FrameInfo::SmoothEffectDrivingThread reporting_thread_type,
    FrameInfo::SmoothEffectDrivingThread metrics_effective_thread_type);

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_METRICS_H_
