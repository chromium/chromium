// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

constexpr char kOverviewGestureNotificationId[] =
    "ash.wm.reverse_overview_gesture";
constexpr int kReverseGestureNotificationShowLimit = 3;

constexpr char kSwitchNextDeskToastId[] = "ash.wm.reverse_next_desk_toast";
constexpr char kSwitchLastDeskToastId[] = "ash.wm.reverse_last_desk_toast";

constexpr base::TimeDelta kToastDurationMs =
    base::TimeDelta::FromMilliseconds(2500);

// Check if the user used the wrong gestures.
bool gDidWrongNextDeskGesture = false;
bool gDidWrongLastDeskGesture = false;

// Is the reverse scrolling for touchpad on.
bool IsNaturalScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kTouchpadEnabled) &&
         pref->GetBoolean(prefs::kNaturalScroll);
}

// Is reverse scrolling for mouse wheel on.
bool IsReverseScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kMouseReverseScroll);
}

// Reverse an offset when the reverse scrolling is on.
float GetOffset(float offset) {
  // The handler code uses the new directions which is the reverse of the old
  // handler code. Reverse the offset if the ReverseScrollGestures feature is
  // disabled so that the users get old behavior.
  if (!features::IsReverseScrollGesturesEnabled())
    return -offset;
  return IsNaturalScrollOn() ? -offset : offset;
}

void ShowOverviewGestureNotification() {
  int reverse_gesture_notification_count =
      Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
          prefs::kReverseGestureNotificationCount);

  if (reverse_gesture_notification_count >=
      kReverseGestureNotificationShowLimit) {
    return;
  }

  base::string16 title = l10n_util::GetStringUTF16(
      IDS_OVERVIEW_REVERSE_GESTURE_NOTIFICATION_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_OVERVIEW_REVERSE_GESTURE_NOTIFICATION_MESSAGE);

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kOverviewGestureNotificationId, title, message, base::string16(),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kOverviewGestureNotificationId),
          message_center::RichNotificationData(), nullptr, gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  // Make the notification popup again if it has been in message center.
  if (message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kOverviewGestureNotificationId)) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kOverviewGestureNotificationId, false);
  }
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kReverseGestureNotificationCount,
      reverse_gesture_notification_count + 1);
}

void ShowReverseGestureToast(const char* toast_id, int message_id) {
  Shell::Get()->toast_manager()->Show(
      ToastData(toast_id, l10n_util::GetStringUTF16(message_id),
                kToastDurationMs.InMilliseconds(), base::nullopt));
}

// The amount the fingers must move in a direction before a continuous gesture
// animation is started. This is to minimize accidental scrolls.
constexpr int kContinuousGestureMoveThresholdDp = 10;

// Handles vertical 3-finger scroll gesture by entering overview on scrolling
// up, and exiting it on scrolling down. If entering overview and window cycle
// list is open, close the window cycle list.
// Returns true if the gesture was handled.
bool Handle3FingerVerticalScroll(float scroll_y) {
  if (std::fabs(scroll_y) < WmGestureHandler::kVerticalThresholdDp)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (in_overview) {
    // If touchpad reverse scroll is on, only swip down can exit overview. If
    // touchpad reverse scroll is off, in M87 swip up can also exit overview but
    // show notification; in M88, swip up will only show notification; in M89
    // the notification is removed.
    if (GetOffset(scroll_y) > 0) {
      if (!features::IsReverseScrollGesturesEnabled() || IsNaturalScrollOn())
        return false;

      ShowOverviewGestureNotification();
    }

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    if (overview_controller->AcceptSelection())
      return true;
    overview_controller->EndOverview();
  } else {
    // If touchpad reverse scroll is on, only swip up can enter overview. If
    // touchpad reverse scroll is off, in M87 swip down can also enter overview
    // but show notification; in M88, swip down will only show notification; in
    // M89 the notification is removed.
    if (GetOffset(scroll_y) < 0) {
      if (!features::IsReverseScrollGesturesEnabled() || IsNaturalScrollOn())
        return false;

      ShowOverviewGestureNotification();
    }

    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    if (window_cycle_controller->IsCycling())
      window_cycle_controller->CancelCycling();

    base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
    overview_controller->StartOverview();
  }

  return true;
}

