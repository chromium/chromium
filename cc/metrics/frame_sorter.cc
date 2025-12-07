// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sorter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/frame_info.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {
namespace {
const base::TimeDelta kDefaultSlidingWindowInterval = base::Seconds(1);
}  // namespace

using FrameState = FrameSorter::FrameState;

void FrameState::OnBegin() {
  on_begin_counter++;
}

void FrameState::OnAck(bool is_dropped) {
  ack_counter++;
  is_dropped_ |= is_dropped;
}

void FrameState::OnReset() {
  should_ignore_ = true;
}

bool FrameState::IsComplete() const {
  return (on_begin_counter == ack_counter);
}

FrameSorter::FrameSorter() = default;
FrameSorter::~FrameSorter() {
  observers_.Clear();
}

void FrameSorter::AddObserver(FrameSorterObserver* observer) {
  observers_.AddObserver(observer);
}

void FrameSorter::RemoveObserver(FrameSorterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FrameSorter::AddNewFrame(const viz::BeginFrameArgs& args) {
  if (current_source_id_.has_value() &&
      current_source_id_ > args.frame_id.source_id) {
    return;
  }
  if (current_source_id_.has_value() &&
      current_source_id_ < args.frame_id.source_id) {
    // The change in source_id can be as a result of crash on gpu process,
    // which invalidates existing pending frames (no ack is expected).
    Reset(true);
  }

  if (!pending_frames_.empty()) {
    const auto& last = pending_frames_.back();
    DCHECK_LE(last.frame_id.sequence_number, args.frame_id.sequence_number);
  }
  current_source_id_ = args.frame_id.source_id;

  // This condition prevents adding duplicate frames to pending_frames_
  // as well as not re-adding a frame after it's removed from pending_frames_.
  if (!frame_states_.count(args.frame_id))
    pending_frames_.push_back(args);
  frame_states_[args.frame_id].OnBegin();

  // Remove the oldest frame until remaining pending frames are lower that
  // than the limit
  while (pending_frames_.size() > kPendingFramesMaxSize) {
    const auto& first = pending_frames_.front();
    frame_states_.erase(first.frame_id);
    frame_infos_.erase(first.frame_id);
    pending_frames_.pop_front();
  }
}

void FrameSorter::AddFrameInfoToBuffer(const FrameInfo& frame_info) {
  ring_buffer_.SaveToBuffer(frame_info.final_state);
  ++total_frames_;
  if (frame_info.final_state == FrameInfo::FrameFinalState::kDropped) {
    ++total_dropped_;
  } else if (frame_info.final_state ==
                 FrameInfo::FrameFinalState::kPresentedPartialNewMain ||
             frame_info.final_state ==
                 FrameInfo::FrameFinalState::kPresentedPartialOldMain) {
    ++total_partial_;
  }
}

void FrameSorter::AddFrameResult(const viz::BeginFrameArgs& args,
                                 const FrameInfo& frame_info) {
  if (pending_frames_.empty() || current_source_id_ > args.frame_id.source_id) {
    // The change in source_id can be as a result of crash on gpu process,
    // and as a result the corresponding frame to result does not exist.
    return;
  }

  // Early exit if expecting acks for frames, such as when:
  // - This frame is already added to pending_frames_
  // - When the frame was in pending_frames_ and was removed because of reset.
  if (!frame_states_.count(args.frame_id))
    return;

  if (report_for_ui_) {
    sliding_window_.emplace(args, frame_info);
    if (frame_info.IsDroppedAffectingSmoothness()) {
      DCHECK_GE(dropped_frame_count_in_window_ + 1, 0u);
      dropped_frame_count_in_window_ += 1;
    }
  }
  const auto f = frame_infos_.find(args.frame_id);
  if (f != frame_infos_.end()) {
    f->second.MergeWith(frame_info);
  } else {
    frame_infos_[args.frame_id] = frame_info;
  }

  const bool is_dropped = frame_info.IsDroppedAffectingSmoothness();
  auto& frame_state = frame_states_[args.frame_id];
  frame_state.OnAck(is_dropped);
  if (!frame_state.IsComplete()) {
    return;
  }
  if (frame_state.should_ignore()) {
    // The associated frame in pending_frames_ was already removed in Reset().
    frame_states_.erase(args.frame_id);
    frame_infos_.erase(args.frame_id);
    return;
  }

  const auto& last_frame = pending_frames_.front();
  if (last_frame.frame_id == args.frame_id) {
    FlushFrames();
  } else if (last_frame.frame_id.sequence_number <=
             args.frame_id.sequence_number) {
    // Checks if the corresponding frame to the result, exists in the
    // pending_frames list.
    DCHECK(std::binary_search(
        pending_frames_.begin(), pending_frames_.end(), args,
        [](const viz::BeginFrameArgs& one, const viz::BeginFrameArgs& two) {
          return one.frame_id < two.frame_id;
        }))
        << args.frame_id.ToString()
        << pending_frames_.front().frame_id.ToString();
  }

  // Report frames on every frame for UI. This needs to happen after
  // `FrameSorter::AddFrameResult` so that the current ending frame is included
  // in the sliding window.
  if (report_for_ui_) {
    auto* recorder = CustomMetricRecorder::Get();
    if (sliding_window_current_percent_dropped_ && recorder) {
      recorder->ReportPercentDroppedFramesInOneSecondWindow2(
          *sliding_window_current_percent_dropped_);
    }

    if (ComputeCurrentWindowSize() < kDefaultSlidingWindowInterval) {
      return;
    }
    DCHECK_GE(dropped_frame_count_in_window_, 0u);
    DCHECK_GE(sliding_window_.size(), dropped_frame_count_in_window_);

    while (ComputeCurrentWindowSize() > kDefaultSlidingWindowInterval) {
      PopSlidingWindow(args);
    }
    DCHECK(!sliding_window_.empty());
  }
}

bool FrameSorter::IsAlreadyReportedDropped(const viz::BeginFrameId& id) const {
  auto it = frame_states_.find(id);
  if (it == frame_states_.end())
    return false;
  return it->second.is_dropped();
}

void FrameSorter::Reset(bool reset_fcp) {
  total_frames_ = 0;
  total_partial_ = 0;
  total_dropped_ = 0;
  for (const auto& pending_frame : pending_frames_) {
    const auto& frame_id = pending_frame.frame_id;
    auto& frame_state = frame_states_[frame_id];
    if (frame_state.IsComplete() && !frame_state.should_ignore()) {
      for (auto& observer : observers_) {
        observer.AddSortedFrame(pending_frame, frame_infos_[frame_id]);
      }
      frame_states_.erase(frame_id);
      frame_infos_.erase(frame_id);
      continue;
    }
    frame_state.OnReset();
  }
  pending_frames_.clear();
  ring_buffer_.Clear();
  if (reset_fcp) {
    first_contentful_paint_received_ = false;
  }
  sliding_window_ = {};
  sliding_window_current_percent_dropped_.reset();
  dropped_frame_count_in_window_ = 0;
}

void FrameSorter::FlushFrames() {
  DCHECK(!pending_frames_.empty());
  size_t flushed_count = 0;
  while (!pending_frames_.empty()) {
    const auto& first = pending_frames_.front();
    const auto& frame_id = first.frame_id;
    auto& frame_state = frame_states_[frame_id];
    if (!frame_state.IsComplete())
      break;
    ++flushed_count;
    for (auto& observer : observers_) {
      observer.AddSortedFrame(first, frame_infos_[frame_id]);
    }
    frame_states_.erase(frame_id);
    frame_infos_.erase(frame_id);
    pending_frames_.pop_front();
  }
  DCHECK_GT(flushed_count, 0u);
}

uint32_t FrameSorter::GetAverageThroughput() const {
  size_t good_frames = 0;
  for (auto it = End(); it; --it) {
    if (**it == FrameInfo::FrameFinalState::kPresentedAll ||
        **it == FrameInfo::FrameFinalState::kPresentedPartialOldMain ||
        **it == FrameInfo::FrameFinalState::kPresentedPartialNewMain) {
      ++good_frames;
    }
  }
  double throughput = 100. * good_frames / ring_buffer_.BufferSize();
  return static_cast<uint32_t>(throughput);
}

void FrameSorter::OnFirstContentfulPaintReceived() {
  DCHECK(!first_contentful_paint_received_);
  first_contentful_paint_received_ = true;
}

base::TimeDelta FrameSorter::ComputeCurrentWindowSize() const {
  if (sliding_window_.empty()) {
    return {};
  }
  return sliding_window_.back().first.frame_time +
         sliding_window_.back().first.interval -
         sliding_window_.front().first.frame_time;
}

void FrameSorter::PopSlidingWindow(const viz::BeginFrameArgs& args) {
  const auto removed_args = sliding_window_.front().first;
  const auto removed_frame_info = sliding_window_.front().second;
  if (removed_frame_info.IsDroppedAffectingSmoothness()) {
    DCHECK_GE(dropped_frame_count_in_window_ - 1, 0u);
    dropped_frame_count_in_window_ -= 1;
  }
  sliding_window_.pop();
  if (sliding_window_.empty()) {
    return;
  }

  // Don't count the newest element if it is outside the current window.
  const auto newest_was_dropped =
      sliding_window_.back().second.IsDroppedAffectingSmoothness();

  uint32_t invalidated_frames = 0;
  if (ComputeCurrentWindowSize() > kDefaultSlidingWindowInterval &&
      newest_was_dropped) {
    invalidated_frames++;
  }

  uint32_t dropped = dropped_frame_count_in_window_ - invalidated_frames;
  auto total_frames_in_window = kDefaultSlidingWindowInterval / args.interval;
  const double percent_dropped_frame =
      std::min((dropped * 100.0) / total_frames_in_window, 100.0);
  sliding_window_current_percent_dropped_ = percent_dropped_frame;
}
void FrameSorter::EnableReportForUI() {
  report_for_ui_ = true;
}

}  // namespace cc
