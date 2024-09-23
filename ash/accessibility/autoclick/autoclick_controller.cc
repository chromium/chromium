// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_controller.h"

#include <tuple>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/autoclick/autoclick_drag_event_rewriter.h"
#include "ash/accessibility/autoclick/autoclick_ring_handler.h"
#include "ash/accessibility/autoclick/autoclick_scroll_position_handler.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The ratio of how long the gesture should take to begin after a dwell to the
// total amount of time of the dwell.
const float kStartGestureDelayRatio = 1 / 6.0;

// How much distance to travel with each generated scroll event.
const int kScrollDelta = 10;

bool IsModifierKey(const ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_SHIFT || key_code == ui::VKEY_LSHIFT ||
         key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
         key_code == ui::VKEY_RCONTROL || key_code == ui::VKEY_MENU ||
         key_code == ui::VKEY_LMENU || key_code == ui::VKEY_RMENU;
}

base::TimeDelta CalculateStartGestureDelay(base::TimeDelta total_delay) {
  return total_delay * kStartGestureDelayRatio;
}

views::Widget::InitParams CreateAutoclickOverlayWidgetParams(
    aura::Window* root_window) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  return params;
}

}  // namespace

// static.
base::TimeDelta AutoclickController::GetDefaultAutoclickDelay() {
  return base::Milliseconds(int64_t{kDefaultAutoclickDelayMs});
}

AutoclickController::AutoclickController()
    : delay_(GetDefaultAutoclickDelay()),
      autoclick_ring_handler_(std::make_unique<AutoclickRingHandler>()),
      drag_event_rewriter_(std::make_unique<AutoclickDragEventRewriter>()) {
  Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource()->AddEventRewriter(
      drag_event_rewriter_.get());
  Shell::Get()->cursor_manager()->AddObserver(this);
  InitClickTimers();
  UpdateRingSize();
}

AutoclickController::~AutoclickController() {
  // Clean up UI.
  HideScrollPosition();
  menu_bubble_controller_ = nullptr;
  CancelAutoclickAction();

  // This may be called during shutdown in which case some of the
  // ash objects may already be destroyed.
  auto* shell = Shell::Get();
  if (!shell) {
    return;
  }

  auto* cursor_manager = shell->cursor_manager();
  if (cursor_manager) {
    cursor_manager->RemoveObserver(this);
  }

  shell->RemovePreTargetHandler(this);
  SetTapDownTarget(nullptr);

  auto* root_window = Shell::GetPrimaryRootWindow();
  if (!root_window) {
    return;
  }

  auto* host = root_window->GetHost();
  if (!host) {
    return;
  }

  auto* event_source = host->GetEventSource();
  if (!event_source) {
    return;
  }

  event_source->RemoveEventRewriter(drag_event_rewriter_.get());
}

float AutoclickController::GetStartGestureDelayRatioForTesting() {
  return kStartGestureDelayRatio;
}

void AutoclickController::SetTapDownTarget(aura::Window* target) {
  if (tap_down_target_ == target)
    return;

  if (tap_down_target_)
    tap_down_target_->RemoveObserver(this);
  tap_down_target_ = target;
  if (tap_down_target_)
    tap_down_target_->AddObserver(this);
}

