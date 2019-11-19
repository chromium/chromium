// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_bubble.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "base/macros.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class Shelf;
class ShelfView;

// OverflowBubbleView hosts a ShelfView to display overflown items.
// Exports to access this class from OverflowBubbleViewTestAPI.
class ASH_EXPORT OverflowBubbleView : public ShelfBubble,
                                      public ShelfButtonDelegate {
 public:
  enum LayoutStrategy {
    // The arrow buttons are not shown. It means that there is enough space to
    // accommodate all of shelf icons.
    NOT_SHOW_ARROW_BUTTONS,

    // Only the left arrow button is shown.
    SHOW_LEFT_ARROW_BUTTON,

    // Only the right arrow button is shown.
    SHOW_RIGHT_ARROW_BUTTON,

    // Both buttons are shown.
    SHOW_BUTTONS
  };

  // |anchor| is the overflow button on the main shelf. |shelf_view| is the
  // ShelfView containing the overflow items.
  OverflowBubbleView(ShelfView* shelf_view,
                     views::View* anchor,
                     SkColor background_color);
  ~OverflowBubbleView() override;

  // Handles events for scrolling the bubble. Returns whether the event
  // has been consumed.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  // These return the actual offset (sometimes reduced by the clamping).
  // |animating| indicates whether animation is needed for scrolling. |x_offset|
  // or |y_offset| has to be float. Otherwise the slow gesture drag is neglected
  int ScrollByXOffset(float x_offset, bool animating);
  int ScrollByYOffset(float y_offset, bool animating);

  int GetFirstVisibleIndex() const;
  int GetLastVisibleIndex() const;

  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  bool CanActivate() const override;

  ShelfView* shelf_view() { return shelf_view_; }
  View* left_arrow() { return left_arrow_; }
  View* right_arrow() { return right_arrow_; }
  LayoutStrategy layout_strategy() const { return layout_strategy_; }
  gfx::Vector2dF scroll_offset() const { return scroll_offset_; }

  // ShelfBubble:
  bool ShouldCloseOnPressDown() override;
  bool ShouldCloseOnMouseExit() override;

  static int GetArrowButtonSize();

  // Padding at the two ends of the shelf in overflow mode.
  static constexpr int kEndPadding = 4;

  // Minimum margin around the bubble so that it doesn't hug the screen edges.
  static constexpr int kMinimumMargin = 8;

  static constexpr int kFadingZone = 16;

 private:
  friend class OverflowBubbleViewTestAPI;

  class OverflowScrollArrowView;
  class OverflowShelfContainerView;

  // Returns the maximum scroll distance.
  int CalculateScrollUpperBound() const;

  // Updates the overflow bubble view's layout strategy after scrolling by the
  // distance of |scroll|. Returns the adapted scroll offset.
  float CalculateLayoutStrategyAfterScroll(float scroll);

  // Ensures that the width of |bubble_bounds| (if it is not horizontally
  // aligned, adjust |bubble_bounds|'s height) is the multiple of the sum of
  // kShelfButtonSize and kShelfButtonSpacing. It helps that all of shelf icons
  // are fully visible.
  void AdjustToEnsureIconsFullyVisible(gfx::Rect* bubble_bounds) const;

  // Creates the animation for scrolling shelf by |scroll_distance|.
  void StartShelfScrollAnimation(float scroll_distance);

  // Update the layout strategy based on the available space.
  void UpdateLayoutStrategy();

  // Scrolls to a new page of shelf icons. |forward| indicates whether the next
  // page or previous page is shown.
  void ScrollToNewPage(bool forward);

  void ScrollToBeginning();
  void ScrollToEnd();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  const char* GetClassName() const override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // ui::EventHandler:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  Shelf* GetShelf();
  const Shelf* GetShelf() const;

  mutable LayoutStrategy layout_strategy_;

  // Child views Owned by views hierarchy.
  View* left_arrow_ = nullptr;
  View* right_arrow_ = nullptr;
  OverflowShelfContainerView* shelf_container_view_ = nullptr;

  // Not owned.
  ShelfView* shelf_view_;

  gfx::Vector2dF scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
