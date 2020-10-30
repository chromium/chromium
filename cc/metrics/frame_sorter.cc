// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sorter.h"

#include <utility>

namespace cc {

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
  if (frames_expecting_acks_.count(args.frame_id)) {
    DCHECK(!frames_expecting_two_acks_.count(args.frame_id));
    frames_expecting_two_acks_.insert(args.frame_id);
    return;
  }
  pending_frames_.push_back(args);
  frames_expecting_acks_.insert(args.frame_id);
}

void FrameSorter::AddFrameResult(const viz::BeginFrameArgs& args,
                                 bool is_dropped) {
  if (pending_frames_.empty() || current_source_id_ > args.frame_id.source_id) {
    // The change in source_id can be as a result of crash on gpu process,
    // and as a result the corresponding frame to result does not exist.
    return;
  }
  DCHECK(!pending_frames_.empty());
  // If Frame expects two acks, record the result but do not push in acked set
  if (RemoveFrameExpectingTwoAcks(args.frame_id)) {
    received_partial_dropped_acks_.insert({args.frame_id, is_dropped});
    return;
  }
  if (frames_to_ignore_acks_.count(args.frame_id)) {
    frames_to_ignore_acks_.erase(args.frame_id);
    return;
  }

  const auto& existing_result =
      received_partial_dropped_acks_.find(args.frame_id);
  if (existing_result != received_partial_dropped_acks_.end())
    is_dropped |= existing_result->second;
  received_acks_.insert({args.frame_id, is_dropped});
  if (pending_frames_.front().frame_id == args.frame_id) {
    FlushAcks();
  } else {
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

bool FrameSorter::RemoveFrameExpectingTwoAcks(
    const viz::BeginFrameId& frame_id) {
  if (frames_expecting_two_acks_.count(frame_id)) {
    frames_expecting_two_acks_.erase(frame_id);
    return true;
  }
  return false;
}

void FrameSorter::Reset() {
  for (auto pending_frame : pending_frames_) {
    const auto& result = received_acks_.find(pending_frame.frame_id);
    if (result == received_acks_.end()) {
      // Future acks will be ignored for this frame.
      frames_to_ignore_acks_.insert(pending_frame.frame_id);
    } else {
      flush_callback_.Run(pending_frame, result->second);
    }
  }
  pending_frames_.clear();
  frames_expecting_acks_.clear();
  received_acks_.clear();
  received_partial_dropped_acks_.clear();
}

void FrameSorter::FlushAcks() {
  DCHECK(!pending_frames_.empty());
  size_t flushed_count = 0;
  while (!pending_frames_.empty()) {
    const auto first = pending_frames_.front();
    const auto& result = received_acks_.find(first.frame_id);
    if (result == received_acks_.end())
      break;
    frames_expecting_acks_.erase(first.frame_id);
    pending_frames_.erase(pending_frames_.begin());
    ++flushed_count;
    flush_callback_.Run(first, result->second);
    received_acks_.erase(result);
  }
  DCHECK_GT(flushed_count, 0u);
}

}  // namespace cc
