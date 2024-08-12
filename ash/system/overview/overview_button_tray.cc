// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/constants/ash_switches.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

bool ShouldButtonBeVisible() {
  auto* shell = Shell::Get();
  SessionControllerImpl* session_controller = shell->session_controller();
  if (session_controller->GetSessionState() !=
          session_manager::SessionState::ACTIVE ||
      session_controller->IsRunningInAppMode()) {
    return false;
  }

  if (switches::IsOverviewButtonEnabledForTests()) {
    return true;
  }

  return shell->tablet_mode_controller()->ShouldShowOverviewButton() &&
         ShelfConfig::Get()->shelf_controls_shown();
}

}  // namespace

constexpr base::TimeDelta OverviewButtonTray::kDoubleTapThresholdMs;

OverviewButtonTray::OverviewButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kOverview),
      icon_(new views::ImageView()),
      scoped_session_observer_(this) {
  SetCallback(base::BindRepeating(&OverviewButtonTray::OnButtonPressed,
                                  base::Unretained(this)));

  const gfx::ImageSkia image = GetIconImage();
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  tray_container()->AddChildView(icon_.get());

  // Since OverviewButtonTray is located on the rightmost position of a
  // horizontal shelf, no separator is required.
  set_separator_visibility(false);

  set_use_bounce_in_animation(false);

  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->shelf_config()->AddObserver(this);
}

OverviewButtonTray::~OverviewButtonTray() {
  if (Shell::Get()->overview_controller())
    Shell::Get()->overview_controller()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->shelf_config()->RemoveObserver(this);
}

void OverviewButtonTray::UpdateAfterLoginStatusChange() {
  UpdateIconVisibility();
}

void OverviewButtonTray::SnapRippleToActivated() {
  views::InkDrop::Get(this)->GetInkDrop()->SnapToActivated();
}

void OverviewButtonTray::OnGestureEvent(ui::GestureEvent* event) {
  Button::OnGestureEvent(event);
  // TODO(crbug.com/40242435): React to long press via `OnButtonPressed()` once
  // this is enabled.
  if (event->type() == ui::EventType::kGestureLongPress) {
    // TODO(crbug.com/40630467): Properly implement the multi-display behavior
    // (in tablet position with an external pointing device).
    SplitViewController::Get(Shell::GetPrimaryRootWindow())
        ->OnOverviewButtonTrayLongPressed(event->location());
  }
}

void OverviewButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnTabletModeEventsBlockingChanged() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnShelfConfigUpdated() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnOverviewModeStarting() {
  SetIsActive(true);
}

void OverviewButtonTray::OnOverviewModeEnded() {
  SetIsActive(false);
}

void OverviewButtonTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {}

void OverviewButtonTray::UpdateTrayItemColor(bool is_active) {
  icon_->SetImage(GetIconImage());
}

std::u16string OverviewButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_BUTTON_ACCESSIBLE_NAME);
}

void OverviewButtonTray::HandleLocaleChange() {}

void OverviewButtonTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void OverviewButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  icon_->SetImage(GetIconImage());
}

void OverviewButtonTray::HideBubble(const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void OverviewButtonTray::OnButtonPressed(const ui::Event& event) {
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
    const OverviewEnterExitType enter_exit_type =
        overview_controller->overview_session()->enter_exit_overview_type();
    if (mru_window_list.size() > 1u &&
        enter_exit_type != OverviewEnterExitType::kFadeInEnter) {
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

      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::DEACTIVATED, nullptr);
      wm::ActivateWindow(new_active_window);
      last_press_event_time_ = std::nullopt;
      return;
    }
  }

  // If not in overview mode record the time of this tap. A subsequent tap will
  // be checked against this to see if we should quick switch.
  last_press_event_time_ = overview_controller->InOverviewSession()
                               ? std::nullopt
                               : std::make_optional(event.time_stamp());

  if (overview_controller->InOverviewSession())
    overview_controller->EndOverview(OverviewEndAction::kOverviewButton);
  else
    overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  base::RecordAction(base::UserMetricsAction("Tray_Overview"));
}

void OverviewButtonTray::UpdateIconVisibility() {
  SetVisiblePreferred(ShouldButtonBeVisible());
}

gfx::ImageSkia OverviewButtonTray::GetIconImage() {
  SkColor color;
  if (GetColorProvider()) {
    color = GetColorProvider()->GetColor(
        is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);
  } else {
    color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonIconColor);
  }
  return gfx::CreateVectorIcon(kShelfOverviewIcon, color);
}

BEGIN_METADATA(OverviewButtonTray)
END_METADATA

}  // namespace ash
