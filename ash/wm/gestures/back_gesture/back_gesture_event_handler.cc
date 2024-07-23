// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/gestures/back_gesture/back_gesture_affordance.h"
#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge_controller_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Distance from the divider's center point that reserved for splitview
// resizing in landscape orientation.
constexpr int kDistanceForSplitViewResize = 49;

// Called by CanStartGoingBack() to check whether we can start swiping from the
// split view divider to go back.
bool CanStartGoingBackFromSplitViewDivider(const gfx::Point& screen_location) {
  if (!IsCurrentScreenOrientationLandscape()) {
    return false;
  }

  // If virtual keyboard is visible when we swipe from the splitview divider
  // area, do not allow go back if the location is inside of the virtual
  // keyboard bounds.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsEnabled() &&
      keyboard_controller->GetVisualBoundsInScreen().Contains(
          screen_location)) {
    return false;
  }
  // Same thing for ARC virtual keyboard as well.
  SystemTrayModel* system_tray_model = Shell::Get()->system_tray_model();
  if (system_tray_model) {
    auto* virtual_keyboard = system_tray_model->virtual_keyboard();
    if (virtual_keyboard->arc_keyboard_visible() &&
        virtual_keyboard->arc_keyboard_bounds().Contains(screen_location)) {
      return false;
    }
  }

  auto* root_window = window_util::GetRootWindowAt(screen_location);
  auto* split_view_controller = SplitViewController::Get(root_window);
  if (!split_view_controller->InTabletSplitViewMode())
    return false;

  // Do not enable back gesture if |screen_location| is inside the extended
  // hotseat, let the hotseat handle the event instead.
  Shelf* shelf = Shelf::ForWindow(root_window);
  if (shelf->shelf_layout_manager()->hotseat_state() ==
          HotseatState::kExtended &&
      shelf->shelf_widget()
          ->hotseat_widget()
          ->GetWindowBoundsInScreen()
          .Contains(screen_location)) {
    return false;
  }

  // Do not enable back gesture if |screen_location| is inside the shelf widget,
  // let the shelf handle the event instead.
  if (shelf->shelf_widget()->GetWindowBoundsInScreen().Contains(
          screen_location)) {
    return false;
  }

  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false);
  const int y_center = divider_bounds.CenterPoint().y();
  // Do not enable back gesture if swiping starts from splitview divider's
  // resizable area.
  if (screen_location.y() >= (y_center - kDistanceForSplitViewResize) &&
      screen_location.y() <= (y_center + kDistanceForSplitViewResize)) {
    return false;
  }

  if (!base::i18n::IsRTL()) {
    divider_bounds.set_x(divider_bounds.x() - kSplitViewDividerExtraInset);
  } else {
    divider_bounds.set_x(divider_bounds.x() - kSplitViewDividerExtraInset -
                         BackGestureEventHandler::kStartGoingBackLeftEdgeInset);
  }

  divider_bounds.set_width(
      divider_bounds.width() + kSplitViewDividerExtraInset +
      BackGestureEventHandler::kStartGoingBackLeftEdgeInset);
  return divider_bounds.Contains(screen_location);
}

// Activate the given |window|.
void ActivateWindow(aura::Window* window) {
  if (!window)
    return;
  WindowState::Get(window)->Activate();
}

