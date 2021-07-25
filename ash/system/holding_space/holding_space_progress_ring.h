// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_

#include <memory>
#include <vector>

#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "base/callback_list.h"
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
  // Each progress ring is associated with an `animation_key_` which is used
  // to look up animations in the `HoldingSpaceAnimationRegistry`. When an
  // animation exists, it will be painted in lieu of the determinate progress
  // ring that would otherwise be painted for the cached `progress_`.
  explicit HoldingSpaceProgressRing(const void* animation_key);

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

  // Invoked when the `animation` associated with this progress ring's
  // `animation_key_` has changed in the `HoldingSpaceAnimationRegistry`.
  // NOTE: The specified `animation` may be `nullptr`.
  void OnProgressRingAnimationChanged(
      HoldingSpaceProgressRingAnimation* animation);

  // The key for which to look up animations in the
  // `HoldingSpaceAnimationRegistry`. When an animation exists, it will be
  // painted in lieu of the determinate progress ring that would otherwise be
  // painted for the cached `progress_`.
  const void* const animation_key_;

  // A subscription to receive events when the animation associated with this
  // progress ring's `animation_key_` has changed in the
  // `HoldingSpaceAnimationRegistry`.
  HoldingSpaceAnimationRegistry::ProgressRingAnimationChangedCallbackList::
      Subscription animation_changed_subscription_;

  // A subscription to receive events on updates to the `animation_` owned by
  // the `HoldingSpaceAnimationRegistry` which is associated with this progress
  // ring's `animation_key_`. On `animation_` update, the progress ring will
  // `InvalidateLayer()` to trigger paint of the next animation frame.
  base::RepeatingClosureList::Subscription animation_updated_subscription_;

  // Cached progress returned from `CalculateProgress()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  absl::optional<float> progress_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_H_
