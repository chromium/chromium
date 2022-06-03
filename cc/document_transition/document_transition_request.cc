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
#include "base/callback_helpers.h"
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
    case viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return "kAnimateRenderer";
    case viz::CompositorFrameTransitionDirective::Type::kRelease:
      return "kRelease";
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
DocumentTransitionRequest::CreatePrepare(
    Effect effect,
    uint32_t document_tag,
    TransitionConfig root_config,
    std::vector<TransitionConfig> shared_element_config,
    base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      effect, document_tag, root_config, shared_element_config,
      std::move(commit_callback)));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateStart(uint32_t document_tag,
                                       uint32_t shared_element_count,
                                       base::OnceClosure commit_callback) {
  return base::WrapUnique(new DocumentTransitionRequest(
      document_tag, shared_element_count, std::move(commit_callback)));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateAnimateRenderer(uint32_t document_tag) {
  return base::WrapUnique(
      new DocumentTransitionRequest(Type::kAnimateRenderer, document_tag));
}

// static
std::unique_ptr<DocumentTransitionRequest>
DocumentTransitionRequest::CreateRelease(uint32_t document_tag) {
  return base::WrapUnique(
      new DocumentTransitionRequest(Type::kRelease, document_tag));
}

DocumentTransitionRequest::DocumentTransitionRequest(
    Effect effect,
    uint32_t document_tag,
    TransitionConfig root_config,
    std::vector<TransitionConfig> shared_element_config,
    base::OnceClosure commit_callback)
    : type_(Type::kSave),
      effect_(effect),
      root_config_(root_config),
      document_tag_(document_tag),
      shared_element_count_(shared_element_config.size()),
      shared_element_config_(std::move(shared_element_config)),
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

DocumentTransitionRequest::DocumentTransitionRequest(Type type,
                                                     uint32_t document_tag)
    : type_(type),
      document_tag_(document_tag),
      shared_element_count_(0u),
      commit_callback_(base::DoNothing()),
      sequence_id_(s_next_sequence_id_++) {}

DocumentTransitionRequest::~DocumentTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
DocumentTransitionRequest::ConstructDirective(
    const std::map<DocumentTransitionSharedElementId, SharedElementInfo>&
        shared_element_render_pass_id_map) const {
  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements(shared_element_count_);
  DCHECK(shared_element_config_.empty() ||
         shared_element_config_.size() == shared_elements.size());
  for (uint32_t i = 0; i < shared_elements.size(); ++i) {
    // For transitions with a null element on the source page, we won't find a
    // render pass below. But we still need to propagate the configuration
    // params.
    if (!shared_element_config_.empty())
      shared_elements[i].config = shared_element_config_[i];

    auto it = std::find_if(
        shared_element_render_pass_id_map.begin(),
        shared_element_render_pass_id_map.end(),
        [this, i](const std::pair<const DocumentTransitionSharedElementId,
                                  SharedElementInfo>& value) {
          return value.first.Matches(document_tag_, i);
        });
    if (it == shared_element_render_pass_id_map.end())
      continue;
    shared_elements[i].render_pass_id = it->second.render_pass_id;
    shared_elements[i].shared_element_resource_id = it->second.resource_id;
  }
  return viz::CompositorFrameTransitionDirective(
      sequence_id_, type_, effect_, root_config_, std::move(shared_elements));
}

std::string DocumentTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_)
      << " effect: " << EffectToString(effect_)
      << " sequence_id: " << sequence_id_ << "]";
  return str.str();
}

DocumentTransitionRequest::SharedElementInfo::SharedElementInfo() = default;
DocumentTransitionRequest::SharedElementInfo::~SharedElementInfo() = default;

}  // namespace cc
