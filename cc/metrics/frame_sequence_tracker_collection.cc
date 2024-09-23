// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker_collection.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/frame_sequence_tracker.h"

namespace cc {

namespace {

using ThreadType = FrameInfo::SmoothEffectDrivingThread;

bool IsScrollType(FrameSequenceTrackerType type) {
  return type == FrameSequenceTrackerType::kTouchScroll ||
         type == FrameSequenceTrackerType::kWheelScroll ||
         type == FrameSequenceTrackerType::kScrollbarScroll;
}

}  // namespace

FrameSequenceTrackerCollection::FrameSequenceTrackerCollection(
    bool is_single_threaded,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : is_single_threaded_(is_single_threaded),
      compositor_frame_reporting_controller_(
          compositor_frame_reporting_controller) {}

FrameSequenceTrackerCollection::~FrameSequenceTrackerCollection() {
  CleanUp();
  frame_trackers_.clear();
  removal_trackers_.clear();
  custom_frame_trackers_.clear();
  accumulated_metrics_.clear();
}

FrameSequenceTracker* FrameSequenceTrackerCollection::StartSequenceInternal(
    FrameSequenceTrackerType type,
    FrameInfo::SmoothEffectDrivingThread scrolling_thread) {
  DCHECK_NE(FrameSequenceTrackerType::kCustom, type);
  if (is_single_threaded_)
    return nullptr;
  auto key = std::make_pair(type, scrolling_thread);
  if (frame_trackers_.contains(key))
    return frame_trackers_[key].get();

  auto tracker = base::WrapUnique(new FrameSequenceTracker(type));
  frame_trackers_[key] = std::move(tracker);

  if (compositor_frame_reporting_controller_)
    compositor_frame_reporting_controller_->AddActiveTracker(type);

  auto* metrics = frame_trackers_[key]->metrics();
  if (accumulated_metrics_.contains(key)) {
    metrics->AdoptTrace(accumulated_metrics_[key].get());
  }
  if (IsScrollType(type)) {
    DCHECK_NE(scrolling_thread, ThreadType::kUnknown);
    metrics->SetScrollingThread(scrolling_thread);
    compositor_frame_reporting_controller_->SetScrollingThread(
        scrolling_thread);
  }

  if (metrics->GetEffectiveThread() == ThreadType::kCompositor) {
    if (compositor_frame_reporting_controller_ &&
        compositor_thread_driving_smoothness_ == 0) {
      compositor_frame_reporting_controller_->SetThreadAffectsSmoothness(
          ThreadType::kCompositor, true);
    }
    ++compositor_thread_driving_smoothness_;
  } else {
    DCHECK_EQ(metrics->GetEffectiveThread(), ThreadType::kMain);
    if (compositor_frame_reporting_controller_ &&
        main_thread_driving_smoothness_ == 0) {
      compositor_frame_reporting_controller_->SetThreadAffectsSmoothness(
          ThreadType::kMain, true);
    }
    ++main_thread_driving_smoothness_;
  }
  return frame_trackers_[key].get();
}

FrameSequenceTracker* FrameSequenceTrackerCollection::StartSequence(
    FrameSequenceTrackerType type) {
  DCHECK(!IsScrollType(type));
  return StartSequenceInternal(type, ThreadType::kUnknown);
}

FrameSequenceTracker* FrameSequenceTrackerCollection::StartScrollSequence(
    FrameSequenceTrackerType type,
    FrameInfo::SmoothEffectDrivingThread scrolling_thread) {
  DCHECK(IsScrollType(type));
  return StartSequenceInternal(type, scrolling_thread);
}

void FrameSequenceTrackerCollection::CleanUp() {
  for (auto& tracker : frame_trackers_)
    tracker.second->CleanUp();
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->CleanUp();
  for (auto& tracker : removal_trackers_)
    tracker->CleanUp();
  for (auto& metric : accumulated_metrics_)
    metric.second->ReportLeftoverData();
}

void FrameSequenceTrackerCollection::StopSequence(
    FrameSequenceTrackerType type) {
  DCHECK_NE(FrameSequenceTrackerType::kCustom, type);

  auto key = std::make_pair(type, ThreadType::kUnknown);
  if (IsScrollType(type)) {
    compositor_frame_reporting_controller_->SetScrollingThread(
        ThreadType::kUnknown);
    key = std::make_pair(type, ThreadType::kCompositor);
    if (!frame_trackers_.contains(key))
      key = std::make_pair(type, ThreadType::kMain);
  }

  if (!frame_trackers_.contains(key))
    return;

  auto tracker = std::move(frame_trackers_[key]);
  if (compositor_frame_reporting_controller_) {
    compositor_frame_reporting_controller_->RemoveActiveTracker(
        tracker->type());
  }

  if (tracker->metrics()->GetEffectiveThread() == ThreadType::kCompositor) {
    DCHECK_GT(compositor_thread_driving_smoothness_, 0u);
    --compositor_thread_driving_smoothness_;
    if (compositor_frame_reporting_controller_ &&
        compositor_thread_driving_smoothness_ == 0) {
      compositor_frame_reporting_controller_->SetThreadAffectsSmoothness(
          ThreadType::kCompositor, false);
    }
  } else {
    DCHECK_GT(main_thread_driving_smoothness_, 0u);
    --main_thread_driving_smoothness_;
    if (compositor_frame_reporting_controller_ &&
        main_thread_driving_smoothness_ == 0) {
      compositor_frame_reporting_controller_->SetThreadAffectsSmoothness(
          ThreadType::kMain, false);
    }
  }

  frame_trackers_.erase(key);
  tracker->ScheduleTerminate();
  removal_trackers_.push_back(std::move(tracker));
  DestroyTrackers();
}

void FrameSequenceTrackerCollection::StartCustomSequence(int sequence_id) {
  DCHECK(!base::Contains(custom_frame_trackers_, sequence_id));

  // base::Unretained() is safe here because |this| owns FrameSequenceTracker
  // and FrameSequenceMetrics.
  custom_frame_trackers_[sequence_id] =
      base::WrapUnique(new FrameSequenceTracker(
          sequence_id,
          base::BindOnce(
              &FrameSequenceTrackerCollection::AddCustomTrackerResult,
              base::Unretained(this), sequence_id)));
}

void FrameSequenceTrackerCollection::StopCustomSequence(int sequence_id) {
  auto it = custom_frame_trackers_.find(sequence_id);
  // This happens when an animation is aborted before starting.
  if (it == custom_frame_trackers_.end())
    return;

  std::unique_ptr<FrameSequenceTracker> tracker = std::move(it->second);
  custom_frame_trackers_.erase(it);
  tracker->ScheduleTerminate();
  removal_trackers_.push_back(std::move(tracker));
  DestroyTrackers();
}

void FrameSequenceTrackerCollection::ClearAll() {
  frame_trackers_.clear();
  custom_frame_trackers_.clear();
  removal_trackers_.clear();
}

void FrameSequenceTrackerCollection::NotifyBeginImplFrame(
    const viz::BeginFrameArgs& args) {
  RecreateTrackers(args);
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportBeginImplFrame(args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportBeginImplFrame(args);
}

void FrameSequenceTrackerCollection::NotifyPauseFrameProduction() {
  for (auto& tracker : frame_trackers_)
    tracker.second->PauseFrameProduction();
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->PauseFrameProduction();
}

void FrameSequenceTrackerCollection::NotifyFrameEnd(
    const viz::BeginFrameArgs& args,
    const viz::BeginFrameArgs& main_args) {
  for (auto& tracker : frame_trackers_)
    tracker.second->ReportFrameEnd(args, main_args);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->ReportFrameEnd(args, main_args);

  // Removal trackers continue to process any frames which they started
  // observing.
  for (auto& tracker : removal_trackers_)
    tracker->ReportFrameEnd(args, main_args);
  DestroyTrackers();
}

void FrameSequenceTrackerCollection::DestroyTrackers() {
  for (auto& tracker : removal_trackers_) {
    if (tracker->termination_status() ==
        FrameSequenceTracker::TerminationStatus::kReadyForTermination) {
      // The tracker is ready to be terminated.
      // For non kCustom typed trackers, take the metrics from the tracker.
      // merge with any outstanding metrics from previous trackers of the same
      // type. If there are enough frames to report the metrics, then report the
      // metrics and destroy it. Otherwise, retain it to be merged with
      // follow-up sequences.
      // For kCustom typed trackers, |metrics| invokes AddCustomTrackerResult
      // on its destruction, which add its data to |custom_tracker_results_|
      // to be picked up by caller.
      if (tracker->metrics() &&
          tracker->type() == FrameSequenceTrackerType::kCustom)
        continue;
      auto metrics = tracker->TakeMetrics();

      auto key = std::make_pair(metrics->type(), metrics->GetEffectiveThread());
      if (accumulated_metrics_.contains(key)) {
        metrics->Merge(std::move(accumulated_metrics_[key]));
        accumulated_metrics_.erase(key);
      }

      if (metrics->HasEnoughDataForReporting())
        metrics->ReportMetrics();
      if (metrics->HasDataLeftForReporting())
        accumulated_metrics_[key] = std::move(metrics);
    }
  }

  std::erase_if(
      removal_trackers_,
      [](const std::unique_ptr<FrameSequenceTracker>& tracker) {
        return tracker->termination_status() ==
               FrameSequenceTracker::TerminationStatus::kReadyForTermination;
      });
}

void FrameSequenceTrackerCollection::RecreateTrackers(
    const viz::BeginFrameArgs& args) {
  std::vector<std::pair<FrameSequenceTrackerType, ThreadType>>
      recreate_trackers;
  for (const auto& tracker : frame_trackers_) {
    if (tracker.second->ShouldReportMetricsNow(args))
      recreate_trackers.push_back(tracker.first);
  }

  for (const auto& key : recreate_trackers) {
    DCHECK(frame_trackers_[key]);
    auto tracker_type = key.first;
    ThreadType thread_type = key.second;

    // StopSequence put the tracker in the |removal_trackers_|, which will
    // report its throughput data when its frame is presented.
    StopSequence(tracker_type);

    // The frame sequence is still active, so create a new tracker to keep
    // tracking this sequence.
    if (thread_type != FrameInfo::SmoothEffectDrivingThread::kUnknown) {
      DCHECK(IsScrollType(tracker_type));
      StartScrollSequence(tracker_type, thread_type);
    } else {
      StartSequence(tracker_type);
    }
  }
}

ActiveFrameSequenceTrackers
FrameSequenceTrackerCollection::FrameSequenceTrackerActiveTypes() const {
  ActiveFrameSequenceTrackers encoded_types = 0;
  for (const auto& key : frame_trackers_) {
    auto thread_type = key.first.first;
    encoded_types |= static_cast<ActiveFrameSequenceTrackers>(
        1 << static_cast<unsigned>(thread_type));
  }
  return encoded_types;
}

FrameSequenceTracker*
FrameSequenceTrackerCollection::GetRemovalTrackerForTesting(
    FrameSequenceTrackerType type) {
  for (const auto& tracker : removal_trackers_)
    if (tracker->type() == type)
      return tracker.get();
  return nullptr;
}

void FrameSequenceTrackerCollection::AddCustomTrackerResult(
    int custom_sequence_id,
    const FrameSequenceMetrics::CustomReportData& data) {
  DCHECK(custom_tracker_results_added_callback_);

  CustomTrackerResults results;
  results[custom_sequence_id] = data;
  custom_tracker_results_added_callback_.Run(results);
}

void FrameSequenceTrackerCollection::AddSortedFrame(
    const viz::BeginFrameArgs& args,
    const FrameInfo& frame_info) {
  for (auto& tracker : frame_trackers_)
    tracker.second->AddSortedFrame(args, frame_info);
  for (auto& tracker : custom_frame_trackers_)
    tracker.second->AddSortedFrame(args, frame_info);

  // Sorted frames could arrive after tracker are scheduled for termination.
  // Removal trackers continue to report metrics for frames which they started
  // observing.
  for (auto& tracker : removal_trackers_) {
    tracker->AddSortedFrame(args, frame_info);
  }

  DestroyTrackers();
}

}  // namespace cc
