// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_

#include <memory>
#include <vector>

#include "ash/wm/overview/overview_types.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class RectF;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class OverviewGrid;
class OverviewHighlightableView;
class OverviewSession;
class RoundedLabelWidget;

// Defines the interface for the overview item which will be implemented by
// `OverviewItem` and `OverviewGroupItem`. The `OverviewGrid` owns the instance
// of this interface.
class OverviewItemBase {
 public:
  OverviewItemBase(OverviewSession* overview_session,
                   OverviewGrid* overview_grid,
                   aura::Window* root_window);
  OverviewItemBase(const OverviewItemBase&) = delete;
  OverviewItemBase& operator=(const OverviewItemBase&) = delete;
  virtual ~OverviewItemBase();

  // Creates an instance of the `OverviewItemBase` given the overview item
  // `type`.
  static std::unique_ptr<OverviewItemBase> Create(
      OverviewItemType type,
      aura::Window* window,
      OverviewSession* overview_session,
      OverviewGrid* overview_grid);

  // Creates `item_widget_`, which holds `overview_item_view_`.
  virtual void CreateItemWidget(OverviewItemType type) = 0;

  virtual views::Widget* GetItemWidget() = 0;

  // Returns the window(s) associated with this, which can be a single window or
  // a list of windows.
  virtual std::vector<aura::Window*> GetWindows() = 0;

  // Sets the bounds of this to `target_bounds` in the
  // `root_window_`. The bounds change will be animated as specified by
  // `animation_type`.
  virtual void SetBounds(const gfx::RectF& target_bounds,
                         OverviewAnimationType animation_type) = 0;

  // Returns the union of the original target bounds of all transformed windows
  // managed by `this`, i.e. all regular (normal or panel transient descendants
  // of the window returned by `GetWindows()`).
  virtual gfx::RectF GetTargetBoundsInScreen() const = 0;

  // Returns the contents view of this.
  virtual views::View* GetView() const = 0;

  // Returns the focusable view of this.
  virtual OverviewHighlightableView* GetFocusableView() const = 0;

  // Updates the rounded corners and shadow on this.
  virtual void UpdateRoundedCornersAndShadow() = 0;

  // Dispatched before entering overview.
  // TODO(b/294916205) : Remove this function for optimization.
  virtual void PrepareForOverview() = 0;

  // Called when the starting animation is completed, or called immediately
  // if there was no starting animation to do any necessary visual changes.
  virtual void OnStartingAnimationComplete() = 0;

  // Sends an accessibility event indicating that this window became selected
  // so that it is highlighted and announced.
  virtual void SendAccessibleSelectionEvent() = 0;

  virtual void OnOverviewItemDragStarted(OverviewItemBase* item) = 0;
  virtual void OnOverviewItemDragEnded(bool snap) = 0;

  // Shows/Hides window item during window dragging. Used when swiping up a
  // window from shelf.
  virtual void SetVisibleDuringItemDragging(bool visible, bool animate) = 0;

  // Shows the cannot snap warning if currently in splitview, and the associated
  // item cannot be snapped.
  virtual void UpdateCannotSnapWarningVisibility(bool animate) = 0;

  // This called when this is dragged and dropped on the mini view of
  // another desk, which prepares this item for being removed from the grid, and
  // the window(s) to restore its transform.
  virtual void OnMovingItemToAnotherDesk() = 0;

  // Updates and maybe creates the mirrors needed for multi display dragging.
  virtual void UpdateMirrorsForDragging(bool is_touch_dragging) = 0;

  // Resets the mirrors needed for multi display dragging.
  virtual void DestroyMirrorsForDragging() = 0;

  // Called when the `OverviewGrid` shuts down to reset the `item_widget_` and
  // remove window(s) from `ScopedOverviewHideWindows`.
  virtual void Shutdown() = 0;

  // Slides the item up or down and then closes the associated window(s). Used
  // by overview swipe to close.
  virtual void AnimateAndCloseItem(bool up) = 0;

 protected:
  // The root window this item is being displayed on.
  raw_ptr<aura::Window> root_window_;

  // Pointer to the overview session that owns the `OverviewGrid` containing
  // `this`. Guaranteed to be non-null for the lifetime of `this`.
  const raw_ptr<OverviewSession> overview_session_;

  // Pointer to the `OverviewGrid` that contains `this`. Guaranteed to be
  // non-null for the lifetime of `this`.
  const raw_ptr<OverviewGrid> overview_grid_;

  bool prepared_for_overview_ = false;

  // True if this overview item is currently being dragged around.
  bool is_being_dragged_ = false;

  // True when the item is dragged and dropped on another desk's mini view. This
  // causes it to restore its transform immediately without any animations,
  // since it is moving to an inactive desk, and therefore won't be visible.
  bool is_moving_to_another_desk_ = false;

  // True if the window(s) are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // `OverviewGrid::PositionWindows()`.
  bool animating_to_close_ = false;

  // True if the contained window(s) should animate during the exiting
  // animation.
  bool should_animate_when_exiting_ = true;

  // A widget with text that may show up on top of `transform_window_` to notify
  // users the window(s) cannot be snapped.
  std::unique_ptr<RoundedLabelWidget> cannot_snap_widget_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_BASE_H_