// Activate the snapped window that is underneath the start |location| for back
// gesture. This is necessary since the snapped window that is underneath is not
// always the current active window.
void ActivateUnderneathWindowInSplitViewMode(
    const gfx::Point& location,
    bool dragged_from_splitview_divider) {
  auto* split_view_controller =
      SplitViewController::Get(window_util::GetRootWindowAt(location));
  if (!split_view_controller->InTabletSplitViewMode())
    return;

  const bool is_rtl = base::i18n::IsRTL();
  auto* left_window = is_rtl ? split_view_controller->secondary_window()
                             : split_view_controller->primary_window();
  auto* right_window = is_rtl ? split_view_controller->primary_window()
                              : split_view_controller->secondary_window();
  const chromeos::OrientationType current_orientation =
      GetCurrentScreenOrientation();
  if (current_orientation == chromeos::OrientationType::kLandscapePrimary) {
    ActivateWindow(dragged_from_splitview_divider ? right_window : left_window);
  } else if (current_orientation ==
             chromeos::OrientationType::kLandscapeSecondary) {
    ActivateWindow(dragged_from_splitview_divider ? left_window : right_window);
  } else {
    if (left_window &&
        split_view_controller
            ->GetSnappedWindowBoundsInScreen(
                SnapPosition::kPrimary,
                /*window_for_minimum_size=*/nullptr,
                chromeos::kDefaultSnapRatio, /*account_for_divider_width=*/true)
            .Contains(location)) {
      ActivateWindow(left_window);
    } else if (right_window && split_view_controller
                                   ->GetSnappedWindowBoundsInScreen(
                                       SnapPosition::kSecondary,
                                       /*window_for_minimum_size=*/nullptr,
                                       chromeos::kDefaultSnapRatio,
                                       /*account_for_divider_width=*/true)
                                   .Contains(location)) {
      ActivateWindow(right_window);
    } else if (split_view_controller->split_view_divider()
                   ->GetDividerBoundsInScreen(
                       /*is_dragging=*/false)
                   .Contains(location)) {
      // Activate the window that above the splitview divider if back gesture
      // starts from splitview divider.
      ActivateWindow(IsCurrentScreenOrientationPrimary() ? left_window
                                                         : right_window);
    }
  }
}

}  // namespace

BackGestureEventHandler::BackGestureEventHandler() {
  if (features::IsHideShelfControlsInTabletModeEnabled()) {
    nudge_controller_ =
        std::make_unique<BackGestureContextualNudgeControllerImpl>();
  }
}

BackGestureEventHandler::~BackGestureEventHandler() = default;

void BackGestureEventHandler::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // Cancel the left edge swipe back during screen rotation.
  if (metrics & DISPLAY_METRIC_ROTATION) {
    back_gesture_affordance_.reset();
    going_back_started_ = false;
  }
}

void BackGestureEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  // Do not handle gesture events that are not generated from |first_touch_id_|.
  if (base::Contains(other_touch_event_ids_list_,
                     event->unique_touch_event_id())) {
    return;
  }

  if (should_wait_for_touch_ack_) {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    gfx::Point screen_location = event->location();
    ::wm::ConvertPointToScreen(target, &screen_location);
    if (MaybeHandleBackGesture(event, screen_location))
      event->StopPropagation();

    // Reset |should_wait_for_touch_ack_| for the last gesture event in the
    // sequence.
    if (event->type() == ui::EventType::kGestureEnd) {
      should_wait_for_touch_ack_ = false;
    }
  }
}

void BackGestureEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  // Do not handle PEN and ERASER events for back gesture. PEN events can come
  // from stylus device.
  if (event->pointer_details().pointer_type == ui::EventPointerType::kPen ||
      event->pointer_details().pointer_type == ui::EventPointerType::kEraser) {
    return;
  }

  // Update |first_touch_id_| on first EventType::kTouchPressed only.
  // EventType::kTouchPressed type check is needed because there could be
  // EventType::kTouchCancelled after EventType::kTouchReleased event.
  if (first_touch_id_ == ui::kPointerIdUnknown &&
      event->type() == ui::EventType::kTouchPressed) {
    first_touch_id_ = event->pointer_details().id;
  }

  if (event->pointer_details().id != first_touch_id_) {
    other_touch_event_ids_list_.insert(event->unique_event_id());
    return;
  }

  if (event->type() == ui::EventType::kTouchReleased) {
    first_touch_id_ = ui::kPointerIdUnknown;
    other_touch_event_ids_list_.clear();
  }

  if (event->type() == ui::EventType::kTouchPressed) {
    x_drag_amount_ = y_drag_amount_ = 0;
    during_reverse_dragging_ = false;
  } else {
    // TODO(oshima): Convert to PointF/float.
    const gfx::Point current_location = event->location();
    x_drag_amount_ += (current_location.x() - last_touch_point_.x());
    y_drag_amount_ += (current_location.y() - last_touch_point_.y());

    // Do not update |during_reverse_dragging_| if touch point's location
    // doesn't change.
    if (current_location.x() != last_touch_point_.x()) {
      during_reverse_dragging_ = current_location.x() < last_touch_point_.x();
      if (base::i18n::IsRTL())
        during_reverse_dragging_ = !during_reverse_dragging_;
    }
  }
  last_touch_point_ = event->location();

  // Get the event target from TouchEvent since target of the GestureEvent
  // from GetAndResetPendingGestures is nullptr. The coordinate conversion is
  // done outside the loop as the previous gesture events in a sequence may
  // invalidate the target, for example given a sequence of
  // {EventType::kGestureScrollEnd, EventType::kGestureEnd} on a non-resizable
  // window, the first gesture will trigger a minimize event which will delete
  // the backdrop, which was the target. See http://crbug.com/1064618.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point screen_location = event->location();
  ::wm::ConvertPointToScreen(target, &screen_location);

  if (event->type() == ui::EventType::kTouchPressed &&
      ShouldWaitForTouchPressAck(screen_location)) {
    should_wait_for_touch_ack_ = true;
    return;
  }

  if (!should_wait_for_touch_ack_) {
    ui::TouchEvent touch_event_copy = *event;
    if (!gesture_provider_.OnTouchEvent(&touch_event_copy))
      return;

    gesture_provider_.OnTouchEventAck(
        touch_event_copy.unique_event_id(), /*event_consumed=*/false,
        /*is_source_touch_event_set_blocking=*/false);

    std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
        gesture_provider_.GetAndResetPendingGestures();
    for (const auto& gesture : gestures) {
      if (MaybeHandleBackGesture(gesture.get(), screen_location))
        event->StopPropagation();
    }
  }
}

const std::string& BackGestureEventHandler::GetName() const {
  static const std::string name("BackGestureEventHandler");
  return name;
}

void BackGestureEventHandler::OnGestureEvent(GestureConsumer* consumer,
                                             ui::GestureEvent* event) {
  // Gesture events here are generated by |gesture_provider_|, and they're
  // handled at OnTouchEvent() by calling MaybeHandleBackGesture().
}

