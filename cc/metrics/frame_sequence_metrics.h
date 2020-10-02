// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_METRICS_H_
#define CC_METRICS_FRAME_SEQUENCE_METRICS_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"

namespace cc {
class ThroughputUkmReporter;
class JankMetrics;

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
  kCanvas = 10,
  kJSAnimation = 11,
  kMaxType
};

class CC_EXPORT FrameSequenceMetrics {
 public:
  FrameSequenceMetrics(FrameSequenceTrackerType type,
                       ThroughputUkmReporter* ukm_reporter);
  ~FrameSequenceMetrics();

  FrameSequenceMetrics(const FrameSequenceMetrics&) = delete;
  FrameSequenceMetrics& operator=(const FrameSequenceMetrics&) = delete;

  enum class ThreadType { kMain, kCompositor, kUnknown };

  struct ThroughputData {
    static std::unique_ptr<base::trace_event::TracedValue> ToTracedValue(
        const ThroughputData& impl,
        const ThroughputData& main,
        ThreadType effective_thred);

    static bool CanReportHistogram(FrameSequenceMetrics* metrics,
                                   ThreadType thread_type,
                                   const ThroughputData& data);

    // Returns the dropped throughput in percent
    static int ReportDroppedFramePercentHistogram(FrameSequenceMetrics* metrics,
                                                  ThreadType thread_type,
                                                  int metric_index,
                                                  const ThroughputData& data);

    // Returns the missed deadline throughput in percent
    static int ReportMissedDeadlineFramePercentHistogram(
        FrameSequenceMetrics* metrics,
        ThreadType thread_type,
        int metric_index,
        const ThroughputData& data);

    void Merge(const ThroughputData& data) {
      frames_expected += data.frames_expected;
      frames_produced += data.frames_produced;
      frames_ontime += data.frames_ontime;
#if DCHECK_IS_ON()
      frames_processed += data.frames_processed;
      frames_received += data.frames_received;
#endif
    }

    int DroppedFramePercent() const {
      if (frames_expected == 0)
        return 0;
      return std::ceil(100 * (frames_expected - frames_produced) /
                       static_cast<double>(frames_expected));
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

#if DCHECK_IS_ON()
    // Tracks the number of frames that is either submitted or reported as no
    // damage.
    uint32_t frames_processed = 0;

    // Tracks the number of begin-frames that are received.
    uint32_t frames_received = 0;
#endif
  };

  void SetScrollingThread(ThreadType thread);

  using CustomReporter =
      base::OnceCallback<void(ThroughputData throughput_data)>;
  // Sets reporter callback for kCustom typed sequence.
  void SetCustomReporter(CustomReporter custom_reporter);

  // Returns the 'effective thread' for the metrics (i.e. the thread most
  // relevant for this metric).
  ThreadType GetEffectiveThread() const;

  void Merge(std::unique_ptr<FrameSequenceMetrics> metrics);
  bool HasEnoughDataForReporting() const;
  bool HasDataLeftForReporting() const;
  // Report related metrics: throughput, checkboarding...
  void ReportMetrics();

  ThroughputData& impl_throughput() { return impl_throughput_; }
  ThroughputData& main_throughput() { return main_throughput_; }
  void add_checkerboarded_frames(int64_t frames) {
    frames_checkerboarded_ += frames;
  }
  uint32_t frames_checkerboarded() const { return frames_checkerboarded_; }

  FrameSequenceTrackerType type() const { return type_; }
  ThroughputUkmReporter* ukm_reporter() const {
    return throughput_ukm_reporter_;
  }

  // Must be called before destructor.
  void ReportLeftoverData();

  void AdoptTrace(FrameSequenceMetrics* adopt_from);
  void AdvanceTrace(base::TimeTicks timestamp);

  void ComputeJank(FrameSequenceMetrics::ThreadType thread_type,
                   uint32_t frame_token,
                   base::TimeTicks presentation_time,
                   base::TimeDelta frame_interval);

  void NotifySubmitForJankReporter(FrameSequenceMetrics::ThreadType thread_type,
                                   uint32_t frame_token,
                                   uint32_t sequence_number);

  void NotifyNoUpdateForJankReporter(
      FrameSequenceMetrics::ThreadType thread_type,
      uint32_t sequence_number,
      base::TimeDelta frame_interval);

 private:
  const FrameSequenceTrackerType type_;

  // Tracks some data to generate useful trace events.
  struct TraceData {
    explicit TraceData(FrameSequenceMetrics* metrics);
    ~TraceData();
    FrameSequenceMetrics* metrics;
    base::TimeTicks last_timestamp = base::TimeTicks::Now();
    int frame_count = 0;
    bool enabled = false;
    void* trace_id = nullptr;

    void Advance(base::TimeTicks new_timestamp);
    void Terminate();
  } trace_data_{this};

  // Pointer to the reporter owned by the FrameSequenceTrackerCollection.
  ThroughputUkmReporter* const throughput_ukm_reporter_;

  ThroughputData impl_throughput_;
  ThroughputData main_throughput_;

  ThreadType scrolling_thread_ = ThreadType::kUnknown;

  // Tracks the number of produced frames that had some amount of
  // checkerboarding, and how many frames showed such checkerboarded frames.
  uint32_t frames_checkerboarded_ = 0;

  // Callback invoked to report metrics for kCustom typed sequence.
  CustomReporter custom_reporter_;

  std::unique_ptr<JankMetrics> jank_reporter_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_METRICS_H_
