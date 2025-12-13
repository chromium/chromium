// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/root_window_controller.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/home_button_tap_overlay.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr base::TimeDelta kAssistantAnimationDelay = base::Milliseconds(200);

}  // namespace

HomeButtonController::HomeButtonController(HomeButton* button)
    : button_(button) {
  DCHECK(button_);

  InitializeAssistantOverlay();
  DCHECK(tap_overlay_);

  Shell* shell = Shell::Get();
  sunfish_scanner_feature_observation_.Observe(
      shell->sunfish_scanner_feature_watcher());
  shell->app_list_controller()->AddObserver(this);
}

HomeButtonController::~HomeButtonController() {
  Shell* shell = Shell::Get();

  // AppListController are destroyed early when Shel is being destroyed, so they
  // may not exist.
  if (shell->app_list_controller())
    shell->app_list_controller()->RemoveObserver(this);
}

bool HomeButtonController::MaybeHandleGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapCancel:
      // Unconditionally stop the animation, even if Sunfish/Scanner
      // is not currently available - because the animation could have been
      // started when they _were_ available.
      // These are no-ops if the animation did not start.
      tap_overlay_->EndAnimation();
      tap_animation_delay_timer_->Stop();

      // After animating the ripple, let the button handle the event.
      return false;
    case ui::EventType::kGestureTapDown:
      if (IsSunfishOrScannerAvailable()) {
        tap_animation_delay_timer_->Start(
            FROM_HERE, kAssistantAnimationDelay,
            base::BindOnce(&HomeButtonController::StartAssistantAnimation,
                           base::Unretained(this)));
      }
      return false;
    case ui::EventType::kGestureLongPress:
      // Only consume the long press event if Assistant / Sunfish/Scanner is
      // available.
      //
      // Update Sunfish/Scanner feature state in case it is stale.
      sunfish_scanner_feature_observation_.GetSource()->UpdateFeatureStates();
      if (IsSunfishOrScannerAvailable()) {
        tap_overlay_->BurstAnimation();
        event->SetHandled();
        RecordScannerFeatureUserState(
            ScannerFeatureUserState::
                kSunfishSessionStartedFromHomeButtonLongPress);
        CaptureModeController::Get()->StartSunfishSession();
        return true;
      }

      return false;
    case ui::EventType::kGestureLongTap:
      // Only consume the long tap event if Sunfish/Scanner is available.
      if (!IsSunfishOrScannerAvailable()) {
        return false;
      }

      // We already handled the long press; consume the long tap to avoid
      // bringing up the context menu again.
      event->SetHandled();
      return true;
    default:
      return false;
  }
}

bool HomeButtonController::IsLongPressActionAvailable() {
  return IsSunfishOrScannerAvailable();
}

bool HomeButtonController::IsSunfishOrScannerAvailable() const {
  return sunfish_scanner_feature_observation_.GetSource()
      ->CanShowSunfishOrScannerUi();
}

void HomeButtonController::OnAppListVisibilityWillChange(bool shown,
                                                         int64_t display_id) {
  if (button_->GetDisplayId() != display_id)
    return;
  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void HomeButtonController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (state != display::TabletState::kInTabletMode) {
    return;
  }
}

void HomeButtonController::OnSunfishScannerFeatureStatesChanged(
    SunfishScannerFeatureWatcher& source) {
  button_->OnIconUpdated();
}

void HomeButtonController::StartAssistantAnimation() {
  tap_overlay_->StartAnimation();
}

void HomeButtonController::OnAppListShown() {
  // Do not show the button as toggled in tablet mode, since the home screen
  // view is always open in the background.
  if (!display::Screen::Get()->InTabletMode()) {
    button_->SetToggled(true);
  }
  button_->UpdateTooltipText();
}

void HomeButtonController::OnAppListDismissed() {
  button_->SetToggled(false);
  button_->UpdateTooltipText();
}

void HomeButtonController::InitializeAssistantOverlay() {
  DCHECK_EQ(nullptr, tap_overlay_);
  tap_overlay_ = new HomeButtonTapOverlay(button_);
  button_->AddChildViewRaw(tap_overlay_.get());
  tap_overlay_->SetVisible(false);
  tap_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
