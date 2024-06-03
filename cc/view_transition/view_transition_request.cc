// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/view_transition/view_transition_request.h"

#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"

namespace cc {
namespace {

std::string TypeToString(viz::CompositorFrameTransitionDirective::Type type) {
  switch (type) {
    case viz::CompositorFrameTransitionDirective::Type::kSave:
      return "kSave";
    case viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer:
      return "kAnimateRenderer";
    case viz::CompositorFrameTransitionDirective::Type::kRelease:
      return "kRelease";
  }
  return "<unknown>";
}

}  // namespace

uint32_t ViewTransitionRequest::s_next_sequence_id_ = 1;

// static
std::unique_ptr<ViewTransitionRequest> ViewTransitionRequest::CreateCapture(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    std::vector<viz::ViewTransitionElementResourceId> capture_ids,
    base::OnceClosure commit_callback) {
  return base::WrapUnique(new ViewTransitionRequest(
      Type::kSave, transition_token, maybe_cross_frame_sink,
      std::move(capture_ids), std::move(commit_callback)));
}

// static
std::unique_ptr<ViewTransitionRequest>
ViewTransitionRequest::CreateAnimateRenderer(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink) {
  return base::WrapUnique(new ViewTransitionRequest(
      Type::kAnimateRenderer, transition_token, maybe_cross_frame_sink, {},
      base::OnceClosure()));
}

// static
std::unique_ptr<ViewTransitionRequest> ViewTransitionRequest::CreateRelease(
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink) {
  return base::WrapUnique(new ViewTransitionRequest(
      Type::kRelease, transition_token, maybe_cross_frame_sink, {},
      base::OnceClosure()));
}

ViewTransitionRequest::ViewTransitionRequest(
    Type type,
    const blink::ViewTransitionToken& transition_token,
    bool maybe_cross_frame_sink,
    std::vector<viz::ViewTransitionElementResourceId> capture_ids,
    base::OnceClosure commit_callback)
    : type_(type),
      transition_token_(transition_token),
      maybe_cross_frame_sink_(maybe_cross_frame_sink),
      commit_callback_(std::move(commit_callback)),
      sequence_id_(s_next_sequence_id_++),
      capture_resource_ids_(std::move(capture_ids)) {
  DCHECK(type_ == Type::kSave || !commit_callback_);
}

ViewTransitionRequest::~ViewTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
ViewTransitionRequest::ConstructDirective(
    const ViewTransitionElementMap& shared_element_render_pass_id_map,
    const gfx::DisplayColorSpaces& display_color_spaces) const {
  switch (type_) {
    case Type::kRelease:
      DCHECK(capture_resource_ids_.empty());
      return viz::CompositorFrameTransitionDirective::CreateRelease(
          transition_token_, maybe_cross_frame_sink_, sequence_id_);
    case Type::kAnimateRenderer:
      DCHECK(capture_resource_ids_.empty());
      return viz::CompositorFrameTransitionDirective::CreateAnimate(
          transition_token_, maybe_cross_frame_sink_, sequence_id_);
    case Type::kSave:
      break;
  }

  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements(capture_resource_ids_.size());

  for (size_t i = 0; i < shared_elements.size(); i++) {
    const auto& capture_resource_id = capture_resource_ids_[i];
    shared_elements[i].view_transition_element_resource_id =
        capture_resource_id;

    auto it = shared_element_render_pass_id_map.find(capture_resource_id);
    if (it != shared_element_render_pass_id_map.end()) {
      shared_elements[i].render_pass_id = it->second;
    }
  }

  return viz::CompositorFrameTransitionDirective::CreateSave(
      transition_token_, maybe_cross_frame_sink_, sequence_id_,
      std::move(shared_elements), display_color_spaces);
}

std::string ViewTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_) << " sequence_id: " << sequence_id_
      << "]";
  return str.str();
}

}  // namespace cc