void AutoclickController::SetEnabled(bool enabled,
                                     bool show_confirmation_dialog) {
  if (enabled_ == enabled)
    return;

  if (enabled) {
    Shell::Get()->AddAccessibilityEventHandler(
        this, AccessibilityEventHandlerManager::HandlerType::kAutoclick);
    // Only create the bubble controller when needed. Most users will not enable
    // automatic clicks, so there's no need to use these unless the feature
    // is on.
    menu_bubble_controller_ = std::make_unique<AutoclickMenuBubbleController>();
    menu_bubble_controller_->ShowBubble(event_type_, menu_position_);
    if (event_type_ == AutoclickEventType::kScroll) {
      InitializeScrollLocation();
      UpdateScrollPosition();
    }
    enabled_ = enabled;
  } else {
    if (show_confirmation_dialog) {
      // If a dialog exists already, no need to show it again.
      if (disable_dialog_)
        return;
      // Show a confirmation dialog before disabling autoclick.
      auto* dialog = new AccessibilityFeatureDisableDialog(
          IDS_ASH_AUTOCLICK_DISABLE_CONFIRMATION_TEXT,
          // Callback for if the user accepts the dialog
          base::BindOnce([]() {
            // If they accept, actually disable autoclick.
            Shell::Get()->autoclick_controller()->SetEnabled(
                false, false /* do not show the dialog */);
          }),
          // Callback for if the user cancels the dialog - marks the
          // feature as enabled again in prefs.
          base::BindOnce([]() {
            // If they cancel, ensure autoclick is enabled.
            Shell::Get()->accessibility_controller()->autoclick().SetEnabled(
                true);
          }));
      disable_dialog_ = dialog->GetWeakPtr();
    } else {
      HideScrollPosition();
      Shell::Get()->RemoveAccessibilityEventHandler(this);
      menu_bubble_controller_ = nullptr;
      // Set the click type to left-click. This is the most useful click type
      // and users will want this type when they re-enable. If users were to
      // re-enable in scroll, or right-click, they would need to use the bubble
      // menu to change types.
      Shell::Get()->accessibility_controller()->SetAutoclickEventType(
          AutoclickEventType::kLeftClick);
      enabled_ = enabled;
    }
  }

  CancelAutoclickAction();
}

bool AutoclickController::IsEnabled() const {
  return enabled_;
}

void AutoclickController::SetAutoclickDelay(base::TimeDelta delay) {
  delay_ = delay;
  InitClickTimers();
}

void AutoclickController::SetAutoclickEventType(AutoclickEventType type) {
  if (menu_bubble_controller_)
    menu_bubble_controller_->SetEventType(type);

  if (type == AutoclickEventType::kScroll) {
    InitializeScrollLocation();
    UpdateScrollPosition();
  } else {
    over_scroll_button_ = false;
    HideScrollPosition();
  }

  if (event_type_ == type)
    return;
  CancelAutoclickAction();
  event_type_ = type;
}

void AutoclickController::SetMovementThreshold(int movement_threshold) {
  movement_threshold_ = movement_threshold;
  UpdateRingSize();
}

void AutoclickController::SetMenuPosition(FloatingMenuPosition menu_position) {
  menu_position_ = menu_position;
  UpdateAutoclickMenuBoundsIfNeeded();
}

void AutoclickController::DoScrollAction(ScrollPadAction action) {
  if (action == ScrollPadAction::kScrollClose) {
    // Set the scroll_location_ back to the default.
    scroll_location_ = gfx::Point(-kDefaultAutoclickMovementThreshold,
                                  -kDefaultAutoclickMovementThreshold);

    // Return to left click.
    event_type_ = AutoclickEventType::kLeftClick;
    Shell::Get()->accessibility_controller()->SetAutoclickEventType(
        event_type_);
    return;
  }
  // Otherwise, do a scroll action at the current scroll target point.
  float scroll_x = 0.0f;
  float scroll_y = 0.0f;
  switch (action) {
    case ScrollPadAction::kScrollUp:
      scroll_y = kScrollDelta;
      break;
    case ScrollPadAction::kScrollDown:
      scroll_y = -kScrollDelta;
      break;
    case ScrollPadAction::kScrollLeft:
      scroll_x = kScrollDelta;
      break;
    case ScrollPadAction::kScrollRight:
      scroll_x = -kScrollDelta;
      break;
    case ScrollPadAction::kScrollClose:
      NOTREACHED();
  }

  // Generate a scroll event at the current scroll location.
  aura::Window* root_window = window_util::GetRootWindowAt(scroll_location_);
  gfx::Point location_in_pixels(scroll_location_);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);
  ui::ScrollEvent scroll(
      ui::EventType::kScroll, gfx::PointF(location_in_pixels),
      gfx::PointF(location_in_pixels), ui::EventTimeForNow(),
      mouse_event_flags_, scroll_x, scroll_y, 0 /* x_offset_ordinal */,
      0 /* y_offset_ordinal */, 2 /* finger_count */);
  ui::MouseWheelEvent wheel(scroll);
  std::ignore = host->GetEventSink()->OnEventFromSource(&wheel);
}

