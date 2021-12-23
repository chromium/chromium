// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_H_

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

// A class owning a `ui::Layer` which paints indication of progress.
// NOTE: The owned `layer()` is not painted if progress == `1.f`.
class HoldingSpaceProgressIndicator : public ui::LayerOwner,
                                      public ui::LayerDelegate {
 public:
  static constexpr char kClassName[] = "HoldingSpaceProgressIndicator";
  static constexpr float kProgressComplete = 1.f;

  HoldingSpaceProgressIndicator(const HoldingSpaceProgressIndicator&) = delete;
  HoldingSpaceProgressIndicator& operator=(
      const HoldingSpaceProgressIndicator&) = delete;
  ~HoldingSpaceProgressIndicator() override;

  // Returns an instance which paints indication of progress for all holding
  // space items in the model attached to the specified `controller`.
  static std::unique_ptr<HoldingSpaceProgressIndicator> CreateForController(
      HoldingSpaceController* controller);

  // Returns an instance which paints indication of progress for the specified
  // holding space `item`.
  static std::unique_ptr<HoldingSpaceProgressIndicator> CreateForItem(
      const HoldingSpaceItem* item);

  // Invoke to schedule repaint of the entire `layer()`.
  void InvalidateLayer();

  // Sets the visibility for this progress indicator's inner icon. Note that
  // the inner icon will only be painted while `progress_` is incomplete,
  // regardless of the value of `visible` provided.
  void SetInnerIconVisible(bool visible);
  bool inner_icon_visible() const { return inner_icon_visible_; }

 protected:
  // Each progress indicator is associated with an `animation_key_` which is
  // used to look up animations in the `HoldingSpaceAnimationRegistry`. When an
  // animation exists, it will be painted in lieu of the determinate progress
  // indication that would otherwise be painted for the cached `progress_`.
  explicit HoldingSpaceProgressIndicator(const void* animation_key);

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

  // Invoked when the ring `animation` associated with this progress indicator's
  // `animation_key_` has changed in the `HoldingSpaceAnimationRegistry`.
  // NOTE: The specified `animation` may be `nullptr`.
  void OnProgressRingAnimationChanged(
      HoldingSpaceProgressRingAnimation* animation);

  // The key for which to look up animations in the
  // `HoldingSpaceAnimationRegistry`. When an animation exists, it will be
  // painted in lieu of the determinate progress indication that would otherwise
  // be painted for the cached `progress_`.
  const void* const animation_key_;

  // A subscription to receive events when the ring animation associated with
  // this progress indicator's `animation_key_` has changed in the
  // `HoldingSpaceAnimationRegistry`.
  HoldingSpaceAnimationRegistry::ProgressRingAnimationChangedCallbackList::
      Subscription ring_animation_changed_subscription_;

  // A subscription to receive events on updates to the ring animation owned by
  // the `HoldingSpaceAnimationRegistry` which is associated with this progress
  // indicator's `animation_key_`. On ring animation update, the progress
  // indicator will `InvalidateLayer()` to trigger paint of the next animation
  // frame.
  base::RepeatingClosureList::Subscription ring_animation_updated_subscription_;

  // Cached progress returned from `CalculateProgress()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  absl::optional<float> progress_;

  // Whether this progress indicator's inner icon is visible. Note that the
  // inner icon will only be painted while `progress_` is incomplete, regardless
  // of this value.
  bool inner_icon_visible_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_H_
