// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_request.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"

namespace cc {
namespace {

std::string TypeToString(viz::CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case viz::CompositorFrameTransitionDirective::Type::kSave:
      return "kSave";
    case viz::CompositorFrameTransitionDirective::Type::kAnimate:
      return "kAnimate";
  }
  return "<unknown>";
}

std::string EffectToString(
    viz::CompositorFrameTransitionDirective::Effect effect) {
  switch (effect) {
    case viz::CompositorFrameTransitionDirective::Effect::kNone:
      return "kNone";
    case viz::CompositorFrameTransitionDirective::Effect::kCoverDown:
      return "kCoverDown";
    case viz::CompositorFrameTransitionDirective::Effect::kCoverLeft:
      return "kCoverLeft";
    case viz::CompositorFrameTransitionDirective::Effect::kCoverRight:
      return "kCoverRight";
    case viz::CompositorFrameTransitionDirective::Effect::kCoverUp:
      return "kCoverUp";
    case viz::CompositorFrameTransitionDirective::Effect::kExplode:
      return "kExplode";
    case viz::CompositorFrameTransitionDirective::Effect::kFade:
      return "kFade";
    case viz::CompositorFrameTransitionDirective::Effect::kImplode:
      return "kImplode";
    case viz::CompositorFrameTransitionDirective::Effect::kRevealDown:
      return "kRevealDown";
    case viz::CompositorFrameTransitionDirective::Effect::kRevealLeft:
      return "kRevealLeft";
    case viz::CompositorFrameTransitionDirective::Effect::kRevealRight:
      return "kRevealRight";
    case viz::CompositorFrameTransitionDirective::Effect::kRevealUp:
      return "kRevealUp";
  }
  return "<unknown>";
}

}  // namespace

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

std::string DocumentTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_)
      << " effect: " << EffectToString(effect_)
      << " duration: " << duration_.InMillisecondsF() << "ms]";
  return str.str();
}

}  // namespace cc