void AutoclickController::OnEnteredScrollButton() {
  if (start_gesture_timer_)
    start_gesture_timer_->Stop();
  if (autoclick_timer_)
    autoclick_timer_->Stop();
  autoclick_ring_handler_->StopGesture();
  over_scroll_button_ = true;
}

void AutoclickController::OnExitedScrollButton() {
  over_scroll_button_ = false;
  // Reset the anchor_location_ so that gestures could begin immediately.
  anchor_location_ = gfx::Point(-kDefaultAutoclickMovementThreshold,
                                -kDefaultAutoclickMovementThreshold);
}

void AutoclickController::HandleAutoclickScrollableBoundsFound(
    const gfx::Rect& bounds_in_screen) {
  // The very first time scrollable bounds are found, the default first
  // position of the scrollbar to be next to the menu bubble.
  if (is_initial_scroll_location_)
    return;
  menu_bubble_controller_->SetScrollPosition(bounds_in_screen,
                                             scroll_location_);
  if (scrollable_bounds_callback_for_testing_) {
    scrollable_bounds_callback_for_testing_.Run(bounds_in_screen);
  }
}

void AutoclickController::UpdateAutoclickMenuBoundsIfNeeded() {
  if (menu_bubble_controller_)
    menu_bubble_controller_->SetPosition(menu_position_);
}

void AutoclickController::UpdateAutoclickWidgetPosition(
    gfx::NativeView native_view,
    aura::Window* root_window) {
  if (native_view->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        native_view,
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  }
}

