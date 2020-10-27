// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_CONTAINER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/view.h"

namespace ash {

class HoldingSpaceItem;

class HoldingSpaceItemViewsContainer : public views::View,
                                       public HoldingSpaceControllerObserver,
                                       public HoldingSpaceModelObserver {
 public:
  HoldingSpaceItemViewsContainer();
  HoldingSpaceItemViewsContainer(const HoldingSpaceItemViewsContainer& other) =
      delete;
  HoldingSpaceItemViewsContainer& operator=(
      const HoldingSpaceItemViewsContainer& other) = delete;
  ~HoldingSpaceItemViewsContainer() override;

  virtual void AddHoldingSpaceItemView(const HoldingSpaceItem* item) = 0;
  virtual void RemoveAllHoldingSpaceItemViews() = 0;
  virtual void RemoveHoldingSpaceItemView(const HoldingSpaceItem* item) = 0;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

 private:
  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      controller_observer_{this};
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> model_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_CONTAINER_H_
