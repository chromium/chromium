// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/raster_scale/raster_scale_layer_observer.h"

#include "ash/shell.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

#if DCHECK_IS_ON()
gfx::Transform AncestorTransform(aura::Window* window) {
  gfx::Transform transform;
  while (window->parent() != nullptr) {
    window = window->parent();
    transform = window->transform() * transform;
  }
  return transform;
}
#endif

void DLogIfAncestorTransformIsNotIdentity(aura::Window* window) {
  // Currently, TSR computes the raster scale purely from the local transform.
  // We don't expect there to be a non-identity transform for ancestors of TSR
  // applied windows, but check it in debug builds here. If there is a
  // non-identity transform, we need to include it somehow. The full (and
  // overcomplex) fix for this would be observing animations and window
  // transforms on every ancestor window and computing the maximum raster scale
  // for each span between each event point we are okay taking the compositor
  // lock at (e.g. animation start/stop on any ancestor window).
  DLOG_IF(ERROR, !AncestorTransform(window).IsIdentity())
      << "Unexpected non-identity transform from parent window coordinates to "
         "root window coordinates";
}

}  // namespace

ScopedRasterScaleLayerObserverLock::ScopedRasterScaleLayerObserverLock(
    base::WeakPtr<RasterScaleLayerObserver> observer)
    : observer_(observer) {
  observer_->IncrementRefCount();
}

ScopedRasterScaleLayerObserverLock::~ScopedRasterScaleLayerObserverLock() {
  if (observer_) {
    observer_->DecrementRefCount();
  }
}

ScopedRasterScaleLayerObserverLock::ScopedRasterScaleLayerObserverLock(
    ScopedRasterScaleLayerObserverLock&&) = default;
ScopedRasterScaleLayerObserverLock&
ScopedRasterScaleLayerObserverLock::operator=(
    ScopedRasterScaleLayerObserverLock&&) = default;

RasterScaleLayerObserver::RasterScaleLayerObserver(aura::Window* observe_window,
                                                   ui::Layer* observe_layer,
                                                   aura::Window* apply_window)
    : observe_window_(observe_window),
      observe_layer_(observe_layer),
      apply_window_(apply_window) {
  for (aura::Window* transient_child :
       GetTransientTreeIterator(apply_window_)) {
    transient_windows_.insert(transient_child);
  }
  if (observe_window->IsVisible()) {
    UpdateRasterScaleFromTransform(observe_layer_->transform());
  }
  if (!windows_observation_.IsObservingSource(observe_window_)) {
    windows_observation_.AddObservation(observe_window_);
  }
  if (!windows_observation_.IsObservingSource(apply_window_)) {
    windows_observation_.AddObservation(apply_window_);
  }
  observe_layer_->AddObserver(this);
  observe_layer_->GetAnimator()->AddObserver(this);
  aura::client::GetTransientWindowClient()->AddObserver(this);
}

RasterScaleLayerObserver::~RasterScaleLayerObserver() {
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
  if (observe_layer_) {
    observe_layer_->GetAnimator()->RemoveObserver(this);
    observe_layer_->RemoveObserver(this);
  }
}

void RasterScaleLayerObserver::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {
  animation_count_++;

  if (observe_window_ == nullptr || observe_layer_ == nullptr ||
      apply_window_ == nullptr) {
    return;
  }

  // It's complex to support more than one element in the
  // LayerAnimationSequence, because we would need to match raster scale
  // updates with the progression of the animation, which would need
  // framework changes. Also, it would introduce jank at each new element
  // during the animation, since changing the raster scale will take a
  // compositor lock to synchronize the raster scale update. Currently, the
  // only animation with at least one element affecting the transform and
  // with at least two elements that would apply to windows is the window
  // bounce animation, which we do not want to update raster scale for,
  // since it's very transient and doesn't change the scale much. So, we
  // will do nothing here if there is more than one element in the animation
  // sequence.
  if (sequence->size() > 1) {
    return;
  }

  ui::LayerAnimationElement::TargetValue value;
  sequence->FirstElement()->GetTargetValue(&value);

  // Don't do anything if we will never be visible.
  if (!observe_window_->IsVisible() && !value.visibility) {
    return;
  }

  // Don't do anything for things not animating the transform.
  if (!(sequence->properties() &
        ui::LayerAnimationElement::AnimatableProperty::TRANSFORM)) {
    return;
  }

  // Don't update if nothing happens until the animation is done:
  if (sequence->FirstElement()->tween_type() == gfx::Tween::ZERO) {
    return;
  }

  const auto new_scale =
      RasterScaleController::RasterScaleFromTransform(value.transform);
  const auto old_scale =
      Shell::Get()->raster_scale_controller()->ComputeRasterScaleForWindow(
          apply_window_);
  if (new_scale >= old_scale) {
    SetRasterScales(new_scale);
  }
}

void RasterScaleLayerObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  animation_count_--;

  if (animation_count_ == 0 && ref_count_ != 0) {
    // We can always apply no matter what here, since the animation is done.
    UpdateRasterScaleFromTransform(observe_layer_->transform());
  }

  MaybeShutdown();
}