void AutoclickController::DoAutoclickAction() {
  // The gesture_anchor_location_ is the position at which the animation is
  // anchored, and where the click should occur.
  aura::Window* root_window =
      window_util::GetRootWindowAt(gesture_anchor_location_);
  DCHECK(root_window) << "Root window not found while attempting autoclick.";

  // But if the thing that would be acted upon is an autoclick menu button, do a
  // fake click instead of whatever other action type we would have done. This
  // ensures that no matter the autoclick setting, users can always change to
  // another autoclick setting. By using a fake click we avoid closing dialogs
  // and menus, allowing autoclick users to interact with those items.
  if (!DragInProgress() &&
      AutoclickMenuContainsPoint(gesture_anchor_location_)) {
    menu_bubble_controller_->ClickOnBubble(gesture_anchor_location_,
                                           mouse_event_flags_);
    // Reset UI.
    CancelAutoclickAction();
    return;
  }

  // Set the in-progress event type locally so that if the event type is updated
  // in the middle of this event being executed it doesn't change execution.
  AutoclickEventType in_progress_event_type = event_type_;
  RecordUserAction(in_progress_event_type);

  if (in_progress_event_type == AutoclickEventType::kScroll) {
    // A dwell during a scroll.
    // Check if the event is over the scroll bubble controller, and if it is,
    // click on the scroll bubble.
    if (AutoclickScrollContainsPoint(gesture_anchor_location_)) {
      menu_bubble_controller_->ClickOnScrollBubble(gesture_anchor_location_,
                                                   mouse_event_flags_);
    } else {
      scroll_location_ = gesture_anchor_location_;
      is_initial_scroll_location_ = false;
      UpdateScrollPosition();
      Shell::Get()
          ->accessibility_controller()
          ->RequestAutoclickScrollableBoundsForPoint(scroll_location_);
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.ChangeScrollPoint"));
    }
    return;
  }

  gfx::Point location_in_pixels(gesture_anchor_location_);
  ::wm::ConvertPointFromScreen(root_window, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);

  bool drag_start =
      in_progress_event_type == AutoclickEventType::kDragAndDrop &&
      !drag_event_rewriter_->IsEnabled();
  bool drag_stop = DragInProgress();

  if (in_progress_event_type == AutoclickEventType::kLeftClick ||
      in_progress_event_type == AutoclickEventType::kRightClick ||
      in_progress_event_type == AutoclickEventType::kDoubleClick ||
      drag_start || drag_stop) {
    int button = in_progress_event_type == AutoclickEventType::kRightClick
                     ? ui::EF_RIGHT_MOUSE_BUTTON
                     : ui::EF_LEFT_MOUSE_BUTTON;

    ui::EventDispatchDetails details;
    if (!drag_stop) {
      // Left click, right click, double click, and beginning of a drag have
      // a pressed event next.
      ui::MouseEvent press_event(
          ui::EventType::kMousePressed, location_in_pixels, location_in_pixels,
          ui::EventTimeForNow(), mouse_event_flags_ | button, button);
      details = host->GetEventSink()->OnEventFromSource(&press_event);
      if (drag_start) {
        drag_event_rewriter_->SetEnabled(true);
        return;
      }
      if (details.dispatcher_destroyed) {
        OnActionCompleted(in_progress_event_type);
        return;
      }
    }
    if (drag_stop)
      drag_event_rewriter_->SetEnabled(false);
    ui::MouseEvent release_event(
        ui::EventType::kMouseReleased, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(), mouse_event_flags_ | button, button);
    details = host->GetEventSink()->OnEventFromSource(&release_event);

    // Now a single click, or half the drag & drop, has been completed.
    if (in_progress_event_type != AutoclickEventType::kDoubleClick ||
        details.dispatcher_destroyed) {
      OnActionCompleted(in_progress_event_type);
      return;
    }

    ui::MouseEvent double_press_event(
        ui::EventType::kMousePressed, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    ui::MouseEvent double_release_event(
        ui::EventType::kMouseReleased, location_in_pixels, location_in_pixels,
        ui::EventTimeForNow(),
        mouse_event_flags_ | button | ui::EF_IS_DOUBLE_CLICK, button);
    details = host->GetEventSink()->OnEventFromSource(&double_press_event);
    if (details.dispatcher_destroyed) {
      OnActionCompleted(in_progress_event_type);
      return;
    }
    details = host->GetEventSink()->OnEventFromSource(&double_release_event);
    OnActionCompleted(in_progress_event_type);
  }
}

void AutoclickController::StartAutoclickGesture() {
  if (event_type_ == AutoclickEventType::kNoAction) {
    // If we are set to "no action" and the gesture wouldn't occur over
    // the autoclick menu, cancel and return early rather than starting the
    // gesture.
    if (!AutoclickMenuContainsPoint(gesture_anchor_location_)) {
      CancelAutoclickAction();
      return;
    }
    // Otherwise, go ahead and start the gesture.
  }
  // The anchor is always the point in the screen where the timer starts, and is
  // used to determine when the cursor has moved far enough to cancel the
  // autoclick.
  anchor_location_ = gesture_anchor_location_;
  autoclick_ring_handler_->StartGesture(
      delay_ - CalculateStartGestureDelay(delay_), anchor_location_,
      ring_widget_.get());
  autoclick_timer_->Reset();
}

void AutoclickController::CancelAutoclickAction() {
  if (start_gesture_timer_)
    start_gesture_timer_->Stop();
  if (autoclick_timer_)
    autoclick_timer_->Stop();
  autoclick_ring_handler_->StopGesture();

  // If we are dragging, complete the drag, so as not to leave the UI in a
  // weird state.
  if (DragInProgress()) {
    DoAutoclickAction();
  }
  drag_event_rewriter_->SetEnabled(false);
  SetTapDownTarget(nullptr);
}

void AutoclickController::OnActionCompleted(
    AutoclickEventType completed_event_type) {
  // No need to change to left click if the setting is not enabled or the
  // event that just executed already was a left click.
  if (!revert_to_left_click_ || event_type_ == AutoclickEventType::kLeftClick ||
      completed_event_type == AutoclickEventType::kLeftClick) {
    return;
  }
  // Change the preference, but set it locally so we do not reset any state when
  // AutoclickController::SetAutoclickEventType is called.
  event_type_ = AutoclickEventType::kLeftClick;
  Shell::Get()->accessibility_controller()->SetAutoclickEventType(event_type_);
}

void AutoclickController::InitClickTimers() {
  CancelAutoclickAction();
  base::TimeDelta start_gesture_delay = CalculateStartGestureDelay(delay_);
  if (autoclick_timer_ && autoclick_timer_->IsRunning())
    autoclick_timer_->Stop();
  if (start_gesture_timer_ && start_gesture_timer_->IsRunning())
    start_gesture_timer_->Stop();
  autoclick_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, delay_ - start_gesture_delay,
      base::BindRepeating(&AutoclickController::DoAutoclickAction,
                          base::Unretained(this)));
  start_gesture_timer_ = std::make_unique<base::RetainingOneShotTimer>(
      FROM_HERE, start_gesture_delay,
      base::BindRepeating(&AutoclickController::StartAutoclickGesture,
                          base::Unretained(this)));
}

