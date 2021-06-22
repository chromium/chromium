// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"

namespace ash {

class HoldingSpaceItem;

// A class owning a `ui::Layer` which paints a ring to indicate progress of its
// associated holding space `item`. NOTE: The `ui::Layer` is not painted if the
// holding space `item` is not in-progress.
class HoldingSpaceProgressRing : public ui::LayerOwner,
                                 public ui::LayerDelegate,
                                 public HoldingSpaceModelObserver {
 public:
  HoldingSpaceProgressRing(const HoldingSpaceItem* item,
                           bool use_light_mode_as_default);
  HoldingSpaceProgressRing(const HoldingSpaceProgressRing&) = delete;
  HoldingSpaceProgressRing& operator=(const HoldingSpaceProgressRing&) = delete;
  ~HoldingSpaceProgressRing() override;

  // Invoke to schedule repaint of the entire `layer()`.
  void InvalidateLayer();

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_scale, float new_scale) override;
  void OnPaintLayer(const ui::PaintContext& context) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;

  // The associated holding space `item` for which to indicate progress.
  // NOTE: May temporarily be `nullptr` during the `item`s destruction sequence.
  const HoldingSpaceItem* item_ = nullptr;

  // If `true`, the progress ring should be painted with light mode as the
  // default color mode. NOTE: This will have no effect if the dark/light mode
  // feature is enabled.
  const bool use_light_mode_as_default_;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