// Handles horizontal 4-finger scroll by switching desks if possible.
// Returns true if the gesture was handled.
bool HandleDesksSwitchHorizontalScroll(float scroll_x) {
  if (std::fabs(scroll_x) < WmGestureHandler::kHorizontalThresholdDp)
    return false;

  if (features::IsReverseScrollGesturesEnabled() && !IsNaturalScrollOn()) {
    if (GetOffset(scroll_x) > 0 && !DesksController::Get()->GetNextDesk() &&
        DesksController::Get()->GetPreviousDesk()) {
      if (!gDidWrongLastDeskGesture) {
        gDidWrongLastDeskGesture = true;
      } else {
        ShowReverseGestureToast(kSwitchLastDeskToastId,
                                IDS_CHANGE_LAST_DESK_REVERSE_GESTURE);
      }
    } else if (GetOffset(scroll_x) < 0 &&
               !DesksController::Get()->GetPreviousDesk() &&
               DesksController::Get()->GetNextDesk()) {
      if (!gDidWrongNextDeskGesture) {
        gDidWrongNextDeskGesture = true;
      } else {
        ShowReverseGestureToast(kSwitchNextDeskToastId,
                                IDS_CHANGE_NEXT_DESK_REVERSE_GESTURE);
      }
    } else {
      gDidWrongNextDeskGesture = false;
      gDidWrongLastDeskGesture = false;
      Shell::Get()->toast_manager()->Cancel(kSwitchNextDeskToastId);
      Shell::Get()->toast_manager()->Cancel(kSwitchLastDeskToastId);
    }
  }

  // If touchpad reverse scroll is on, the swipe direction will invert.
  return DesksController::Get()->ActivateAdjacentDesk(
      /*going_left=*/GetOffset(scroll_x) < 0,
      DesksSwitchSource::kDeskSwitchTouchpad);
}

}  // namespace

WmGestureHandler::WmGestureHandler()
    : is_enhanced_desk_animations_(features::IsEnhancedDeskAnimations()) {}

WmGestureHandler::~WmGestureHandler() = default;

bool WmGestureHandler::ProcessWheelEvent(const ui::MouseEvent& event) {
  if (event.IsMouseWheelEvent() &&
      Shell::Get()->window_cycle_controller()->IsCycling()) {
    if (!scroll_data_)
      scroll_data_ = ScrollData();

    // Convert mouse wheel events into three-finger scrolls for window cycle
    // list and also swap y offset with x offset.
    return ProcessEventImpl(
        /*finger_count=*/3,
        IsReverseScrollOn() ? event.AsMouseWheelEvent()->y_offset()
                            : -event.AsMouseWheelEvent()->y_offset(),
        event.AsMouseWheelEvent()->x_offset());
  }

  return false;
}

bool WmGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  // ET_SCROLL_FLING_CANCEL means a touchpad swipe has started.
  if (event.type() == ui::ET_SCROLL_FLING_CANCEL) {
    scroll_data_ = ScrollData();
    return false;
  }

  // ET_SCROLL_FLING_START means a touchpad swipe has ended.
  if (event.type() == ui::ET_SCROLL_FLING_START) {
    bool success = EndScroll();
    DCHECK(!scroll_data_);
    return success;
  }

  DCHECK_EQ(ui::ET_SCROLL, event.type());

  return ProcessEventImpl(event.finger_count(), event.x_offset(),
                          event.y_offset());
}

