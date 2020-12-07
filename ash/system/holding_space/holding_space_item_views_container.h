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

namespace ui {
class CallbackLayerAnimationObserver;
class LayerAnimationObserver;
}  // namespace ui

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;

class HoldingSpaceItemViewsContainer : public views::View,
                                       public HoldingSpaceControllerObserver,
                                       public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceItemViewsContainer(HoldingSpaceItemViewDelegate*);
  HoldingSpaceItemViewsContainer(const HoldingSpaceItemViewsContainer& other) =
      delete;
  HoldingSpaceItemViewsContainer& operator=(
      const HoldingSpaceItemViewsContainer& other) = delete;
  ~HoldingSpaceItemViewsContainer() override;

  // Resets the container. Called when the tray bubble starts closing to
  // stop observing the holding space controller/model to ensure that no new
  // items are created while the bubble widget is being asynchronously closed.
  void Reset();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

 protected:
  // Returns whether a view for the specified `item` exists in this holding
  // space item views container. Note that returning true will result in a call
  // to `RemoveAllHoldingSpaceItemViews()` after which views for existing
  // holding space items will be re-added via call to
  // `AddHoldingSpaceItemView()`.
  virtual bool ContainsHoldingSpaceItemView(const HoldingSpaceItem* item) = 0;

  // Returns whether any views associated with holding space items exist which
  // in this holding space item views container. Note that returning true will
  // result in a call to `RemoveAllHoldingSpaceItemViews()`.
  virtual bool ContainsHoldingSpaceItemViews() = 0;

  // Returns whether a view for the specified `item` will be added to this
  // holding space item views container. Note that `AddHoldingSpaceItemView()`
  // will only be invoked if this method returns true for the given `item`.
  virtual bool WillAddHoldingSpaceItemView(const HoldingSpaceItem* item) = 0;

  // Invoked to add a view to this holding space item views container for the
  // specified `item`.
  virtual void AddHoldingSpaceItemView(const HoldingSpaceItem* item) = 0;

  // Invoked to remove all views associated with holding space items from this
  // holding space item views container.
  virtual void RemoveAllHoldingSpaceItemViews() = 0;

  // Invoked to initiate animate in of the contents of this holding space item
  // views container. Any animations created must be associated with `observer`.
  virtual void AnimateIn(ui::LayerAnimationObserver* observer) = 0;

  // Invoked to initiate animate out of the contents of this holding space item
  // views container. Any animations created must be associated with `observer`.
  virtual void AnimateOut(ui::LayerAnimationObserver* observer) = 0;

  HoldingSpaceItemViewDelegate* delegate() { return delegate_; }

 private:
  enum AnimationState : uint32_t {
    kNotAnimating = 0,
    kAnimatingIn = 1 << 1,
    kAnimatingOut = 1 << 2,
  };

  // Invoke to start animating in the contents of this holding space item views
  // container. No-ops if animate in is already in progress.
  void MaybeAnimateIn();

  // Invoke to start animating out the contents of this holding space item views
  // container. No-ops if animate out is already in progress.
  void MaybeAnimateOut();

  // Invoked when an animate in/out of the contents of this holding space item
  // views container has been completed. These methods always return true to
  // delete the observer which notified the event.
  bool OnAnimateInCompleted(const ui::CallbackLayerAnimationObserver&);
  bool OnAnimateOutCompleted(const ui::CallbackLayerAnimationObserver&);

  HoldingSpaceItemViewDelegate* const delegate_;

  // Bit flag representation of current `AnimationState`. Note that it is
  // briefly possible to be both `kAnimatingIn` and `kAnimatingOut` when one
  // animation is preempting another.
  uint32_t animation_state_ = AnimationState::kNotAnimating;

  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      controller_observer_{this};
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> model_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEWS_CONTAINER_H_
