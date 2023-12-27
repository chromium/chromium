// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_CONTROLLER_H_
#define ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/ash_constants.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class RetainingOneShotTimer;
}  // namespace base

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AccessibilityFeatureDisableDialog;
class AutoclickDragEventRewriter;
class AutoclickMenuBubbleController;
class AutoclickRingHandler;
class AutoclickScrollPositionHandler;

// Autoclick is one of the accessibility features. If enabled, two circles will
// animate at the mouse event location and an automatic mouse event event will
// happen after a certain amount of time at that location. The event type is
// determined by SetAutoclickEventType.
class ASH_EXPORT AutoclickController
    : public ui::EventHandler,
      public aura::WindowObserver,
      public aura::client::CursorClientObserver {
 public:
  // Autoclick's scroll event types.
  enum ScrollPadAction {
    kScrollUp = 1,
    kScrollDown = 2,
    kScrollLeft = 3,
    kScrollRight = 4,
    kScrollClose = 5,
  };

  AutoclickController();

  AutoclickController(const AutoclickController&) = delete;
  AutoclickController& operator=(const AutoclickController&) = delete;

  ~AutoclickController() override;

  // Set whether autoclicking is enabled. If |show_confirmation_dialog|, a
  // confirmation dialog will be shown when disabling autoclick to ensure
  // the user doesn't accidentally lock themselves out of the feature.
  void SetEnabled(bool enabled, bool show_confirmation_dialog);

  // Returns true if autoclicking is enabled.
  bool IsEnabled() const;

  // Set the time to wait in milliseconds from when the mouse stops moving
  // to when the autoclick event is sent.
  void SetAutoclickDelay(base::TimeDelta delay);

  // Gets the default wait time as a base::TimeDelta object.
  static base::TimeDelta GetDefaultAutoclickDelay();

  // Sets the event type.
  void SetAutoclickEventType(AutoclickEventType type);

  // Sets the movement threshold beyond which mouse movements cancel or begin
  // a new Autoclick event.
  void SetMovementThreshold(int movement_threshold);

  // Sets the menu position and updates the UI.
  void SetMenuPosition(FloatingMenuPosition menu_position);

  // Performs the given ScrollPadAction at the current scrolling point.
  void DoScrollAction(ScrollPadAction action);

  // The cursor is over a scroll (up/down/left/right) button.
  void OnEnteredScrollButton();

  // The cursor has exited a scroll (up/down/left/right) button.
  void OnExitedScrollButton();

  // The Accessibility Common extension has found scrollble bounds at the
  // current scroll point.
  void HandleAutoclickScrollableBoundsFound(const gfx::Rect& bounds_in_screen);

  // Update the bubble menu bounds if necessary to avoid system UI.
  void UpdateAutoclickMenuBoundsIfNeeded();

  // Sets whether to revert to a left click after any other event type.
  void set_revert_to_left_click(bool revert_to_left_click) {
    revert_to_left_click_ = revert_to_left_click;
  }

  // Sets whether to stabilize the cursor position during a click.
  // If |stabilize_position|, the click position will not change after the
  // autoclick timer and gesture animation begin, so long as the cursor does
  // not move outside of the movement threshold. If the position is not
  // stabilized, the cursor movements will translate into autoclick position
  // movements (but a cursor movement larger than the movement threshold from
  // the starting position will still cancel the click).
  void set_stabilize_click_position(bool stabilize_position) {
    stabilize_click_position_ = stabilize_position;
  }

  // Functionality for testing.
  static float GetStartGestureDelayRatioForTesting();
  AutoclickMenuBubbleController* GetMenuBubbleControllerForTesting() {
    return menu_bubble_controller_.get();
  }
  AccessibilityFeatureDisableDialog* GetDisableDialogForTesting() {
    return disable_dialog_.get();
  }
  void SetScrollableBoundsCallbackForTesting(
      base::RepeatingCallback<void(const gfx::Rect&)> callback) {
    scrollable_bounds_callback_for_testing_ = callback;
  }

 private:
  void SetTapDownTarget(aura::Window* target);
  void UpdateAutoclickWidgetPosition(gfx::NativeView native_view,
                                     aura::Window* root_window);
  void DoAutoclickAction();
  void StartAutoclickGesture();
  void CancelAutoclickAction();
  void OnActionCompleted(AutoclickEventType event_type);
  void InitClickTimers();
  void UpdateRingWidget();
  void UpdateRingSize();
  void InitializeScrollLocation();
  void UpdateScrollPosition();
  void HideScrollPosition();
  void RecordUserAction(AutoclickEventType event_type) const;
  bool DragInProgress() const;
  void CreateMenuBubbleController();
  bool AutoclickMenuContainsPoint(const gfx::Point& point) const;
  bool AutoclickScrollContainsPoint(const gfx::Point& point) const;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // aura::client::CursorClientObserver overrides:
  void OnCursorVisibilityChanged(bool is_visible) override;
  // TODO(katie): Override OnCursorDisplayChanged to move the autoclick
  // bubble menu to the same display as the cursor.

  // Whether Autoclick is currently enabled.
  bool enabled_ = false;
  AutoclickEventType event_type_ = kDefaultAutoclickEventType;
  bool revert_to_left_click_ = true;
  bool stabilize_click_position_ = false;
  int movement_threshold_ = kDefaultAutoclickMovementThreshold;
  // TODO(katie): The default position should flex with the user's choice of
  // language (RTL vs LTR) and shelf position, following the same behavior
  // as the volume slider bubble. However, once the user changes the position
  // manually, the position will be fixed regardless of language direction and
  // shelf position. This probably means adding a new AutoclickMenuPostion
  // enum for "system default".
  FloatingMenuPosition menu_position_ = kDefaultAutoclickMenuPosition;
  int mouse_event_flags_ = ui::EF_NONE;
  // The target window is observed by AutoclickController for the duration
  // of a autoclick gesture.
  raw_ptr<aura::Window> tap_down_target_ = nullptr;
  // The most recent mouse location.
  gfx::Point last_mouse_location_{-kDefaultAutoclickMovementThreshold,
                                  -kDefaultAutoclickMovementThreshold};
  // The position in screen coordinates used to determine the distance the
  // mouse has moved since dwell began. It is used to determine
  // if move events should cancel the gesture.
  gfx::Point anchor_location_{-kDefaultAutoclickMovementThreshold,
                              -kDefaultAutoclickMovementThreshold};
  // The position in screen coodinates tracking where the autoclick gesture
  // should be anchored. While the |start_gesture_timer_| is running and before
  // the animation is drawn, subtle mouse movements will update the
  // |gesture_anchor_location_|, so that once animation begins it can focus on
  // the most recent mose point.
  gfx::Point gesture_anchor_location_{-kDefaultAutoclickMovementThreshold,
                                      -kDefaultAutoclickMovementThreshold};
  // The point at which the next scroll event will occur.
  gfx::Point scroll_location_{-kDefaultAutoclickMovementThreshold,
                              -kDefaultAutoclickMovementThreshold};
  // Whether the current scroll_location_ is the initial one set automatically,
  // or if false, it was chosen explicitly by the user. The scroll bubble
  // positions are different in these two cases.
  bool is_initial_scroll_location_ = true;
  // Whether the cursor is currently over a scroll button. If true, new gestures
  // will not be started. This ensures the autoclick ring is not drawn over
  // the scroll position buttons, and extra clicks will not be generated there.
  bool over_scroll_button_ = false;
  base::RepeatingCallback<void(const gfx::Rect&)>
      scrollable_bounds_callback_for_testing_;

  // The widget containing the autoclick ring.
  std::unique_ptr<views::Widget> ring_widget_;
  base::TimeDelta delay_;
  // The timer that counts down from the beginning of a gesture until a click.
  std::unique_ptr<base::RetainingOneShotTimer> autoclick_timer_;
  // The timer that counts from when the user stops moving the mouse
  // until the start of the animated gesture. This keeps the animation from
  // showing up when the mouse cursor is moving quickly across the screen,
  // instead waiting for the mouse to begin a dwell.
  std::unique_ptr<base::RetainingOneShotTimer> start_gesture_timer_;
  std::unique_ptr<AutoclickRingHandler> autoclick_ring_handler_;
  std::unique_ptr<AutoclickScrollPositionHandler>
      autoclick_scroll_position_handler_;
  std::unique_ptr<AutoclickDragEventRewriter> drag_event_rewriter_;
  std::unique_ptr<AutoclickMenuBubbleController> menu_bubble_controller_;

  // Holds a weak pointer to the dialog shown when autoclick is being disabled.
  base::WeakPtr<AccessibilityFeatureDisableDialog> disable_dialog_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_AUTOCLICK_AUTOCLICK_CONTROLLER_H_