bool WmGestureHandler::ProcessEventImpl(int finger_count,
                                        float delta_x,
                                        float delta_y) {
  if (!scroll_data_)
    return false;

  // Only two, three or four finger scrolls are supported.
  if (finger_count != 2 && finger_count != 3 && finger_count != 4) {
    scroll_data_.reset();
    return false;
  }

  // There is a finger switch, end the current gesture.
  if (scroll_data_->finger_count != 0 &&
      scroll_data_->finger_count != finger_count) {
    scroll_data_.reset();
    return false;
  }

  if (finger_count == 2 && !IsNaturalScrollOn()) {
    // Two finger swipe from left to right should move the list right regardless
    // of natural scroll settings.
    delta_x = -delta_x;
  }

  scroll_data_->scroll_x += delta_x;
  scroll_data_->scroll_y += delta_y;

  // If the requirements to cycle the window cycle list or  move the overview
  // selector are met, reset |scroll_data_|. If both are open, cycle the window
  // cycle list.
  const bool moved = CycleWindowCycleList(finger_count, scroll_data_->scroll_x,
                                          scroll_data_->scroll_y) ||
                     MoveOverviewSelection(finger_count, scroll_data_->scroll_x,
                                           scroll_data_->scroll_y);

  if (is_enhanced_desk_animations_ && finger_count == 4) {
    DCHECK(!moved);
    // Update the continuous desk animation if it has already been started,
    // otherwise start it if it passes the threshold.
    if (scroll_data_->continuous_gesture_started) {
      DesksController::Get()->UpdateSwipeAnimation(delta_x);
    } else if (std::abs(scroll_data_->scroll_x) >
               kContinuousGestureMoveThresholdDp) {
      if (!DesksController::Get()->StartSwipeAnimation(
              /*move_left=*/delta_x > 0)) {
        // Starting an animation failed. This can happen if we are on the
        // lockscreen or an ongoing animation from a different source is
        // happening. In this case reset |scroll_data_| and wait for the next 4
        // finger swipe.
        scroll_data_.reset();
        return false;
      }
      scroll_data_->continuous_gesture_started = true;
    }
  }

  if (moved)
    scroll_data_ = ScrollData();
  scroll_data_->finger_count = finger_count;
  return moved;
}

// static
void WmGestureHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kReverseGestureNotificationCount, 0);
}

bool WmGestureHandler::EndScroll() {
  if (!scroll_data_)
    return false;

  const int finger_count = scroll_data_->finger_count;
  const float scroll_x = scroll_data_->scroll_x;
  const float scroll_y = scroll_data_->scroll_y;
  const bool continuous_gesture_started =
      scroll_data_->continuous_gesture_started;
  scroll_data_.reset();

  if (finger_count == 0)
    return false;

  if (finger_count == 3) {
    if (std::fabs(scroll_x) < std::fabs(scroll_y))
      return Handle3FingerVerticalScroll(scroll_y);

    return MoveOverviewSelection(finger_count, scroll_x, scroll_y);
  }

  if (finger_count != 4)
    return false;

  if (!is_enhanced_desk_animations_)
    return HandleDesksSwitchHorizontalScroll(scroll_x);

  if (continuous_gesture_started)
    DesksController::Get()->EndSwipeAnimation();

  return continuous_gesture_started;
}

bool WmGestureHandler::MoveOverviewSelection(int finger_count,
                                             float scroll_x,
                                             float scroll_y) {
  if (finger_count != 3)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!ShouldHorizontallyScroll(in_overview, scroll_x, scroll_y))
    return false;

  overview_controller->IncrementSelection(/*forward=*/scroll_x > 0);
  return true;
}

bool WmGestureHandler::CycleWindowCycleList(int finger_count,
                                            float scroll_x,
                                            float scroll_y) {
  if (!features::IsInteractiveWindowCycleListEnabled() ||
      (finger_count != 2 && finger_count != 3)) {
    return false;
  }

  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
  const bool is_cycling = window_cycle_controller->IsCycling();
  if (!ShouldHorizontallyScroll(is_cycling, scroll_x, scroll_y))
    return false;

  window_cycle_controller->HandleCycleWindow(
      scroll_x > 0 ? WindowCycleController::FORWARD
                   : WindowCycleController::BACKWARD);
  return true;
}

bool WmGestureHandler::ShouldHorizontallyScroll(bool in_session,
                                                float scroll_x,
                                                float scroll_y) {
  // Dominantly vertical scrolls and small horizontal scrolls do not move the
  // selector.
  if (!in_session || std::fabs(scroll_x) < std::fabs(scroll_y))
    return false;

  if (std::fabs(scroll_x) < kHorizontalThresholdDp)
    return false;

  return true;
}

}  // namespace ash
