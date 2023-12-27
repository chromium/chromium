// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_TEST_API_H_
#define ASH_SHELF_SHELF_VIEW_TEST_API_H_

#include <optional>
#include <string>

#include "ash/public/cpp/shelf_item.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_tree_owner.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
class Rect;
}

namespace views {
class BoundsAnimator;
class View;
}

namespace ash {
class OverflowBubble;
class ShelfAppButton;
class ShelfButtonPressedMetricTracker;
class ShelfTooltipManager;
class ShelfView;

// Use the api in this class to test ShelfView.
class ShelfViewTestAPI {
 public:
  explicit ShelfViewTestAPI(ShelfView* shelf_view);

  ShelfViewTestAPI(const ShelfViewTestAPI&) = delete;
  ShelfViewTestAPI& operator=(const ShelfViewTestAPI&) = delete;

  ~ShelfViewTestAPI();

  // Number of icons displayed.
  size_t GetButtonCount();

  // Retrieve the button at |index|, doesn't support the home button,
  // because the home button is not a ShelfAppButton.
  ShelfAppButton* GetButton(int index);

  // Adds a new item of the given type to the view.
  ShelfID AddItem(ShelfItemType type);

  // Retrieve the view at |index|.
  views::View* GetViewAt(int index);

  // Gets current/ideal bounds for button at |index|.
  const gfx::Rect& GetBoundsByIndex(int index);
  const gfx::Rect& GetIdealBoundsByIndex(int index);

  // Makes shelf view show its overflow bubble.
  void ShowOverflowBubble();

  // Makes shelf view hide its overflow bubble.
  void HideOverflowBubble();

  // An accessor for the |bounds_animator_| duration.
  base::TimeDelta GetAnimationDuration() const;

  // Sets animation duration for test.
  void SetAnimationDuration(base::TimeDelta duration);

  // Runs message loop and waits until all add/remove animations are done for
  // the given bounds animator.
  void RunMessageLoopUntilAnimationsDone(
      views::BoundsAnimator* bounds_animator);

  // Runs message loop and waits until all add/remove animations are done on
  // the shelf view.
  void RunMessageLoopUntilAnimationsDone();

  // Gets the anchor point that would be used for a context menu with these
  // parameters.
  gfx::Rect GetMenuAnchorRect(const views::View& source,
                              const gfx::Point& location,
                              bool context_menu) const;

  // Close any open app list or context menu; returns true if a menu was closed.
  bool CloseMenu();

  // The union of all visible shelf item bounds.
  const gfx::Rect& visible_shelf_item_bounds_union() const;

  // An accessor for |shelf_view|.
  ShelfView* shelf_view() { return shelf_view_; }

  // An accessor for the shelf tooltip manager.
  ShelfTooltipManager* tooltip_manager();

  // An accessor for overflow bubble.
  OverflowBubble* overflow_bubble();

  // Returns minimum distance before drag starts.
  int GetMinimumDragDistance() const;

  // Wrapper for ShelfView::SameDragType.
  bool SameDragType(ShelfItemType typea, ShelfItemType typeb) const;

  // Returns re-insertable bounds in screen.
  gfx::Rect GetBoundsForDragInsertInScreen();

  // Returns true if item is ripped off.
  bool IsRippedOffFromShelf();

  // Returns true when an item is dragged from one shelf to another (eg.
  // overflow).
  bool DraggedItemToAnotherShelf();

  // An accessor for |shelf_button_pressed_metric_tracker_|.
  ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker();

  // Set callback which will run after showing shelf context menu.
  void SetShelfContextMenuCallback(base::RepeatingClosure closure);

  // Returns |separator_index_|.
  std::optional<size_t> GetSeparatorIndex() const;

  // Checks whether the separator is visible or not.
  bool IsSeparatorVisible() const;

  bool HasPendingPromiseAppRemoval(const std::string& promise_app_id) const;

 private:
  raw_ptr<ShelfView, DanglingUntriaged> shelf_view_;
  int id_ = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_TEST_API_H_
