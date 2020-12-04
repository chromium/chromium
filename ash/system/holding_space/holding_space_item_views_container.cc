// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_container.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

using AnimatableProperty = ui::LayerAnimationElement::AnimatableProperty;

// CallbackAnimationObserver ---------------------------------------------------

// An implicit animation observer which invokes a `callback` on animation
// completion. The `callback` will be notified whether the animation completed
// due to abort or if the animation completed normally.
class CallbackAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  using Callback = base::RepeatingCallback<void(bool aborted)>;

  explicit CallbackAnimationObserver(Callback callback) : callback_(callback) {}
  CallbackAnimationObserver(const CallbackAnimationObserver&) = delete;
  CallbackAnimationObserver& operator=(const CallbackAnimationObserver&) =
      delete;
  ~CallbackAnimationObserver() override = default;

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    bool aborted = false;
    for (int i = AnimatableProperty::FIRST_PROPERTY;
         i < AnimatableProperty::SENTINEL; ++i) {
      const AnimatableProperty property = static_cast<AnimatableProperty>(i);
      if (WasAnimationAbortedForProperty(property)) {
        aborted = true;
        break;
      }
    }
    callback_.Run(aborted);
  }

  Callback callback_;
};

}  // namespace

// HoldingSpaceItemViewsContainer ----------------------------------------------

HoldingSpaceItemViewsContainer::HoldingSpaceItemViewsContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate),
      animate_in_observer_(
          std::make_unique<CallbackAnimationObserver>(base::BindRepeating(
              &HoldingSpaceItemViewsContainer::OnAnimateInCompleted,
              base::Unretained(this)))),
      animate_out_observer_(
          std::make_unique<CallbackAnimationObserver>(base::BindRepeating(
              &HoldingSpaceItemViewsContainer::OnAnimateOutCompleted,
              base::Unretained(this)))) {
  controller_observer_.Add(HoldingSpaceController::Get());

  // The holding space views container will attach `animate_in_observer_` and
  // `animate_out_observer_` to a `ui::ScopedLayerAnimationSettings` associated
  // with itself in order to determine when animations are completed. To do so,
  // the holding space item views container must have a layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

HoldingSpaceItemViewsContainer::~HoldingSpaceItemViewsContainer() = default;

void HoldingSpaceItemViewsContainer::Reset() {
  model_observer_.RemoveAll();
  controller_observer_.RemoveAll();
}

void HoldingSpaceItemViewsContainer::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsContainer::ChildVisibilityChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  model_observer_.Add(model);
  for (const auto& item : model->items())
    OnHoldingSpaceItemAdded(item.get());
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Remove(model);
  if (ContainsHoldingSpaceItemViews())
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  if (!item->IsFinalized())
    return;
  if (WillAddHoldingSpaceItemView(item))
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  if (ContainsHoldingSpaceItemView(item))
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  if (WillAddHoldingSpaceItemView(item))
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsContainer::MaybeAnimateIn() {
  if (animation_state_ & AnimationState::kAnimatingIn)
    return;

  animation_state_ |= AnimationState::kAnimatingIn;

  // In the event that the call to `AnimateIn()` did not result in an animation
  // being scheduled, `OnAnimateInCompleted()` should still be called. To ensure
  // this occurs, add the animation observer to a scoped settings doing nothing.
  ui::ScopedLayerAnimationSettings animation_settings(layer()->GetAnimator());
  animation_settings.AddObserver(animate_in_observer_.get());

  AnimateIn(animate_in_observer_.get());
}

void HoldingSpaceItemViewsContainer::MaybeAnimateOut() {
  if (animation_state_ & AnimationState::kAnimatingOut)
    return;

  animation_state_ |= AnimationState::kAnimatingOut;

  // Don't allow event processing while animating out. The views being animated
  // out may be associated with holding space items that no longer exist and
  // so should not be acted upon by the user during this time.
  SetCanProcessEventsWithinSubtree(false);

  // In the event that the call to `AnimateOut()` did not result in an animation
  // being scheduled, `OnAnimateOutCompleted()` should still be called. To
  // ensure this occurs, add the animation observer to a scoped settings doing
  // nothing.
  ui::ScopedLayerAnimationSettings animation_settings(layer()->GetAnimator());
  animation_settings.AddObserver(animate_out_observer_.get());

  AnimateOut(animate_out_observer_.get());
}

void HoldingSpaceItemViewsContainer::OnAnimateInCompleted(bool aborted) {
  DCHECK(animation_state_ & AnimationState::kAnimatingIn);
  animation_state_ &= ~AnimationState::kAnimatingIn;

  if (aborted)
    return;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // Restore event processing that was disabled while animating out. The views
  // that have been animated in should all be associated with holding space
  // items that exist in the model.
  SetCanProcessEventsWithinSubtree(true);
}

void HoldingSpaceItemViewsContainer::OnAnimateOutCompleted(bool aborted) {
  DCHECK(animation_state_ & AnimationState::kAnimatingOut);
  animation_state_ &= ~AnimationState::kAnimatingOut;

  if (aborted)
    return;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // All holding space item views are going to be removed after which views will
  // be re-added for those items which still exist. A `ScopedSelectionRestore`
  // will serve to persist the current selection during this modification.
  HoldingSpaceItemViewDelegate::ScopedSelectionRestore scoped_selection_restore(
      delegate_);

  if (ContainsHoldingSpaceItemViews())
    RemoveAllHoldingSpaceItemViews();

  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (!model)
    return;

  bool is_empty = true;

  for (const auto& item : model->items()) {
    if (item->IsFinalized() && WillAddHoldingSpaceItemView(item.get())) {
      AddHoldingSpaceItemView(item.get());
      is_empty = false;
    }
  }

  if (!is_empty)
    MaybeAnimateIn();
}

}  // namespace ash
