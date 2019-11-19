// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_TEST_API_H_
#define ASH_SHELF_SHELF_VIEW_TEST_API_H_

#include "ash/public/cpp/shelf_item.h"
#include "base/macros.h"
#include "ui/base/ui_base_types.h"

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
  ~ShelfViewTestAPI();

  // Number of icons displayed.
  int GetButtonCount();

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

 private:
  ShelfView* shelf_view_;
  int id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTestAPI);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_TEST_API_H_
