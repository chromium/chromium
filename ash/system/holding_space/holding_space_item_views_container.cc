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
    AddHoldingSpaceItemView(item.get());
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Remove(model);
  RemoveAllHoldingSpaceItemViews();
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  AddHoldingSpaceItemView(item);
}

void HoldingSpaceItemViewsContainer::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  RemoveHoldingSpaceItemView(item);
}

}  // namespace ash