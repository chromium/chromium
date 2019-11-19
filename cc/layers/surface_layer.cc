// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include <stdint.h>

#include "base/trace_event/trace_event.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  return base::WrapRefCounted(new SurfaceLayer());
}

scoped_refptr<SurfaceLayer> SurfaceLayer::Create(
    UpdateSubmissionStateCB update_submission_state_callback) {
  return base::WrapRefCounted(
      new SurfaceLayer(std::move(update_submission_state_callback)));
}

SurfaceLayer::SurfaceLayer() = default;

SurfaceLayer::SurfaceLayer(
    UpdateSubmissionStateCB update_submission_state_callback)
    : update_submission_state_callback_(
          std::move(update_submission_state_callback)) {}

SurfaceLayer::~SurfaceLayer() {
  DCHECK(!layer_tree_host());
}

void SurfaceLayer::SetSurfaceId(const viz::SurfaceId& surface_id,
                                const DeadlinePolicy& deadline_policy) {
  if (surface_range_.end() == surface_id &&
      deadline_policy.use_existing_deadline()) {
    return;
  }
  if (surface_id.local_surface_id().is_valid()) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(surface_id.local_surface_id().embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "SetSurfaceId", "surface_id", surface_id.ToString());
  }

  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range_);

  surface_range_ = viz::SurfaceRange(surface_range_.start(), surface_id);

  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range_);

  // We should never block or set a deadline on an invalid
  // |surface_range_|.
  if (!surface_range_.IsValid()) {
    deadline_in_frames_ = 0u;
  } else if (!deadline_policy.use_existing_deadline()) {
    deadline_in_frames_ = deadline_policy.deadline_in_frames();
  }
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsCommit();
}

void SurfaceLayer::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  // The fallback should never move backwards.
  DCHECK(!surface_range_.start() ||
         !surface_range_.start()->IsNewerThan(surface_id));
  if (surface_range_.start() == surface_id)
    return;

  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range_);

  surface_range_ = viz::SurfaceRange(
      surface_id.is_valid() ? base::Optional<viz::SurfaceId>(surface_id)
                            : base::nullopt,
      surface_range_.end());

  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range_);

  SetNeedsCommit();
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  if (stretch_content_to_fill_bounds_ == stretch_content_to_fill_bounds)
    return;
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  SetNeedsPushProperties();
}

void SurfaceLayer::SetSurfaceHitTestable(bool surface_hit_testable) {
  if (surface_hit_testable_ == surface_hit_testable)
    return;
  surface_hit_testable_ = surface_hit_testable;
}

void SurfaceLayer::SetHasPointerEventsNone(bool has_pointer_events_none) {
  if (has_pointer_events_none_ == has_pointer_events_none)
    return;
  has_pointer_events_none_ = has_pointer_events_none;
  SetNeedsPushProperties();
  // Change of pointer-events property triggers an update of viz hit test data,
  // we need to commit in order to submit the new data with compositor frame.
  SetNeedsCommit();
}

void SurfaceLayer::SetIsReflection(bool is_reflection) {
  is_reflection_ = true;
}

void SurfaceLayer::SetMayContainVideo(bool may_contain_video) {
  may_contain_video_ = may_contain_video;
}

std::unique_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  auto layer_impl = SurfaceLayerImpl::Create(tree_impl, id(),
                                             update_submission_state_callback_);
  layer_impl->set_may_contain_video(may_contain_video_);
  return layer_impl;
}

bool SurfaceLayer::HasDrawableContent() const {
  return surface_range_.IsValid() && Layer::HasDrawableContent();
}

void SurfaceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    return;
  }
  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range_);

  Layer::SetLayerTreeHost(host);

  if (layer_tree_host() && surface_range_.IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range_);
}

void SurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "SurfaceLayer::PushPropertiesTo");
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetRange(surface_range_, std::move(deadline_in_frames_));
  // Unless the client explicitly calls SetSurfaceId again after this
  // commit, don't block on |surface_range_| again.
  deadline_in_frames_ = 0u;
  layer_impl->SetIsReflection(is_reflection_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
  layer_impl->SetSurfaceHitTestable(surface_hit_testable_);
  layer_impl->SetHasPointerEventsNone(has_pointer_events_none_);
}

}  // namespace cc