void AutoclickController::UpdateRingWidget() {
  aura::Window* const target =
      window_util::GetRootWindowAt(last_mouse_location_);
  SetTapDownTarget(target);
  aura::Window* const root_window = target->GetRootWindow();
  if (ring_widget_) {
    UpdateAutoclickWidgetPosition(ring_widget_->GetNativeView(), root_window);
  } else {
    ring_widget_ = std::make_unique<views::Widget>(
        CreateAutoclickOverlayWidgetParams(root_window));
    ring_widget_->SetOpacity(1.0f);
  }
}

void AutoclickController::UpdateRingSize() {
  autoclick_ring_handler_->SetSize(movement_threshold_);
}

void AutoclickController::InitializeScrollLocation() {
  // Sets the scroll location to the center of the root window.
  scroll_location_ =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint();
  is_initial_scroll_location_ = true;
  Shell::Get()
      ->accessibility_controller()
      ->RequestAutoclickScrollableBoundsForPoint(scroll_location_);
}

void AutoclickController::UpdateScrollPosition() {
  if (!enabled_)
    return;

  aura::Window* const target = window_util::GetRootWindowAt(scroll_location_);
  SetTapDownTarget(target);
  aura::Window* const root_window = target->GetRootWindow();
  if (autoclick_scroll_position_handler_) {
    UpdateAutoclickWidgetPosition(
        autoclick_scroll_position_handler_->GetNativeView(), root_window);
  } else {
    autoclick_scroll_position_handler_ =
        std::make_unique<AutoclickScrollPositionHandler>(
            std::make_unique<views::Widget>(
                CreateAutoclickOverlayWidgetParams(root_window)));
  }
  autoclick_scroll_position_handler_->SetScrollPointCenterInScreen(
      scroll_location_);
}

void AutoclickController::HideScrollPosition() {
  autoclick_scroll_position_handler_.reset();

  // TODO(katie): Clear any Autoclick scroll focus rings here.
}

bool AutoclickController::DragInProgress() const {
  return event_type_ == AutoclickEventType::kDragAndDrop &&
         drag_event_rewriter_->IsEnabled();
}

bool AutoclickController::AutoclickMenuContainsPoint(
    const gfx::Point& point) const {
  return menu_bubble_controller_ &&
         menu_bubble_controller_->ContainsPointInScreen(point);
}

bool AutoclickController::AutoclickScrollContainsPoint(
    const gfx::Point& point) const {
  return menu_bubble_controller_ &&
         menu_bubble_controller_->ScrollBubbleContainsPointInScreen(point);
}

void AutoclickController::RecordUserAction(
    AutoclickEventType event_type) const {
  switch (event_type) {
    case AutoclickEventType::kLeftClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.LeftClick"));
      return;
    case AutoclickEventType::kRightClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.RightClick"));
      return;
    case AutoclickEventType::kDoubleClick:
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.DoubleClick"));
      return;
    case AutoclickEventType::kDragAndDrop:
      // Only log drag-and-drop once per drag-and-drop. It takes two "dwells"
      // to complete a full drag-and-drop cycle, which could lead to double
      // the events logged.
      if (DragInProgress())
        return;
      base::RecordAction(
          base::UserMetricsAction("Accessibility.Autoclick.DragAndDrop"));
      return;
    case AutoclickEventType::kScroll:
      // Scroll users actions will be recorded from AutoclickScrollView.
    case AutoclickEventType::kNoAction:
      // No action shouldn't have a UserAction, so we return.
      return;
  }
}

