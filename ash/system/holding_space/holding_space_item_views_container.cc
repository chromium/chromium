// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_container.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace ash {

HoldingSpaceItemViewsContainer::HoldingSpaceItemViewsContainer() {
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
  for (const auto& item : model->items()) {
    if (item->IsFinalized())
      AddHoldingSpaceItemView(item.get(), /*due_to_finalization=*/false);
  }
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Remove(model);
  RemoveAllHoldingSpaceItemViews();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  if (!item->IsFinalized())
    return;

  AddHoldingSpaceItemView(item, /*due_to_finalization=*/false);
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  RemoveHoldingSpaceItemView(item);
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  AddHoldingSpaceItemView(item, /*due_to_finalization=*/true);
}

}  // namespace ash
