// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

constexpr base::TimeDelta OverviewButtonTray::kDoubleTapThresholdMs;

OverviewButtonTray::OverviewButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(new views::ImageView()),
      scoped_session_observer_(this) {
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kShelfOverviewIcon, ShelfConfig::Get()->shelf_icon_color());
  icon_->SetImage(image);
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  tray_container()->AddChildView(icon_);

  // Since OverviewButtonTray is located on the rightmost position of a
  // horizontal shelf, no separator is required.
  set_separator_visibility(false);

  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

OverviewButtonTray::~OverviewButtonTray() {
  if (Shell::Get()->overview_controller())
    Shell::Get()->overview_controller()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void OverviewButtonTray::UpdateAfterLoginStatusChange(LoginStatus status) {
  UpdateIconVisibility();
}

void OverviewButtonTray::SnapRippleToActivated() {
  GetInkDrop()->SnapToActivated();
}

void OverviewButtonTray::OnGestureEvent(ui::GestureEvent* event) {
  Button::OnGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    Shell::Get()->overview_controller()->OnOverviewButtonTrayLongPressed(
        event->location());
  }
}

bool OverviewButtonTray::PerformAction(const ui::Event& event) {
  DCHECK(event.type() == ui::ET_MOUSE_RELEASED ||
         event.type() == ui::ET_GESTURE_TAP);

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  // Skip if the second tap happened outside of overview. This can happen if a
  // window gets activated in between, which cancels overview mode.
  if (overview_controller->InOverviewSession() && last_press_event_time_ &&
      event.time_stamp() - last_press_event_time_.value() <
          kDoubleTapThresholdMs) {
    base::RecordAction(base::UserMetricsAction("Tablet_QuickSwitch"));

    // Build mru window list. Use cycle as it excludes some windows we are not
    // interested in such as transient children. Limit only to windows in the
    // current active desk for now. TODO(afakhry): Revisit with UX.
    MruWindowTracker::WindowList mru_window_list =
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
            kActiveDesk);

    // Switch to the second most recently used window (most recent is the
    // current window) if it exists, unless splitview mode is active. Do not
    // switch if we entered overview mode with all windows minimized.
    const OverviewSession::EnterExitOverviewType enter_exit_type =
        overview_controller->overview_session()->enter_exit_overview_type();
    if (mru_window_list.size() > 1u &&
        enter_exit_type !=
            OverviewSession::EnterExitOverviewType::kFadeInEnter &&
        enter_exit_type !=
            OverviewSession::EnterExitOverviewType::kSlideInEnter) {
      aura::Window* new_active_window = mru_window_list[1];

      // In tablet split view mode, quick switch will only affect the windows on
      // the non default side. The window which was dragged to either side to
      // begin split view will remain untouched. Skip that window if it appears
      // in the mru list. Clamshell split view mode is impossible here, as it
      // implies overview mode, and you cannot quick switch in overview mode.
      SplitViewController* split_view_controller =
          SplitViewController::Get(Shell::GetPrimaryRootWindow());
      if (split_view_controller->InTabletSplitViewMode() &&
          mru_window_list.size() > 2u) {
        if (mru_window_list[0] ==
                split_view_controller->GetDefaultSnappedWindow() ||
            mru_window_list[1] ==
                split_view_controller->GetDefaultSnappedWindow()) {
          new_active_window = mru_window_list[2];
        }
      }

      AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
      wm::ActivateWindow(new_active_window);
      last_press_event_time_ = base::nullopt;
      return true;
    }
  }

  // If not in overview mode record the time of this tap. A subsequent tap will
  // be checked against this to see if we should quick switch.
  last_press_event_time_ = overview_controller->InOverviewSession()
                               ? base::nullopt
                               : base::make_optional(event.time_stamp());

  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview();
  else
    overview_controller->StartOverview();
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_OVERVIEW);

  // The return value doesn't matter here. OnOverviewModeStarting() and
  // OnOverviewModeEnded() will do the right thing to set the button state.
  return true;
}

void OverviewButtonTray::HandlePerformActionResult(bool action_performed,
                                                   const ui::Event& event) {}

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

const char* OverviewButtonTray::GetClassName() const {
  return "OverviewButtonTray";
}

void OverviewButtonTray::UpdateIconVisibility() {
  // The visibility of the OverviewButtonTray has diverged from
  // OverviewController::CanSelect. The visibility of the button should
  // not change during transient times in which CanSelect is false. Such as when
  // a modal dialog is present.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  bool active_session = session_controller->GetSessionState() ==
                        session_manager::SessionState::ACTIVE;
  bool app_mode = session_controller->IsRunningInAppMode();

  bool should_show =
      Shell::Get()->tablet_mode_controller()->ShouldShowOverviewButton();

  SetVisiblePreferred(should_show && active_session && !app_mode);
}

}  // namespace ash
