// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_

#include <memory>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"

namespace ash {

class HoldingSpaceController;
class HoldingSpaceItem;
class HoldingSpaceProgressRingAnimation;

// A class owning a `ui::Layer` which paints a ring to indicate progress.
// NOTE: The owned `layer()` is not painted if progress == `1.f`.
class HoldingSpaceProgressRing : public ui::LayerOwner,
                                 public ui::LayerDelegate {
 public:
  HoldingSpaceProgressRing(const HoldingSpaceProgressRing&) = delete;
  HoldingSpaceProgressRing& operator=(const HoldingSpaceProgressRing&) = delete;
  ~HoldingSpaceProgressRing() override;

  // Returns an instance which paints a ring to indicate progress of all holding
  // space items in the model attached to the specified `controller`.
  static std::unique_ptr<HoldingSpaceProgressRing> CreateForController(
      HoldingSpaceController* controller);

  // Returns an instance which paints a ring to indicate progress of the
  // specified holding space `item`.
  static std::unique_ptr<HoldingSpaceProgressRing> CreateForItem(
      const HoldingSpaceItem* item);

  // Invoke to schedule repaint of the entire `layer()`.
  void InvalidateLayer();

 protected:
  HoldingSpaceProgressRing();

  // Returns the calculated progress to paint to the owned `layer()`. This is
  // invoked during `UpdateVisualState()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  // NOTE: If progress == `1.f`, progress is complete and will not be painted.
  virtual absl::optional<float> CalculateProgress() const = 0;

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_scale, float new_scale) override;
  void OnPaintLayer(const ui::PaintContext& context) override;
  void UpdateVisualState() override;

  // Cached progress returned from `CalculateProgress()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  absl::optional<float> progress_;

  // Optional animation to paint in lieu of the determinate progress ring that
  // would otherwise be painted for the cached `progress_`.
  std::unique_ptr<HoldingSpaceProgressRingAnimation> animation_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