void AutoclickController::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(event->target());
  if (event->type() == ui::EventType::kMouseCaptureChanged) {
    return;
  }
  last_mouse_location_ = event->target()->GetScreenLocation(*event);
  if (over_scroll_button_)
    return;
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED) &&
      (event->type() == ui::EventType::kMouseMoved ||
       (event->type() == ui::EventType::kMouseDragged &&
        drag_event_rewriter_->IsEnabled()))) {
    mouse_event_flags_ = event->flags();
    // Update the point even if the animation is not currently being shown.
    UpdateRingWidget();

    // The distance between the mouse location and the anchor location
    // must exceed a certain threshold to initiate a new autoclick countdown.
    // This ensures that mouse jitter caused by poor motor control does not
    // 1. initiate an unwanted autoclick from rest
    // 2. prevent the autoclick from ever occurring when the mouse
    //    arrives at the target.
    gfx::Vector2d delta = last_mouse_location_ - anchor_location_;
    if (delta.LengthSquared() >= movement_threshold_ * movement_threshold_) {
      anchor_location_ = last_mouse_location_;
      gesture_anchor_location_ = last_mouse_location_;
      // Stop all the timers, restarting the gesture timer only. This keeps
      // the animation from being drawn while the user is still moving quickly.
      start_gesture_timer_->Reset();
      if (autoclick_timer_) {
        autoclick_timer_->Stop();
      }
      autoclick_ring_handler_->StopGesture();
    } else if (start_gesture_timer_->IsRunning()) {
      // Keep track of where the gesture will be anchored.
      gesture_anchor_location_ = last_mouse_location_;
    } else if (autoclick_timer_->IsRunning() && !stabilize_click_position_) {
      // If we are not stabilizing the click position, update the gesture
      // center with each mouse move event.
      gesture_anchor_location_ = last_mouse_location_;
      autoclick_ring_handler_->SetGestureCenter(last_mouse_location_,
                                                ring_widget_.get());
    }
  } else if (event->type() == ui::EventType::kMousePressed ||
             event->type() == ui::EventType::kMouseReleased ||
             event->type() == ui::EventType::kMousewheel) {
    CancelAutoclickAction();
  }
}

void AutoclickController::OnKeyEvent(ui::KeyEvent* event) {
  int modifier_mask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                      ui::EF_IS_EXTENDED_KEY;
  int new_modifiers = event->flags() & modifier_mask;
  mouse_event_flags_ = (mouse_event_flags_ & ~modifier_mask) | new_modifiers;

  if (!IsModifierKey(event->key_code()))
    CancelAutoclickAction();
}

void AutoclickController::OnTouchEvent(ui::TouchEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnGestureEvent(ui::GestureEvent* event) {
  CancelAutoclickAction();
}

void AutoclickController::OnScrollEvent(ui::ScrollEvent* event) {
  // A single tap can create a scroll event, so ignore scroll starts and
  // cancels but cancel autoclicks when scrolls actually occur.
  if (event->type() == ui::EventType::kScrollFlingStart ||
      event->type() == ui::EventType::kScrollFlingCancel) {
    return;
  }
  CancelAutoclickAction();
}

void AutoclickController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(tap_down_target_, window);
  CancelAutoclickAction();
}

void AutoclickController::OnCursorVisibilityChanged(bool is_visible) {
  if (!menu_bubble_controller_)
    return;
  // TODO(katie): Check that the display which is fullscreen is the same as the
  // one containing the bubble, to determine whether to hide the bubble.
  // Currently just checking if the display under the mouse is fullscreen.
  aura::Window* window = GetWindowForFullscreenModeInRoot(
      window_util::GetRootWindowAt(last_mouse_location_));
  bool is_fullscreen = window != nullptr;

  // Hide the bubble when the cursor is gone in fullscreen mode.
  // Always show it otherwise.
  menu_bubble_controller_->SetBubbleVisibility(is_fullscreen ? is_visible
                                                             : true);
}

}  // namespace ash