bool BackGestureEventHandler::MaybeHandleBackGesture(
    ui::GestureEvent* event,
    const gfx::Point& screen_location) {
  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
      going_back_started_ = CanStartGoingBack(screen_location);
      if (!going_back_started_)
        break;
      back_gesture_affordance_ = std::make_unique<BackGestureAffordance>(
          screen_location, dragged_from_splitview_divider_);
      if (features::IsHideShelfControlsInTabletModeEnabled()) {
        // Cancel the in-waiting or in-progress back nudge animation.
        nudge_controller_->OnBackGestureStarted();
        contextual_tooltip::HandleGesturePerformed(
            Shell::Get()->session_controller()->GetActivePrefService(),
            contextual_tooltip::TooltipType::kBackGesture);
      }
      return true;
    case ui::EventType::kGestureScrollBegin:
      if (!going_back_started_)
        break;
      back_start_location_ = screen_location;

      base::RecordAction(base::UserMetricsAction("Ash_Tablet_BackGesture"));
      back_gesture_start_scenario_type_ = GetStartScenarioType(
          dragged_from_splitview_divider_, back_start_location_);
      RecordStartScenarioType(back_gesture_start_scenario_type_);
      break;
    case ui::EventType::kGestureScrollUpdate:
      if (!going_back_started_)
        break;
      CHECK(back_gesture_affordance_);
      back_gesture_affordance_->Update(x_drag_amount_, y_drag_amount_,
                                       during_reverse_dragging_);
      return true;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kGestureEnd: {
      if (!going_back_started_)
        break;
      CHECK(back_gesture_affordance_);
      // Complete the back gesture if the affordance is activated or fling
      // with large enough velocity. Note, complete can be different actions
      // while in different scenarios, but always fading out the affordance at
      // the end.
      SCOPED_CRASH_KEY_BOOL("286590216", "back_gesture_affordance_1",
                            back_gesture_affordance_ != nullptr);
      SCOPED_CRASH_KEY_BOOL("286590216", "going_back_started_1",
                            going_back_started_);
      SCOPED_CRASH_KEY_NUMBER("286590216", "event.type",
                              base::to_underlying(event->type()));
      if (back_gesture_affordance_->IsActivated() ||
          (event->type() == ui::EventType::kScrollFlingStart &&
           event->details().velocity_x() >= kFlingVelocityForGoingBack)) {
        auto* shell = Shell::Get();
        if (!keyboard_util::CloseKeyboardIfActive()) {
          ActivateUnderneathWindowInSplitViewMode(
              back_start_location_, dragged_from_splitview_divider_);
          if (shell->app_list_controller()->IsHomeScreenVisible()) {
            DCHECK(shell->app_list_controller()->GetAppListViewState() ==
                   AppListViewState::kFullscreenSearch);
            // Exit home screen search and go back to home screen all apps page.
            shell->app_list_controller()->Back();
          } else {
            auto* top_window = window_util::GetTopWindow();
            auto* top_window_state = WindowState::Get(top_window);
            if (top_window_state && top_window_state->IsFullscreen() &&
                !shell->overview_controller()->InOverviewSession()) {
              // For fullscreen ARC apps, show the hotseat and shelf on the
              // first back swipe, and send a back event on the second back
              // swipe. For other fullscreen apps, exit fullscreen.
              const bool arc_app =
                  top_window_state->window()->GetProperty(
                      chromeos::kAppTypeKey) == chromeos::AppType::ARC_APP;
              if (arc_app) {
                // Go back to the previous page if the shelf was already shown,
                // otherwise record as showing shelf.
                if (Shelf::ForWindow(top_window_state->window())->IsVisible()) {
                  SendBackEvent(screen_location);
                } else {
                  Shelf::ForWindow(top_window_state->window())
                      ->shelf_layout_manager()
                      ->UpdateVisibilityStateForBackGesture();
                  RecordEndScenarioType(
                      BackGestureEndScenarioType::kShowShelfAndHotseat);
                }
              } else {
                // Complete as exiting the fullscreen mode of the underneath
                // window.
                const WMEvent wm_event(WM_EVENT_TOGGLE_FULLSCREEN);
                top_window_state->OnWMEvent(&wm_event);
                RecordEndScenarioType(
                    BackGestureEndScenarioType::kExitFullscreen);
              }
            } else if (window_util::ShouldMinimizeTopWindowOnBack()) {
              // Complete as minimizing the underneath window.
              top_window_state->Minimize();
              RecordEndScenarioType(
                  GetEndScenarioType(back_gesture_start_scenario_type_,
                                     BackGestureEndType::kMinimize));
            } else {
              // Complete as going back to the previous page of the underneath
              // window.
              SendBackEvent(screen_location);
            }
            // |top_window| could be nullptr while in overview mode since back
            // gesture is allowed in overview mode even no window opens.
            if (top_window) {
              RecordUnderneathWindowType(
                  GetUnderneathWindowType(back_gesture_start_scenario_type_));
            }
          }
        }
        SCOPED_CRASH_KEY_BOOL("286590216", "back_gesture_affordance_2",
                              back_gesture_affordance_ != nullptr);
        SCOPED_CRASH_KEY_BOOL("286590216", "going_back_started_2",
                              going_back_started_);
        // `back_gesture_affordance_` could be reset after receiving the event,
        // depends on the timing that receives the display rotation
        // notification.
        if (back_gesture_affordance_) {
          back_gesture_affordance_->Complete();
        }
      } else {
        back_gesture_affordance_->Abort();
        RecordEndScenarioType(GetEndScenarioType(
            back_gesture_start_scenario_type_, BackGestureEndType::kAbort));
      }
      going_back_started_ = false;
      dragged_from_splitview_divider_ = false;

      return true;
    }
    default:
      break;
  }

  return going_back_started_;
}

