// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AutoclickScrollBubbleController;
class TrayBubbleView;

// Manages the bubble which contains an AutoclickMenuView.
class ASH_EXPORT AutoclickMenuBubbleController
    : public TrayBubbleView::Delegate,
      public LocaleChangeObserver {
 public:
  // The duration of the position change animation for the menu and scroll
  // bubbles in milliseconds.
  static const int kAnimationDurationMs = 150;

  AutoclickMenuBubbleController();

  AutoclickMenuBubbleController(const AutoclickMenuBubbleController&) = delete;
  AutoclickMenuBubbleController& operator=(
      const AutoclickMenuBubbleController&) = delete;

  ~AutoclickMenuBubbleController() override;

  // Sets the currently selected event type.
  void SetEventType(AutoclickEventType type);

  // Sets the menu's position on the screen.
  void SetPosition(FloatingMenuPosition position);

  // Set the scroll menu's position on the screen. The rect is the bounds of
  // the scrollable area, and the point is the user-selected scroll point.
  void SetScrollPosition(gfx::Rect scroll_bounds_in_dips,
                         const gfx::Point& scroll_point_in_dips);

  void ShowBubble(AutoclickEventType event_type, FloatingMenuPosition position);

  void CloseBubble();

  // Shows or hides the bubble.
  void SetBubbleVisibility(bool is_visible);

  // Performs a mouse event on the bubble at the given location in DIPs.
  void ClickOnBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Performs a mouse event on the scroll bubble at the given location in DIPs.
  void ClickOnScrollBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Whether the the bubble, if the bubble exists, contains the given screen
  // point.
  bool ContainsPointInScreen(const gfx::Point& point);

  // Whether the scroll bubble exists and contains the given screen point.
  bool ScrollBubbleContainsPointInScreen(const gfx::Point& point);

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  std::u16string GetAccessibleNameForBubble() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // LocaleChangeObserver:
  void OnLocaleChanged() override;

  // For tests only.
  views::Widget* GetBubbleWidgetForTesting() { return bubble_widget_; }
  void SetAnimateForTesting(bool animate) { animate_ = animate; }

 private:
  friend class AutoclickMenuBubbleControllerTest;
  friend class AutoclickTest;
  friend class AutoclickTestUtils;
  friend class FloatingAccessibilityControllerTest;

  // Owned by views hierarchy.
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<AutoclickMenuView> menu_view_ = nullptr;
  FloatingMenuPosition position_ = kDefaultAutoclickMenuPosition;

  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  // The controller for the scroll bubble. Only exists during a scroll. Owned
  // by this class so that positioning calculations can take place using both
  // classes at once.
  std::unique_ptr<AutoclickScrollBubbleController> scroll_bubble_controller_;

  // Whether to animate position changes. Can be set to false for testing.
  bool animate_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
