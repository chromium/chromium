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
struct BeginFrameAck;
struct BeginFrameArgs;
struct BeginFrameId;
}  // namespace viz

namespace cc {
// Tracks a sequence of frames to determine the throughput. It tracks this by
// tracking the vsync sequence-numbers (from |BeginFrameArgs::sequence_number|),
// and the presentation-timestamps (from |gfx::PresentationFeedback|). It also
// tracks which frames were expected to include update from the main-thread, and
// which presented frames did include updates from the main-thread.
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

  // Notifies the tracker when a BeginFrameArgs is dispatched to the main
  // thread.
  void ReportBeginMainFrame(const viz::BeginFrameArgs& args);

  void ReportMainFrameProcessed(const viz::BeginFrameArgs& args);

  // Notifies the tracker when the compositor submits a CompositorFrame.
  // |origin_args| represents the BeginFrameArgs that triggered the update from
  // the main-thread.
  void ReportSubmitFrame(uint32_t frame_token,
                         bool has_missing_content,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);

  void ReportFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

  // Notifies the tracker of the presentation-feedback of a previously submitted
  // CompositorFrame with |frame_token|.
  void ReportFramePresented(uint32_t frame_token,
                            const gfx::PresentationFeedback& feedback);

  // Notifies the tracker that a CompositorFrame is not going to be submitted
  // for a particular BeginFrameArgs because it did not cause any damage (visual
  // change). Note that if a begin-main-frame was dispatched, then a separate
  // call to |ReportMainFrameCausedNoDamage()| is made to notify that the
  // main-thread did not cause any damage/updates.
  void ReportImplFrameCausedNoDamage(const viz::BeginFrameAck& ack);

  // Notifies the tracker that a |BeginFrameArgs| either was not dispatched to
  // the main-thread (because it did not ask for it), or that a |BeginFrameArgs|
  // that was dispatched to the main-thread did not cause any updates/damage.
  void ReportMainFrameCausedNoDamage(const viz::BeginFrameArgs& args,
                                     bool aborted);

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

  FrameSequenceMetrics::ThroughputData& impl_throughput() {
    return metrics_->impl_throughput();
  }
  FrameSequenceMetrics::ThroughputData& main_throughput() {
    return metrics_->main_throughput();
  }

  void ScheduleTerminate();

  struct TrackedFrameData {
    // Represents the |BeginFrameArgs::source_id| and
    // |BeginFrameArgs::sequence_number| fields of the last processed
    // BeginFrameArgs.
    uint64_t previous_source = 0;
    uint64_t previous_sequence = 0;

    // The difference in |BeginFrameArgs::sequence_number| fields of the last
    // two processed BeginFrameArgs.
    uint32_t previous_sequence_delta = 0;
  };

  struct CheckerboardingData {
    CheckerboardingData();
    ~CheckerboardingData();

    // Tracks whether the last presented frame had checkerboarding. This is used
    // to track how many vsyncs showed frames with checkerboarding.
    bool last_frame_had_checkerboarding = false;

    base::TimeTicks last_frame_timestamp;

    // A list of frame-tokens that had checkerboarding.
    base::circular_deque<uint32_t> frames;
  };

  void UpdateTrackedFrameData(TrackedFrameData* frame_data,
                              uint64_t source_id,
                              uint64_t sequence_number,
                              uint64_t throttled_frame_count);

  bool ShouldIgnoreBeginFrameSource(uint64_t source_id) const;

  bool ShouldIgnoreSequence(uint64_t sequence_number) const;

  const int custom_sequence_id_;

  TerminationStatus termination_status_ = TerminationStatus::kActive;

  TrackedFrameData begin_impl_frame_data_;
  TrackedFrameData begin_main_frame_data_;

  std::unique_ptr<FrameSequenceMetrics> metrics_;

  CheckerboardingData checkerboarding_;

  // Tracks the list of frame-tokens for compositor-frames that included new
  // updates from the main-thread, whose presentation-feedback have not been
  // received yet. When the presentation-feedback for a frame is received, the
  // corresponding frame-token is removed from this collection.
  base::circular_deque<uint32_t> main_frames_;

  // Keeps track of the sequence-number of the first received begin-main-frame.
  // This is used to ignore submitted frames that include updates from earlier
  // begin-main-frames.
  uint64_t first_received_main_sequence_ = 0;

