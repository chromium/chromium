// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/presentation_time_callback_buffer.h"

#include <utility>
#include <vector>

namespace cc {

PresentationTimeCallbackBuffer::PresentationTimeCallbackBuffer() = default;

PresentationTimeCallbackBuffer::PresentationTimeCallbackBuffer(
    PresentationTimeCallbackBuffer&& other)
    : frame_token_infos_(std::move(other.frame_token_infos_)) {}

PresentationTimeCallbackBuffer& PresentationTimeCallbackBuffer::operator=(
    PresentationTimeCallbackBuffer&& other) {
  if (this != &other) {
    frame_token_infos_ = std::move(other.frame_token_infos_);
  }
  return *this;
}

PresentationTimeCallbackBuffer::~PresentationTimeCallbackBuffer() = default;

PresentationTimeCallbackBuffer::FrameTokenInfo::FrameTokenInfo(uint32_t token)
    : token(token) {}

PresentationTimeCallbackBuffer::FrameTokenInfo::FrameTokenInfo(
    FrameTokenInfo&&) = default;
PresentationTimeCallbackBuffer::FrameTokenInfo&
PresentationTimeCallbackBuffer::FrameTokenInfo::operator=(FrameTokenInfo&&) =
    default;
PresentationTimeCallbackBuffer::FrameTokenInfo::~FrameTokenInfo() = default;

void PresentationTimeCallbackBuffer::RegisterMainThreadPresentationCallbacks(
    uint32_t frame_token,
    std::vector<CallbackType> callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FrameTokenInfo& frame_info = GetOrMakeRegistration(frame_token);

  // Splice the given |callbacks| onto the vector of existing callbacks.
  auto& sink = frame_info.main_thread_callbacks;
  sink.reserve(sink.size() + callbacks.size());
  std::move(callbacks.begin(), callbacks.end(), std::back_inserter(sink));

  DCHECK_LE(frame_token_infos_.size(), 25u);
}

void PresentationTimeCallbackBuffer::RegisterCompositorPresentationCallbacks(
    uint32_t frame_token,
    std::vector<CallbackType> callbacks) {
  // Splice the given |callbacks| onto the vector of existing callbacks.
  std::vector<LayerTreeHost::PresentationTimeCallback>& sink =
      GetOrMakeRegistration(frame_token).compositor_thread_callbacks;
  sink.reserve(sink.size() + callbacks.size());
  std::move(callbacks.begin(), callbacks.end(), std::back_inserter(sink));

  DCHECK_LE(frame_token_infos_.size(), 25u);
}

void PresentationTimeCallbackBuffer::RegisterFrameTime(
    uint32_t frame_token,
    base::TimeTicks frame_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FrameTokenInfo& frame_info = GetOrMakeRegistration(frame_token);

  // Protect against clobbering previously registered frame times.
  DCHECK(frame_info.frame_time.is_null() ||
         frame_info.frame_time == frame_time);
  frame_info.frame_time = frame_time;

  DCHECK_LE(frame_token_infos_.size(), 25u);
}

PresentationTimeCallbackBuffer::PendingCallbacks::PendingCallbacks() = default;
PresentationTimeCallbackBuffer::PendingCallbacks::PendingCallbacks(
    PendingCallbacks&&) = default;
PresentationTimeCallbackBuffer::PendingCallbacks&
PresentationTimeCallbackBuffer::PendingCallbacks::operator=(
    PendingCallbacks&&) = default;
PresentationTimeCallbackBuffer::PendingCallbacks::~PendingCallbacks() = default;

PresentationTimeCallbackBuffer::PendingCallbacks
PresentationTimeCallbackBuffer::PopPendingCallbacks(uint32_t frame_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PendingCallbacks result;

  while (!frame_token_infos_.empty()) {
    auto info = frame_token_infos_.begin();
    if (viz::FrameTokenGT(info->token, frame_token))
      break;

    // Forward compositor frame timings on exact frame token matches. These
    // timings correspond to frames that caused on-screen damage.
    if (info->token == frame_token && !info->frame_time.is_null())
      result.frame_time = info->frame_time;

    // Collect the main-thread callbacks. It's the caller's job to post them to
    // the main thread.
    std::move(info->main_thread_callbacks.begin(),
              info->main_thread_callbacks.end(),
              std::back_inserter(result.main_thread_callbacks));

    // Collect the compositor-thread callbacks. It's the caller's job to run
    // them on the compositor thread.
    std::move(info->compositor_thread_callbacks.begin(),
              info->compositor_thread_callbacks.end(),
              std::back_inserter(result.compositor_thread_callbacks));

    frame_token_infos_.erase(info);
  }

  return result;
}

PresentationTimeCallbackBuffer::FrameTokenInfo&
PresentationTimeCallbackBuffer::GetOrMakeRegistration(uint32_t frame_token) {
  // If the freshest registration is for an earlier frame token, add a new
  // entry to the queue.
  if (frame_token_infos_.empty() ||
      viz::FrameTokenGT(frame_token, frame_token_infos_.back().token)) {
    frame_token_infos_.emplace_back(frame_token);
  }

  // Registrations should use monotonically increasing frame tokens.
  DCHECK_EQ(frame_token_infos_.back().token, frame_token);

  return frame_token_infos_.back();
}

}  // namespace cc
