// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include <stdint.h>
#include <memory>
#include <utility>

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

SurfaceLayer::SurfaceLayer()
    : may_contain_video_(false),
      deadline_in_frames_(0u),
      stretch_content_to_fill_bounds_(false),
      surface_hit_testable_(false),
      has_pointer_events_none_(false),
      is_reflection_(false),
      callback_layer_tree_host_changed_(false) {}

SurfaceLayer::SurfaceLayer(
    UpdateSubmissionStateCB update_submission_state_callback)
    : update_submission_state_callback_(
          std::move(update_submission_state_callback)),
      may_contain_video_(false),
      deadline_in_frames_(0u),
      stretch_content_to_fill_bounds_(false),
      surface_hit_testable_(false),
      has_pointer_events_none_(false),
      is_reflection_(false),
      callback_layer_tree_host_changed_(false) {}

SurfaceLayer::~SurfaceLayer() {
  DCHECK(!layer_tree_host());
}

void SurfaceLayer::SetSurfaceId(const viz::SurfaceId& surface_id,
                                const DeadlinePolicy& deadline_policy) {
  if (surface_range_.Read(*this).end() == surface_id &&
      deadline_policy.use_existing_deadline()) {
    return;
  }
  auto& surface_range = surface_range_.Write(*this);
  if (surface_id.local_surface_id().is_valid()) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(surface_id.local_surface_id().embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "SetSurfaceId", "surface_id", surface_id.ToString());
  }

  if (layer_tree_host() && surface_range.IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range);

  surface_range = viz::SurfaceRange(surface_range.start(), surface_id);

  if (layer_tree_host() && surface_range.IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range);

  // We should never block or set a deadline on an invalid
  // |surface_range|.
  if (!surface_range.IsValid()) {
    deadline_in_frames_.Write(*this) = 0u;
  } else if (!deadline_policy.use_existing_deadline()) {
    deadline_in_frames_.Write(*this) = deadline_policy.deadline_in_frames();
  }
  UpdateDrawsContent();
  SetNeedsCommit();
}

void SurfaceLayer::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  // The fallback should never move backwards.
  DCHECK(!surface_range_.Read(*this).start() ||
         !surface_range_.Read(*this).start()->IsNewerThan(surface_id));
  if (surface_range_.Read(*this).start() == surface_id)
    return;

  auto& surface_range = surface_range_.Write(*this);
  if (layer_tree_host() && surface_range.IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range);

  surface_range = viz::SurfaceRange(
      surface_id.is_valid() ? std::optional<viz::SurfaceId>(surface_id)
                            : std::nullopt,
      surface_range.end());

  if (layer_tree_host() && surface_range.IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range);

  SetNeedsCommit();
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  if (stretch_content_to_fill_bounds_.Read(*this) ==
      stretch_content_to_fill_bounds)
    return;
  stretch_content_to_fill_bounds_.Write(*this) = stretch_content_to_fill_bounds;
  SetNeedsPushProperties();
}

void SurfaceLayer::SetSurfaceHitTestable(bool surface_hit_testable) {
  if (surface_hit_testable_.Read(*this) == surface_hit_testable)
    return;
  surface_hit_testable_.Write(*this) = surface_hit_testable;
}

void SurfaceLayer::SetHasPointerEventsNone(bool has_pointer_events_none) {
  if (has_pointer_events_none_.Read(*this) == has_pointer_events_none)
    return;
  has_pointer_events_none_.Write(*this) = has_pointer_events_none;
  SetNeedsPushProperties();
  // Change of pointer-events property triggers an update of viz hit test data,
  // we need to commit in order to submit the new data with compositor frame.
  SetNeedsCommit();
}

void SurfaceLayer::SetIsReflection(bool is_reflection) {
  is_reflection_.Write(*this) = true;
}

void SurfaceLayer::SetMayContainVideo(bool may_contain_video) {
  may_contain_video_.Write(*this) = may_contain_video;
  SetNeedsCommit();
}

std::unique_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  auto layer_impl = SurfaceLayerImpl::Create(
      tree_impl, id(), update_submission_state_callback_.Read(*this));
  return layer_impl;
}

bool SurfaceLayer::HasDrawableContent() const {
  return surface_range_.Read(*this).IsValid() && Layer::HasDrawableContent();
}

void SurfaceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    return;
  }

  // Any time we change trees, start out as "not visible". Notify the impl layer
  // in case drawing has already started so it can reset its drawing state.
  // Note: if this layer is detached while throttled, the LayerImpl may remain
  // in place until we reattach; in that case it will never know it went
  // invisible and so needs to be reset.
  auto callback = update_submission_state_callback_.Read(*this);
  if (callback) {
    callback.Run(false, nullptr);
    callback_layer_tree_host_changed_.Write(*this) = true;
  }

  if (layer_tree_host() && surface_range_.Read(*this).IsValid())
    layer_tree_host()->RemoveSurfaceRange(surface_range_.Read(*this));

  Layer::SetLayerTreeHost(host);

  if (layer_tree_host() && surface_range_.Read(*this).IsValid())
    layer_tree_host()->AddSurfaceRange(surface_range_.Read(*this));
}

void SurfaceLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);
  TRACE_EVENT0("cc", "SurfaceLayer::PushPropertiesTo");
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetRange(surface_range_.Read(*this),
                       std::move(deadline_in_frames_.Write(*this)));
  // Unless the client explicitly calls SetSurfaceId again after this
  // commit, don't block on |surface_range_| again.
  deadline_in_frames_.Write(*this) = 0u;
  layer_impl->SetIsReflection(is_reflection_.Read(*this));
  layer_impl->SetStretchContentToFillBounds(
      stretch_content_to_fill_bounds_.Read(*this));
  layer_impl->SetSurfaceHitTestable(surface_hit_testable_.Read(*this));
  layer_impl->SetHasPointerEventsNone(has_pointer_events_none_.Read(*this));
  layer_impl->set_may_contain_video(may_contain_video_.Read(*this));

  if (callback_layer_tree_host_changed_.Read(*this)) {
    // Anytime SetLayerTreeHost is called and
    // `update_submission_state_callback_` is defined, the callback will be used
    // to reset the visibility state. We must share this information with the
    // SurfaceLayerImpl since it also tracks visibility state so it can avoid
    // unnecessary invocations of the callback.
    layer_impl->ResetStateForUpdateSubmissionStateCallback();
    callback_layer_tree_host_changed_.Write(*this) = false;
  }
}

}  // namespace cc