  // Keeps track of the first submitted compositor-frame. This is used to ignore
  // reports from frames that were submitted before this tracker had been
  // created.
  uint32_t first_submitted_frame_ = 0;

  // Keeps track of the latest submitted compositor-frame, so that it can
  // determine when it has received presentation-feedback for submitted frames.
  // This is used to decide when to terminate this FrameSequenceTracker object.
  uint32_t last_submitted_frame_ = 0;

  // Keeps track of the begin-main-frames that need to be processed. There can
  // be multiple in-flight, as BeginMainFrame to ReadyToCommit can be longer
  // than one `viz::BeginFrameArgs.interval`. When this occurs the Compositor
  // can send the `n+1` sequence_number, only for the Commit for `n` to arrive
  // and lead to frame production.
  std::deque<uint64_t> pending_main_sequences_;
  uint64_t aborted_main_frame_ = 0;
  uint64_t no_damage_draw_main_frames_ = 0;

  // Keeps track of the last sequence-number that produced a frame from the
  // main-thread.
  uint64_t last_submitted_main_sequence_ = 0;

  // Keeps track of the last sequence-number that produced a frame that did not
  // have any damage from the main-thread.
  uint64_t last_no_main_damage_sequence_ = 0;

  // The time when this tracker is created, or the time when it was previously
  // scheduled to report histogram.
  base::TimeTicks first_frame_timestamp_;

  // Tracks the presentation timestamp of the previous frame.
  base::TimeTicks last_frame_presentation_timestamp_;

  // Keeps track of whether the impl-frame being processed did not have any
  // damage from the compositor (i.e. 'impl damage').
  bool frame_had_no_compositor_damage_ = false;

  // Keeps track of whether a CompositorFrame is submitted during the frame.
  bool compositor_frame_submitted_ = false;
  bool submitted_frame_had_new_main_content_ = false;

  // Keeps track of whether the frame-states should be reset.
  bool reset_all_state_ = false;

  // A frame that is ignored at ReportSubmitFrame should never be presented.
  // TODO(xidachen): this should not be necessary. Some webview tests seem to
  // present a frame even if it is ignored by ReportSubmitFrame.
  base::flat_set<uint32_t> ignored_frame_tokens_;

  // Report the throughput metrics every 5 seconds.
  const base::TimeDelta time_delta_to_report_ = base::Seconds(5);

  uint64_t last_started_impl_sequence_ = 0;
  uint64_t last_processed_impl_sequence_ = 0;

  uint64_t last_processed_main_sequence_ = 0;
  uint64_t last_processed_main_sequence_latency_ = 0;

  // Handle off-screen main damage case. In this case, the sequence is typically
  // like: b(1)B(0,1)E(1)n(1)e(1)b(2)n(2)e(2)...b(10)E(2)B(10,10)n(10)e(10).
  // Note that between two 'E's, all the impl frames caused no damage, and
  // no main frames were submitted or caused no damage.
  bool had_impl_frame_submitted_between_commits_ = false;
  uint64_t previous_begin_main_sequence_ = 0;
  // TODO(xidachen): remove this one.
  uint64_t current_begin_main_sequence_ = 0;

  // True when an impl-impl is not ended. A tracker is ready for termination
  // only when the last impl-frame is ended (ReportFrameEnd).
  bool is_inside_frame_ = false;

#if DCHECK_IS_ON()
  // This stringstream represents a sequence of frame reporting activities on
  // the current tracker. Each letter can be one of the following:
  // {'B', 'N', 'b', 'n', 'S', 'P'}, where
  // 'B' = ReportBeginMainFrame(), 'N' = ReportMainFrameCausedNoDamage(),
  // 'b' = ReportBeginImplFrame(), 'n' = ReportMainFrameCausedNoDamage(),
  // 'S' = ReportSubmitFrame() and 'P' = ReportFramePresented().
  // Note that |frame_sequence_trace_| is only defined and populated
  // when DCHECK is on.
  std::stringstream frame_sequence_trace_;

  // |frame_sequence_trace_| can be very long, in some cases we just need a
  // substring of it. This var tells us how many chars can be ignored from the
  // beginning of that debug string.
  unsigned ignored_trace_char_count_ = 0;

  // If ReportBeginImplFrame is never called on a arg, then ReportBeginMainFrame
  // should ignore that arg.
  base::flat_set<viz::BeginFrameId> impl_frames_;
#endif
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_TRACKER_H_
