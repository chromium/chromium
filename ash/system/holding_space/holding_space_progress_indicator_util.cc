// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_indicator_util.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_item_updated_fields.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "base/memory/raw_ptr.h"

namespace ash {
namespace holding_space_util {
namespace {

// HoldingSpaceControllerProgressIndicator -------------------------------------

// A class owning a `ui::Layer` which paints indication of progress for all
// items in the model attached to its associated holding space `controller_`.
// NOTE: The owned `layer()` is not painted if there are no items in progress.
class HoldingSpaceControllerProgressIndicator
    : public ProgressIndicator,
      public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceControllerProgressIndicator(
      HoldingSpaceController* controller)
      : ProgressIndicator(
            HoldingSpaceAnimationRegistry::GetInstance(),
            ProgressIndicatorAnimationRegistry::AsAnimationKey(controller)),
        controller_(controller) {
    controller_observation_.Observe(controller_.get());
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

 private:
  // ProgressIndicator:
  std::optional<float> CalculateProgress() const override {
    // If there is no `model` attached, then there are no in-progress holding
    // space items. Do not paint the progress indication.
    const HoldingSpaceModel* model = controller_->model();
    if (!model)
      return kProgressComplete;

    HoldingSpaceProgress cumulative_progress;

    // Iterate over all holding space items.
    for (const auto& item : model->items()) {
      // Ignore any holding space items that are not yet initialized, since
      // they are not visible to the user, or items that are not visibly
      // in-progress, since they do not contribute to `cumulative_progress`.
      if (item->IsInitialized() && !item->progress().IsHidden() &&
          !item->progress().IsComplete()) {
        cumulative_progress += item->progress();
      }
    }

    return cumulative_progress.GetValue();
  }

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    model_observation_.Observe(model);
    InvalidateLayer();
  }

  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override {
    model_observation_.Reset();
    InvalidateLayer();
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item->IsInitialized() && !item->progress().IsComplete()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item->IsInitialized() && !item->progress().IsComplete()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override {
    if (item->IsInitialized() && updated_fields.previous_progress) {
      InvalidateLayer();
    }
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    if (!item->progress().IsComplete())
      InvalidateLayer();
  }

  // The associated holding space `controller_` for which to indicate progress
  // of all holding space items in its attached model.
  const raw_ptr<HoldingSpaceController> controller_;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observation_{this};

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};
};

// HoldingSpaceItemProgressIndicator -------------------------------------------

// A class owning a `ui::Layer` which paints indication of progress for its
// associated holding space `item_`. NOTE: The owned `layer()` is not painted if
// the associated `item_` is not in progress.
class HoldingSpaceItemProgressIndicator : public ProgressIndicator,
                                          public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceItemProgressIndicator(const HoldingSpaceItem* item)
      : ProgressIndicator(
            HoldingSpaceAnimationRegistry::GetInstance(),
            ProgressIndicatorAnimationRegistry::AsAnimationKey(item)),
        item_(item) {
    model_observation_.Observe(HoldingSpaceController::Get()->model());
  }

 private:
  // ProgressIndicator:
  std::optional<float> CalculateProgress() const override {
    // If `item_` is `nullptr` it is being destroyed. Ensure the progress
    // indication is not painted in this case. Similarly, ensure the progress
    // indication is not painted when progress is hidden.
    return item_ && !item_->progress().IsHidden() ? item_->progress().GetValue()
                                                  : kProgressComplete;
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(
      const HoldingSpaceItem* item,
      const HoldingSpaceItemUpdatedFields& updated_fields) override {
    if (item_ == item && updated_fields.previous_progress) {
      InvalidateLayer();
    }
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item_ == item) {
        item_ = nullptr;
        return;
      }
    }
  }

  // The associated holding space `item` for which to indicate progress.
  // NOTE: May temporarily be `nullptr` during the `item`s destruction sequence.
  raw_ptr<const HoldingSpaceItem> item_ = nullptr;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};
};

}  // namespace

// Utilities -------------------------------------------------------------------

std::unique_ptr<ProgressIndicator> CreateProgressIndicatorForController(
    HoldingSpaceController* controller) {
  return std::make_unique<HoldingSpaceControllerProgressIndicator>(controller);
}

std::unique_ptr<ProgressIndicator> CreateProgressIndicatorForItem(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemProgressIndicator>(item);
}

}  // namespace holding_space_util
}  // namespace ash
