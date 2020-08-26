// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef ASH_HOLDING_SPACE_HOLDING_SPACE_PRESENTER_H_
#define ASH_HOLDING_SPACE_HOLDING_SPACE_PRESENTER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/containers/flat_map.h"
#include "base/scoped_observer.h"

namespace ash {

class HoldingSpaceItem;

// Manages the temporary holding space UI for a root window.
// The main job of the class is
// *   to observe the holding space model and update item representations in the
//     holding space UI.
// *   to handle user actions within the holding space UI, and updates the
//     holding space model accordingly.
// NOTE: Currently this class only tracks the list of items within the active
// holding space model.
class ASH_EXPORT HoldingSpacePresenter : public HoldingSpaceControllerObserver,
                                         public HoldingSpaceModelObserver {
 public:
  HoldingSpacePresenter();
  HoldingSpacePresenter(const HoldingSpacePresenter& other) = delete;
  HoldingSpacePresenter& operator=(const HoldingSpacePresenter& other) = delete;
  ~HoldingSpacePresenter() override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

  const std::vector<std::string>& GetItemIds(HoldingSpaceItem::Type type) const;

 private:
  // Handles the items from the model, and starts observing the model.
  void HandleNewModel(HoldingSpaceModel* model);

  // IDs of items in the active holding space model, as observed by the holding
  // space presenter.
  base::flat_map<HoldingSpaceItem::Type, std::vector<std::string>> item_ids_;

  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      controller_observer_{this};
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> model_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_HOLDING_SPACE_HOLDING_SPACE_PRESENTER_H_
