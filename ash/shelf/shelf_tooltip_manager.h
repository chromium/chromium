// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
#define ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

namespace ash {
class ShelfBubble;
class Shelf;
class ShelfTooltipDelegate;

// ShelfTooltipManager manages the tooltip bubble that appears for shelf items.
class ASH_EXPORT ShelfTooltipManager : public ui::EventHandler,
                                       public ShelfObserver {
 public:
  explicit ShelfTooltipManager(Shelf* shelf);
  ~ShelfTooltipManager() override;

  // Closes the tooltip; uses an animation if |animate| is true.
  void Close(bool animate = true);

  // Returns true if the tooltip is currently visible.
  bool IsVisible() const;

  // Returns the view to which the tooltip bubble is anchored. May be null.
  views::View* GetCurrentAnchorView() const;

  // Show the tooltip bubble for the specified view.
  void ShowTooltip(views::View* view);
  void ShowTooltipWithDelay(views::View* view);

  // Set the timer delay in ms for testing.
  void set_timer_delay_for_test(int timer_delay) { timer_delay_ = timer_delay; }

  void set_shelf_tooltip_delegate(
      ShelfTooltipDelegate* shelf_tooltip_delegate) {
    DCHECK(!shelf_tooltip_delegate_ || !shelf_tooltip_delegate);

    shelf_tooltip_delegate_ = shelf_tooltip_delegate;
  }

 protected:
  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // ShelfObserver overrides:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

 private:
  friend class ShelfViewTest;
  friend class ShelfTooltipManagerTest;

  // A helper function to check for shelf visibility and view validity.
  bool ShouldShowTooltipForView(views::View* view);

  // A helper function to close the tooltip on mouse and touch press events.
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  int timer_delay_;
  base::OneShotTimer timer_;
  Shelf* shelf_ = nullptr;
  ShelfBubble* bubble_ = nullptr;

  ShelfTooltipDelegate* shelf_tooltip_delegate_ = nullptr;

  base::WeakPtrFactory<ShelfTooltipManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
