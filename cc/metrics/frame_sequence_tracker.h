// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_TRACKER_H_
#define CC_METRICS_FRAME_SEQUENCE_TRACKER_H_

#include <deque>
#include <memory>
#include <sstream>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
struct BeginFrameArgs;
struct BeginFrameId;
}  // namespace viz

namespace cc {
// Tracks a sequence of frames to determine the throughput. It tracks this by
// tracking the vsync sequence-numbers (from |BeginFrameArgs::sequence_number|),
// and the presentation-timestamps (from |gfx::PresentationFeedback|).
// This object should be created through
// FrameSequenceTrackerCollection::CreateTracker() API.
class CC_EXPORT FrameSequenceTracker {
 public:
  enum class TerminationStatus {
    kActive,
    kScheduledForTermination,
    kReadyForTermination,
  };

  static const char* GetFrameSequenceTrackerTypeName(
      FrameSequenceTrackerType type);

  ~FrameSequenceTracker();

  FrameSequenceTracker(const FrameSequenceTracker&) = delete;
  FrameSequenceTracker& operator=(const FrameSequenceTracker&) = delete;

  // Notifies the tracker when the compositor thread starts to process a
  // BeginFrameArgs.
  void ReportBeginImplFrame(const viz::BeginFrameArgs& args);

  // Notifies the tracker when the compositor thread has finished processing a
  // BeginFrameArgs.
  void ReportFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

  // Notifies that frame production has currently paused. This is typically used
  // for interactive frame-sequences, e.g. during touch-scroll.
  void PauseFrameProduction();

  TerminationStatus termination_status() const { return termination_status_; }

  // Returns true if we should ask this tracker to report its throughput data.
  bool ShouldReportMetricsNow(const viz::BeginFrameArgs& args) const;

  FrameSequenceMetrics* metrics() { return metrics_.get(); }
  FrameSequenceTrackerType type() const { return metrics_->type(); }
  int custom_sequence_id() const { return custom_sequence_id_; }

  std::unique_ptr<FrameSequenceMetrics> TakeMetrics();

  // Called by the destructor of FrameSequenceTrackerCollection, asking its
  // |metrics_| to report.
  void CleanUp();

  // Called by FrameSorter, this delivers `frame_info` merging both Compositor
  // and Main thread updates for the given `args`. These are sorted by the
  // `viz::BeginFrameArgs::frame_id` so we do not have to perform our own
  // analysis of interleaving frames.
  void AddSortedFrame(const viz::BeginFrameArgs& args,
                      const FrameInfo& frame_info);

 private:
  friend class FrameSequenceTrackerCollection;
  friend class FrameSequenceTrackerTest;

  // Constructs a tracker for a typed sequence other than kCustom.
  explicit FrameSequenceTracker(FrameSequenceTrackerType type);
  // Constructs a tracker for a kCustom typed sequence.
  FrameSequenceTracker(int custom_sequence_id,
                       FrameSequenceMetrics::CustomReporter custom_reporter);

  void ScheduleTerminate();

  struct TrackedFrameData {
    // Represents the |BeginFrameArgs::source_id| and
    // |BeginFrameArgs::sequence_number| fields of the last processed
    // BeginFrameArgs.
    uint64_t previous_source = 0;
    uint64_t previous_sequence = 0;
  };

  bool ShouldIgnoreBeginFrameSource(uint64_t source_id) const;
  void ResetAllStateIfPaused();

  const int custom_sequence_id_;

  TerminationStatus termination_status_ = TerminationStatus::kActive;

  TrackedFrameData begin_impl_frame_data_;

  std::unique_ptr<FrameSequenceMetrics> metrics_;

  // Keeps track of whether the frame-states should be reset.
  bool reset_all_state_ = false;

  // Report the throughput metrics every 5 seconds.
  const base::TimeDelta time_delta_to_report_ = base::Seconds(5);

  // True when an impl-impl is not ended. A tracker is ready for termination
  // only when the last impl-frame is ended (ReportFrameEnd).
  bool is_inside_frame_ = false;

  // The args for the first frame that started after the tracker was created.
  viz::BeginFrameArgs first_begin_frame_args_;

  // Frame id of the last ended frame when the tracker is active.
  viz::BeginFrameId last_ended_frame_id_;
  // Frame id of the last sorted frame that the tracker was notified. If this is
  // at least equal to `last_ended_frame_id_` then we are no longer awaiting
  // frame feedback and can terminate immediately.
  viz::BeginFrameId last_sorted_frame_id_;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_TRACKER_H_
