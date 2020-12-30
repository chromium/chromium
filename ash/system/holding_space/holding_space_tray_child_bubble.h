// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace ash {

class HoldingSpaceItemViewDelegate;
class HoldingSpaceItemViewsSection;

// Child bubble of the `HoldingSpaceTrayBubble`.
class HoldingSpaceTrayChildBubble : public views::View,
                                    public HoldingSpaceControllerObserver,
                                    public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceTrayChildBubble(HoldingSpaceItemViewDelegate* delegate);
  HoldingSpaceTrayChildBubble(const HoldingSpaceTrayChildBubble& other) =
      delete;
  HoldingSpaceTrayChildBubble& operator=(
      const HoldingSpaceTrayChildBubble& other) = delete;
  ~HoldingSpaceTrayChildBubble() override;

  // Initializes the child bubble.
  void Init();

  // Resets the child bubble. Called when the tray bubble starts closing to stop
  // observing the holding space controller/model to ensure that no new items
  // are created while the bubble widget is begin asynchronously closed.
  void Reset();

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

 protected:
  // Invoked to create the `sections_` for this child bubble.
  virtual std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
  CreateSections() = 0;

  HoldingSpaceItemViewDelegate* delegate() { return delegate_; }

 private:
  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // Invoked to animate in/out this view if necessary.
  void MaybeAnimateIn();
  void MaybeAnimateOut();

  // Invoked to animate in/out this view. These methods should only be called
  // from `MaybeAnimateIn()`/`MaybeAnimateOut()` respectively as those methods
  // contain gating criteria for when these methods may be invoked.
  void AnimateIn(ui::LayerAnimationObserver* observer);
  void AnimateOut(ui::LayerAnimationObserver* observer);

  // Invoked when an in/out animation has completed. If `aborted` is true,
  // the animation was cancelled and did not animation to target end values.
  void OnAnimateInCompleted(bool aborted);
  void OnAnimateOutCompleted(bool aborted);

  HoldingSpaceItemViewDelegate* const delegate_;

  // Views owned by view hierarchy.
  std::vector<HoldingSpaceItemViewsSection*> sections_;

  // Whether or not this view is currently being animated out.
  bool is_animating_out_ = false;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observer_{this};

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};

  base::WeakPtrFactory<HoldingSpaceTrayChildBubble> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
