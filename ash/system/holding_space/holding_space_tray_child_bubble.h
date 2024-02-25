// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace ash {

class HoldingSpaceItemView;
class HoldingSpaceItemViewsSection;
class HoldingSpaceViewDelegate;

// Child bubble of the `HoldingSpaceTrayBubble`.
class ASH_EXPORT HoldingSpaceTrayChildBubble
    : public views::View,
      public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
  METADATA_HEADER(HoldingSpaceTrayChildBubble, views::View)

 public:
  explicit HoldingSpaceTrayChildBubble(HoldingSpaceViewDelegate* delegate);
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

  // Returns all holding space item views in the child bubble. Views are
  // returned in top-to-bottom, left-to-right order (or mirrored for RTL).
  std::vector<HoldingSpaceItemView*> GetHoldingSpaceItemViews();

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override;

 protected:
  // Invoked to create the `sections_` for this child bubble.
  virtual std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
  CreateSections() = 0;

  // Invoked to create the `placeholder_` for this child bubble to be shown
  // when all `sections_` are not visible. Note that when a `placeholder_` is
  // provided, the child bubble will always be visible. When absent, the child
  // bubble will only be visible when one or more of its `sections_` are
  // visible.
  virtual std::unique_ptr<views::View> CreatePlaceholder();

  HoldingSpaceViewDelegate* delegate() { return delegate_; }

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

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

  const raw_ptr<HoldingSpaceViewDelegate, DanglingUntriaged> delegate_;

  // Views owned by view hierarchy.
  std::vector<raw_ptr<HoldingSpaceItemViewsSection, VectorExperimental>>
      sections_;
  raw_ptr<views::View> placeholder_ = nullptr;

  // Whether or not to ignore `ChildVisibilityChanged()` events. This is used
  // when removing all holding space item views from `sections_` to prevent this
  // view from inadvertently regaining visibility.
  bool ignore_child_visibility_changed_ = false;

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
