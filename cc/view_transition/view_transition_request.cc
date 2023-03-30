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
#include "cc/view_transition/view_transition_element_id.h"
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
    uint32_t document_tag,
    uint32_t shared_element_count,
    viz::NavigationID navigation_id,
    std::vector<viz::ViewTransitionElementResourceId> capture_ids,
    base::OnceClosure commit_callback) {
  return base::WrapUnique(new ViewTransitionRequest(
      Type::kSave, document_tag, shared_element_count, navigation_id,
      std::move(capture_ids), std::move(commit_callback)));
}

// static
std::unique_ptr<ViewTransitionRequest>
ViewTransitionRequest::CreateAnimateRenderer(uint32_t document_tag,
                                             viz::NavigationID navigation_id) {
  return base::WrapUnique(
      new ViewTransitionRequest(Type::kAnimateRenderer, document_tag, 0u,
                                navigation_id, {}, base::OnceClosure()));
}

// static
std::unique_ptr<ViewTransitionRequest> ViewTransitionRequest::CreateRelease(
    uint32_t document_tag,
    viz::NavigationID navigation_id) {
  return base::WrapUnique(
      new ViewTransitionRequest(Type::kRelease, document_tag, 0u, navigation_id,
                                {}, base::OnceClosure()));
}

ViewTransitionRequest::ViewTransitionRequest(
    Type type,
    uint32_t document_tag,
    uint32_t shared_element_count,
    viz::NavigationID navigation_id,
    std::vector<viz::ViewTransitionElementResourceId> capture_ids,
    base::OnceClosure commit_callback)
    : type_(type),
      document_tag_(document_tag),
      shared_element_count_(shared_element_count),
      navigation_id_(navigation_id),
      commit_callback_(std::move(commit_callback)),
      sequence_id_(s_next_sequence_id_++),
      capture_resource_ids_(std::move(capture_ids)) {
  DCHECK(type_ == Type::kSave || !commit_callback_);
}

ViewTransitionRequest::~ViewTransitionRequest() = default;

viz::CompositorFrameTransitionDirective
ViewTransitionRequest::ConstructDirective(
    const SharedElementMap& shared_element_render_pass_id_map) const {
  switch (type_) {
    case Type::kRelease:
      DCHECK_EQ(shared_element_count_, 0u);
      DCHECK(capture_resource_ids_.empty());
      return viz::CompositorFrameTransitionDirective::CreateRelease(
          navigation_id_, sequence_id_);
    case Type::kAnimateRenderer:
      DCHECK_EQ(shared_element_count_, 0u);
      DCHECK(capture_resource_ids_.empty());
      return viz::CompositorFrameTransitionDirective::CreateAnimate(
          navigation_id_, sequence_id_);
    case Type::kSave:
      break;
  }

  std::vector<viz::CompositorFrameTransitionDirective::SharedElement>
      shared_elements(shared_element_count_);
  auto capture_resource_ids = capture_resource_ids_;
  for (uint32_t i = 0; i < shared_elements.size(); ++i) {
    auto it = base::ranges::find_if(
        shared_element_render_pass_id_map,
        [this, i](const SharedElementMap::value_type& value) {
          return value.first.Matches(document_tag_, i);
        });
    if (it == shared_element_render_pass_id_map.end())
      continue;
    shared_elements[i].render_pass_id = it->second.render_pass_id;
    shared_elements[i].view_transition_element_resource_id =
        it->second.resource_id;

    // Remove the resource id from our capture ids, since we just want to have
    // "empty" resource ids left -- the ones that don't have a render pass
    // associated with them.
    capture_resource_ids.erase(
        std::remove(capture_resource_ids.begin(), capture_resource_ids.end(),
                    it->second.resource_id),
        capture_resource_ids.end());
  }

  // Add invalid render pass id for each empty resource id left in capture ids.
  for (auto& empty_resource_id : capture_resource_ids) {
    shared_elements.emplace_back();
    shared_elements.back().view_transition_element_resource_id =
        empty_resource_id;
  }

  return viz::CompositorFrameTransitionDirective::CreateSave(
      navigation_id_, sequence_id_, std::move(shared_elements));
}

std::string ViewTransitionRequest::ToString() const {
  std::ostringstream str;
  str << "[type: " << TypeToString(type_) << " sequence_id: " << sequence_id_
      << "]";
  return str.str();
}

ViewTransitionRequest::SharedElementInfo::SharedElementInfo() = default;
ViewTransitionRequest::SharedElementInfo::~SharedElementInfo() = default;

}  // namespace cc
