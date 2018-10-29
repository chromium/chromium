// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

constexpr base::TimeDelta OverviewButtonTray::kDoubleTapThresholdMs;

OverviewButtonTray::OverviewButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(new views::ImageView()),
      scoped_session_observer_(this) {
  SetInkDropMode(InkDropMode::ON);

  gfx::ImageSkia image =
      gfx::CreateVectorIcon(kShelfOverviewIcon, kShelfIconColor);
  icon_->SetImage(image);
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  tray_container()->AddChildView(icon_);

  // Since OverviewButtonTray is located on the rightmost position of a
  // horizontal shelf, no separator is required.
  set_separator_visibility(false);

  Shell::Get()->AddShellObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

OverviewButtonTray::~OverviewButtonTray() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void OverviewButtonTray::UpdateAfterLoginStatusChange(LoginStatus status) {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnGestureEvent(ui::GestureEvent* event) {
  Button::OnGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    Shell::Get()->window_selector_controller()->OnOverviewButtonTrayLongPressed(
        event->location());
  }
}

bool OverviewButtonTray::PerformAction(const ui::Event& event) {
  DCHECK(event.type() == ui::ET_MOUSE_RELEASED ||
         event.type() == ui::ET_GESTURE_TAP);

  if (last_press_event_time_ &&
      event.time_stamp() - last_press_event_time_.value() <
          kDoubleTapThresholdMs) {
    // Second taps should not be processed outside of overview mode. (First
    // taps should be outside of overview).
    DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());

    base::RecordAction(base::UserMetricsAction("Tablet_QuickSwitch"));

    // Build mru window list. Use cycle as it excludes some windows we are not
    // interested in such as transient children.
    MruWindowTracker::WindowList mru_window_list =
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();

    // Switch to the second most recently used window (most recent is the
    // current window) if it exists, unless splitview mode is active.
    if (mru_window_list.size() > 1u) {
      aura::Window* new_active_window = mru_window_list[1];

      // In splitview mode, quick switch will only affect the windows on the non
      // default side. The window which was dragged to either side to begin
      // splitview will remain untouched. Skip that window if it appears in the
      // mru list.
      SplitViewController* split_view_controller =
          Shell::Get()->split_view_controller();
      if (split_view_controller->IsSplitViewModeActive() &&
          mru_window_list.size() > 2u) {
        if (mru_window_list[0] ==
                split_view_controller->GetDefaultSnappedWindow() ||
            mru_window_list[1] ==
                split_view_controller->GetDefaultSnappedWindow()) {
          new_active_window = mru_window_list[2];
        }
      }

      AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
      wm::GetWindowState(new_active_window)->Activate();
      last_press_event_time_ = base::nullopt;
      return true;
    }
  }

  // If not in overview mode record the time of this tap. A subsequent tap will
  // be checked against this to see if we should quick switch.
  last_press_event_time_ =
      Shell::Get()->window_selector_controller()->IsSelecting()
          ? base::nullopt
          : base::make_optional(event.time_stamp());

  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  // Note: Toggling overview mode will fail if there is no window to show, the
  // screen is locked, a modal dialog is open or is running in kiosk app
  // session.
  bool performed = controller->ToggleOverview();
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_OVERVIEW);
  return performed;
}

void OverviewButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnTabletModeEventsBlockingChanged() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnOverviewModeStarting() {
  SetIsActive(true);
}

void OverviewButtonTray::OnOverviewModeEnded() {
  SetIsActive(false);
}

void OverviewButtonTray::ClickedOutsideBubble() {}

base::string16 OverviewButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_BUTTON_ACCESSIBLE_NAME);
}

void OverviewButtonTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void OverviewButtonTray::UpdateIconVisibility() {
  // The visibility of the OverviewButtonTray has diverged from
  // WindowSelectorController::CanSelect. The visibility of the button should
  // not change during transient times in which CanSelect is false. Such as when
  // a modal dialog is present.
  SessionController* session_controller = Shell::Get()->session_controller();
  SetVisible(Shell::Get()
                 ->tablet_mode_controller()
                 ->AreInternalInputDeviceEventsBlocked() &&
             session_controller->GetSessionState() ==
                 session_manager::SessionState::ACTIVE &&
             !session_controller->IsRunningInAppMode());
}

}  // namespace ash
