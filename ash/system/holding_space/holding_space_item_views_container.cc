// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_container.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ui/compositor/callback_layer_animation_observer.h"

namespace ash {

namespace {

// Value returned during notification of animation completion events in order to
// delete the observer which provided notification.
constexpr bool kDeleteObserver = true;

}  // namespace

// HoldingSpaceItemViewsContainer ----------------------------------------------

HoldingSpaceItemViewsContainer::HoldingSpaceItemViewsContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  controller_observer_.Add(HoldingSpaceController::Get());
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

  // NOTE: `animate_in_observer` is deleted after `OnAnimateInCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_in_observer =
      new ui::CallbackLayerAnimationObserver(base::BindRepeating(
          &HoldingSpaceItemViewsContainer::OnAnimateInCompleted,
          base::Unretained(this)));

  AnimateIn(animate_in_observer);
  animate_in_observer->SetActive();
}

void HoldingSpaceItemViewsContainer::MaybeAnimateOut() {
  if (animation_state_ & AnimationState::kAnimatingOut)
    return;

  animation_state_ |= AnimationState::kAnimatingOut;

  // Don't allow event processing while animating out. The views being animated
  // out may be associated with holding space items that no longer exist and
  // so should not be acted upon by the user during this time.
  SetCanProcessEventsWithinSubtree(false);

  // NOTE: `animate_out_observer` is deleted after `OnAnimateOutCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_out_observer =
      new ui::CallbackLayerAnimationObserver(base::BindRepeating(
          &HoldingSpaceItemViewsContainer::OnAnimateOutCompleted,
          base::Unretained(this)));

  AnimateOut(animate_out_observer);
  animate_out_observer->SetActive();
}

bool HoldingSpaceItemViewsContainer::OnAnimateInCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingIn);
  animation_state_ &= ~AnimationState::kAnimatingIn;

  if (observer.aborted_count())
    return kDeleteObserver;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // Restore event processing that was disabled while animating out. The views
  // that have been animated in should all be associated with holding space
  // items that exist in the model.
  SetCanProcessEventsWithinSubtree(true);

  return kDeleteObserver;
}

bool HoldingSpaceItemViewsContainer::OnAnimateOutCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingOut);
  animation_state_ &= ~AnimationState::kAnimatingOut;

  if (observer.aborted_count())
    return kDeleteObserver;

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
    return kDeleteObserver;

  bool is_empty = true;

  for (const auto& item : model->items()) {
    if (item->IsFinalized() && WillAddHoldingSpaceItemView(item.get())) {
      AddHoldingSpaceItemView(item.get());
      is_empty = false;
    }
  }

  if (!is_empty)
    MaybeAnimateIn();

  return kDeleteObserver;
}

}  // namespace ash
