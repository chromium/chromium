// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sorter.h"

#include <utility>

#include "cc/metrics/frame_info.h"

namespace cc {

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

FrameSorter::FrameSorter(InOrderBeginFramesCallback callback)
    : flush_callback_(std::move(callback)) {
  DCHECK(!flush_callback_.is_null());
}
FrameSorter::~FrameSorter() = default;

void FrameSorter::AddNewFrame(const viz::BeginFrameArgs& args) {
  if (current_source_id_.has_value() &&
      current_source_id_ > args.frame_id.source_id) {
    return;
  }
  if (current_source_id_.has_value() &&
      current_source_id_ < args.frame_id.source_id) {
    // The change in source_id can be as a result of crash on gpu process,
    // which invalidates existing pending frames (no ack is expected).
    Reset();
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
}

bool FrameSorter::IsAlreadyReportedDropped(const viz::BeginFrameId& id) const {
  auto it = frame_states_.find(id);
  if (it == frame_states_.end())
    return false;
  return it->second.is_dropped();
}

void FrameSorter::Reset() {
  for (auto pending_frame : pending_frames_) {
    const auto& frame_id = pending_frame.frame_id;
    auto& frame_state = frame_states_[frame_id];
    if (frame_state.IsComplete() && !frame_state.should_ignore()) {
      flush_callback_.Run(pending_frame, frame_infos_[frame_id]);
      frame_states_.erase(frame_id);
      frame_infos_.erase(frame_id);
      continue;
    }
    frame_state.OnReset();
  }
  pending_frames_.clear();
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
    flush_callback_.Run(first, frame_infos_[frame_id]);
    frame_states_.erase(frame_id);
    frame_infos_.erase(frame_id);
    pending_frames_.pop_front();
  }
  DCHECK_GT(flushed_count, 0u);
}

}  // namespace cc
