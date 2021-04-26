// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_request.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "cc/document_transition/document_transition_shared_element_id.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"

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
                                         uint32_t document_tag,
                                         uint32_t shared_element_count,
                                         base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      effect, document_tag, shared_element_count, std::move(commit_callback)));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateStart(uint32_t document_tag,
                                       uint32_t shared_element_count,
                                       base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      document_tag, shared_element_count, std::move(commit_callback)));
}

DocumentTransitionRequest::DocumentTransitionRequest(
    Effect effect,
    uint32_t document_tag,
    uint32_t shared_element_count,
    base::OnceClosure commit_callback)
    : type_(Type::kSave),
      effect_(effect),
      document_tag_(document_tag),
      shared_element_count_(shared_element_count),
      commit_callback_(std::move(commit_callback)),
      sequence_id_(s_next_sequence_id_++) {}

DocumentTransitionRequest::DocumentTransitionRequest(
    uint32_t document_tag,
    uint32_t shared_element_count,
    base::OnceClosure commit_callback)
    : type_(Type::kAnimate),
      document_tag_(document_tag),
      shared_element_count_(shared_element_count),
      commit_callback_(std::move(commit_callback)),
      sequence_id_(s_next_sequence_id_++) {}

DocumentTransitionRequest::~DocumentTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
DocumentTransitionRequest::ConstructDirective(
    const std::map<DocumentTransitionSharedElementId,
                   viz::CompositorRenderPassId>&
        shared_element_render_pass_id_map) const {
  std::vector<viz::CompositorRenderPassId> shared_passes(shared_element_count_);
  for (uint32_t i = 0; i < shared_passes.size(); ++i) {
    auto it = std::find_if(
        shared_element_render_pass_id_map.begin(),
        shared_element_render_pass_id_map.end(),
        [this, i](const std::pair<const DocumentTransitionSharedElementId,
                                  viz::CompositorRenderPassId>& value) {
          return value.first.Matches(document_tag_, i);
        });
    if (it == shared_element_render_pass_id_map.end())
      continue;
    shared_passes[i] = it->second;
  }
  return viz::CompositorFrameTransitionDirective(sequence_id_, type_, effect_,
                                                 std::move(shared_passes));
}

std::string DocumentTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_)
      << " effect: " << EffectToString(effect_)
      << " sequence_id: " << sequence_id_ << "]";
  return str.str();
}

}  // namespace cc