void RasterScaleLayerObserver::OnLayerAnimationWillRepeat(
    ui::LayerAnimationSequence* sequence) {
  DLOG(ERROR) << "Unexpected repeating layer animation.";
}

void RasterScaleLayerObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  animation_count_--;
  MaybeShutdown();
}

void RasterScaleLayerObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

void RasterScaleLayerObserver::OnWindowVisibilityChanged(aura::Window* window,
                                                         bool visible) {
  if (observe_window_ == nullptr || observe_layer_ == nullptr ||
      apply_window_ == nullptr) {
    return;
  }

  if (window != observe_window_) {
    return;
  }

  UpdateRasterScaleFromTransform(observe_layer_->transform());
}

void RasterScaleLayerObserver::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  if (window == observe_window_) {
    observe_window_ = nullptr;
  }
  if (window == apply_window_) {
    apply_window_ = nullptr;
  }
  MaybeShutdown();
}

void RasterScaleLayerObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (observe_window_ == nullptr || observe_layer_ == nullptr ||
      apply_window_ == nullptr) {
    return;
  }

  if (window != observe_window_) {
    return;
  }

  // Needed for showing minimized windows via mirror view.
  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    UpdateRasterScaleFromTransform(observe_layer_->transform());
  }
}

void RasterScaleLayerObserver::OnWindowTransformed(
    aura::Window* window,
    ui::PropertyChangeReason reason) {
  if (observe_window_ == nullptr || observe_layer_ == nullptr ||
      apply_window_ == nullptr) {
    return;
  }

  if (window != observe_window_) {
    return;
  }

  // Transformations happening in an animation are handled separately.
  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    UpdateRasterScaleFromTransform(observe_layer_->transform());
  }
}

void RasterScaleLayerObserver::OnWindowLayerRecreated(aura::Window* window) {
  if (window != observe_window_) {
    return;
  }

  // OnWindowLayerRecreated shouldn't be called for mirror layers, so safe to
  // update here.
  if (observe_layer_) {
    observe_layer_->GetAnimator()->RemoveObserver(this);
    observe_layer_->RemoveObserver(this);
  }

  // Animations are not carried over to the new layer.
  animation_count_ = 0;

  observe_layer_ = window->layer();
  observe_layer_->AddObserver(this);
  observe_layer_->GetAnimator()->AddObserver(this);

  MaybeShutdown();
}

void RasterScaleLayerObserver::LayerDestroyed(ui::Layer* layer) {
  CHECK_EQ(layer, observe_layer_);
  observe_layer_ = nullptr;
  animation_count_ = 0;
  MaybeShutdown();
}

void RasterScaleLayerObserver::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != apply_window_ &&
      !::wm::HasTransientAncestor(parent, apply_window_)) {
    return;
  }

  transient_windows_.insert(transient_child);

  // Apply the existing raster scale to the new transient window.
  auto iter = raster_scales_.find(apply_window_);
  if (iter == raster_scales_.end()) {
    return;
  }

  const float raster_scale = iter->second->raster_scale();
  ScopedSetRasterScale::SetOrUpdateRasterScale(
      transient_child, raster_scale, &raster_scales_[transient_child]);
}

void RasterScaleLayerObserver::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != apply_window_ &&
      !wm::HasTransientAncestor(parent, apply_window_)) {
    return;
  }

  // Stop applying raster scale to the transient window and forget it.
  transient_windows_.erase(transient_child);
  raster_scales_.erase(transient_child);
}

void RasterScaleLayerObserver::SetRasterScales(float raster_scale) {
  DLogIfAncestorTransformIsNotIdentity(observe_window_);

  ScopedSetRasterScale::SetOrUpdateRasterScale(apply_window_, raster_scale,
                                               &raster_scales_[apply_window_]);
  for (aura::Window* window : transient_windows_) {
    ScopedSetRasterScale::SetOrUpdateRasterScale(window, raster_scale,
                                                 &raster_scales_[window]);
  }
}

void RasterScaleLayerObserver::UpdateRasterScaleFromTransform(
    const gfx::Transform& transform) {
  if (observe_window_ == nullptr || observe_layer_ == nullptr ||
      apply_window_ == nullptr) {
    return;
  }

  if (observe_window_->IsVisible()) {
    const auto raster_scale =
        RasterScaleController::RasterScaleFromTransform(transform);
    SetRasterScales(raster_scale);
  } else {
    // This content is not shown via the window, so stop holding a raster scale
    // lock on it.
    raster_scales_.clear();
  }
}

void RasterScaleLayerObserver::IncrementRefCount() {
  ref_count_++;
}

void RasterScaleLayerObserver::DecrementRefCount() {
  ref_count_--;
  MaybeShutdown();
}

void RasterScaleLayerObserver::MaybeShutdown() {
  // If there are no animations (we won't do anything in the future) and no more
  // refs, we can delete.
  if (ref_count_ != 0 || animation_count_ != 0) {
    return;
  }

  delete this;
}

}  // namespace ash
