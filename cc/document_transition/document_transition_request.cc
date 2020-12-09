// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_request.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"

namespace cc {

uint32_t DocumentTransitionRequest::s_next_sequence_id_ = 1;

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreatePrepare(Effect effect,
                                         base::TimeDelta duration,
                                         base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      effect, duration, std::move(commit_callback)));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateStart(base::OnceClosure commit_callback) {
  return base::WrapUnique(
      new DocumentTransitionRequest(std::move(commit_callback)));
}

DocumentTransitionRequest::DocumentTransitionRequest(
    Effect effect,
    base::TimeDelta duration,
    base::OnceClosure commit_callback)
    : type_(Type::kSave),
      effect_(effect),
      duration_(duration),
      commit_callback_(std::move(commit_callback)) {}

DocumentTransitionRequest::DocumentTransitionRequest(
    base::OnceClosure commit_callback)
    : type_(Type::kAnimate), commit_callback_(std::move(commit_callback)) {}

DocumentTransitionRequest::~DocumentTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
DocumentTransitionRequest::ConstructDirective() const {
  // Note that the clamped_duration is also verified at
  // CompositorFrameTransitionDirective deserialization time.
  auto clamped_duration =
      duration_ < viz::CompositorFrameTransitionDirective::kMaxDuration
          ? duration_
          : viz::CompositorFrameTransitionDirective::kMaxDuration;
  return viz::CompositorFrameTransitionDirective(s_next_sequence_id_++, type_,
                                                 effect_, clamped_duration);
}

}  // namespace cc
