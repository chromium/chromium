// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_
#define CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace viz {
struct BeginFrameArgs;
}  // namespace viz

namespace cc {
class FrameSequenceTracker;
class CompositorFrameReportingController;
class UkmManager;

// Map of kCustom tracker results keyed by a sequence id.
using CustomTrackerResults =
    base::flat_map<int, FrameSequenceMetrics::CustomReportData>;

typedef uint16_t ActiveFrameSequenceTrackers;

// Used for notifying attached FrameSequenceTracker's of begin-frames and
// submitted frames.
class CC_EXPORT FrameSequenceTrackerCollection {
 public:
  FrameSequenceTrackerCollection(
      bool is_single_threaded,
      CompositorFrameReportingController* frame_reporting_controller);
  ~FrameSequenceTrackerCollection();

  FrameSequenceTrackerCollection(const FrameSequenceTrackerCollection&) =
      delete;
  FrameSequenceTrackerCollection& operator=(
      const FrameSequenceTrackerCollection&) = delete;

  // Creates a new tracker for the specified sequence-type if one doesn't
  // already exist. Returns the associated FrameSequenceTracker instance.
  FrameSequenceTracker* StartSequence(FrameSequenceTrackerType type);
  FrameSequenceTracker* StartScrollSequence(
      FrameSequenceTrackerType type,
      FrameInfo::SmoothEffectDrivingThread scrolling_thread);

  // Schedules |tracker| for destruction. This is preferred instead of outright
  // desrtruction of the tracker, since this ensures that the actual tracker
  // instance is destroyed *after* the presentation-feedbacks have been received
  // for all submitted frames.
  void StopSequence(FrameSequenceTrackerType type);

  // Creates a kCustom tracker for the given sequence id. It is an error and
  // DCHECKs if there is already a tracker associated with the sequence id.
  void StartCustomSequence(int sequence_id);

  // Schedules the kCustom tracker representing |sequence_id| for destruction.
  // It is a no-op if there is no tracker associated with the sequence id.
  // Similar to StopSequence above, the tracker instance is destroyed *after*
  // the presentation feedbacks have been received for all submitted frames.
  void StopCustomSequence(int sequence_id);

  // Removes all trackers. This also immediately destroys all trackers that had
  // been scheduled for destruction, even if there are pending
  // presentation-feedbacks. This is typically used if the client no longer
  // expects to receive presentation-feedbacks for the previously submitted
  // frames (e.g. when the gpu process dies).
  void ClearAll();

  // Notifies all trackers of various events.
  void NotifyBeginImplFrame(const viz::BeginFrameArgs& args);
  void NotifyPauseFrameProduction();
  void NotifyFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

  // Return the type of each active frame tracker, encoded into a 16 bit
  // integer with the bit at each position corresponding to the enum value of
  // each type.
  ActiveFrameSequenceTrackers FrameSequenceTrackerActiveTypes() const;

  FrameSequenceTracker* GetRemovalTrackerForTesting(
      FrameSequenceTrackerType type);

  void SetUkmManager(UkmManager* manager);

  using NotifyCustomerTrackerResutlsCallback =
      base::RepeatingCallback<void(const CustomTrackerResults&)>;
  void set_custom_tracker_results_added_callback(
      NotifyCustomerTrackerResutlsCallback callback) {
    custom_tracker_results_added_callback_ = std::move(callback);
  }

  void AddSortedFrame(const viz::BeginFrameArgs& args,
                      const FrameInfo& frame_info);

 private:
  friend class FrameSequenceTrackerTest;

  FrameSequenceTracker* StartSequenceInternal(
      FrameSequenceTrackerType type,
      FrameInfo::SmoothEffectDrivingThread scrolling_thread);

  void RecreateTrackers(const viz::BeginFrameArgs& args);
  // Destroy the trackers that are ready to be terminated.
  void DestroyTrackers();

  // Ask all trackers to report their metrics if there is any, must be the first
  // thing in the destructor.
  void CleanUp();

  // Adds collected metrics data for |custom_sequence_id| to be picked up via
  // TakeCustomTrackerResults() below.
  void AddCustomTrackerResult(
      int custom_sequence_id,
      const FrameSequenceMetrics::CustomReportData& data);

  const bool is_single_threaded_;

  // The callsite can use the type to manipulate the tracker.
  base::flat_map<
      std::pair<FrameSequenceTrackerType, FrameInfo::SmoothEffectDrivingThread>,
      std::unique_ptr<FrameSequenceTracker>>
      frame_trackers_;

  // Custom trackers are keyed by a custom sequence id.
  base::flat_map<int, std::unique_ptr<FrameSequenceTracker>>
      custom_frame_trackers_;

  // Called when throughput metrics are available for custom trackers added by
  // |AddCustomTrackerResult()|.
  NotifyCustomerTrackerResutlsCallback custom_tracker_results_added_callback_;

  std::vector<std::unique_ptr<FrameSequenceTracker>> removal_trackers_;
  const raw_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;

  base::flat_map<
      std::pair<FrameSequenceTrackerType, FrameInfo::SmoothEffectDrivingThread>,
      std::unique_ptr<FrameSequenceMetrics>>
      accumulated_metrics_;

  // Tracks how many smoothness effects are driven by each thread.
  size_t main_thread_driving_smoothness_ = 0;
  size_t compositor_thread_driving_smoothness_ = 0;
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_TRACKER_COLLECTION_H_