bool BackGestureEventHandler::CanStartGoingBack(
    const gfx::Point& screen_location) {
  Shell* shell = Shell::Get();
  // Do not enable back gesture in Kiosk mode, as we are never supposed to leave
  // fullscreen mode there.
  if (shell->session_controller()->IsRunningInAppMode())
    return false;

  if (!display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  // Do not enable back gesture if it is not in an ACTIVE session. e.g, login
  // screen, lock screen.
  if (shell->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Do not enable back gesture if `screen_location` is inside the tuck handle,
  // let `FloatController` handle the event instead.
  if (aura::Window* floated_window =
          window_util::GetFloatedWindowForActiveDesk()) {
    auto* float_controller = Shell::Get()->float_controller();
    if (float_controller->IsFloatedWindowTuckedForTablet(floated_window)) {
      auto* tuck_handle_widget =
          float_controller->GetTuckHandleWidget(floated_window);
      if (tuck_handle_widget &&
          tuck_handle_widget->GetWindowBoundsInScreen().Contains(
              screen_location)) {
        return false;
      }
    }
  }

  gfx::Rect hit_bounds_in_screen(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              window_util::GetRootWindowAt(screen_location))
          .work_area());
  if (base::i18n::IsRTL()) {
    hit_bounds_in_screen.set_x(hit_bounds_in_screen.right() -
                               kStartGoingBackLeftEdgeInset);
  }

  hit_bounds_in_screen.set_width(kStartGoingBackLeftEdgeInset);
  const bool hit_in_limited_area =
      hit_bounds_in_screen.Contains(screen_location);

  const bool is_home_launcher_visible =
      shell->app_list_controller()->IsHomeScreenVisible();
  const bool is_fullscreen_search_state =
      shell->app_list_controller()->GetAppListViewState() ==
      AppListViewState::kFullscreenSearch;

  // Enable back gesture if it is in home screen |kFullScreenSearch| state and
  // swipe from the left limited area. Otherwise, always disable the back
  // gesture when it is in home screen.
  if (is_home_launcher_visible) {
    if (is_fullscreen_search_state)
      return hit_in_limited_area;
    return false;
  }

  aura::Window* top_window = window_util::GetTopWindow();
  // Do not enable back gesture if MRU window list is empty and it is not in
  // overview mode.
  if (!top_window && !shell->overview_controller()->InOverviewSession())
    return false;

  // If the event location falls into the window's gesture exclusion zone, do
  // not handle it.
  for (aura::Window* window = top_window; window; window = window->parent()) {
    SkRegion* gesture_exclusion =
        window->GetProperty(kSystemGestureExclusionKey);
    if (gesture_exclusion) {
      gfx::Point location_in_window = screen_location;
      ::wm::ConvertPointFromScreen(window, &location_in_window);
      if (gesture_exclusion->contains(location_in_window.x(),
                                      location_in_window.y())) {
        return false;
      }
    }
  }

  // If the target window does not allow touch action, do not handle it.
  if (!Shell::Get()->shell_delegate()->AllowDefaultTouchActions(top_window))
    return false;

  if (hit_in_limited_area)
    return true;

  dragged_from_splitview_divider_ =
      CanStartGoingBackFromSplitViewDivider(screen_location);
  return dragged_from_splitview_divider_;
}

void BackGestureEventHandler::SendBackEvent(const gfx::Point& screen_location) {
  window_util::SendBackKeyEvent(window_util::GetRootWindowAt(screen_location));
  RecordEndScenarioType(GetEndScenarioType(back_gesture_start_scenario_type_,
                                           BackGestureEndType::kBack));
}

bool BackGestureEventHandler::ShouldWaitForTouchPressAck(
    const gfx::Point& screen_location) {
  aura::Window* top_window = window_util::GetTopWindow();
  if (!top_window || !CanStartGoingBack(screen_location))
    return false;

  return !top_window->GetProperty(chromeos::kIsShowingInOverviewKey) &&
         Shell::Get()->shell_delegate()->ShouldWaitForTouchPressAck(top_window);
}

}  // namespace ash
