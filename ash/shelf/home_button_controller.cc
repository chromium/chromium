// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/assistant_overlay.h"
#include "ash/shelf/home_button.h"
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
  DCHECK(assistant_overlay_);

  Shell* shell = Shell::Get();
  shell->app_list_controller()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
  AssistantState::Get()->AddObserver(this);
}

HomeButtonController::~HomeButtonController() {
  Shell* shell = Shell::Get();

  // AppListController are destroyed early when Shel is being destroyed, so they
  // may not exist.
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  if (shell->app_list_controller())
    shell->app_list_controller()->RemoveObserver(this);
  if (AssistantState::Get())
    AssistantState::Get()->RemoveObserver(this);
}

bool HomeButtonController::MaybeHandleGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapCancel:
      if (IsAssistantAvailable()) {
        assistant_overlay_->EndAnimation();
        assistant_animation_delay_timer_->Stop();
      }

      // After animating the ripple, let the button handle the event.
      return false;
    case ui::EventType::kGestureTapDown:
      if (IsAssistantAvailable()) {
        assistant_animation_delay_timer_->Start(
            FROM_HERE, kAssistantAnimationDelay,
            base::BindOnce(&HomeButtonController::StartAssistantAnimation,
                           base::Unretained(this)));
      }
      return false;
    case ui::EventType::kGestureLongPress:
      // Only consume the long press event if the Assistant is available.
      if (!IsAssistantAvailable())
        return false;

      base::RecordAction(base::UserMetricsAction(
          "VoiceInteraction.Started.HomeButtonLongPress"));
      assistant_overlay_->BurstAnimation();
      event->SetHandled();
      Shell::SetRootWindowForNewWindows(
          button_->GetWidget()->GetNativeWindow()->GetRootWindow());
      AssistantUiController::Get()->ShowUi(
          AssistantEntryPoint::kLongPressLauncher);
      return true;
    case ui::EventType::kGestureLongTap:
      // Only consume the long tap event if the Assistant is available.
      if (!IsAssistantAvailable())
        return false;

      // We already handled the long press; consume the long tap to avoid
      // bringing up the context menu again.
      event->SetHandled();
      return true;
    default:
      return false;
  }
}

bool HomeButtonController::IsAssistantAvailable() {
  AssistantStateBase* state = AssistantState::Get();
  return state->allowed_state() == assistant::AssistantAllowedState::ALLOWED &&
         state->settings_enabled().value_or(false);
}

bool HomeButtonController::IsAssistantVisible() {
  return AssistantUiController::Get()->GetModel()->visibility() ==
         AssistantVisibility::kVisible;
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

void HomeButtonController::OnAssistantFeatureAllowedChanged(
    assistant::AssistantAllowedState state) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::OnAssistantSettingsEnabled(bool enabled) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::StartAssistantAnimation() {
  assistant_overlay_->StartAnimation(false);
}

void HomeButtonController::OnAppListShown() {
  // Do not show the button as toggled in tablet mode, since the home screen
  // view is always open in the background.
  if (!Shell::Get()->IsInTabletMode()) {
    button_->SetToggled(true);
  }
}

void HomeButtonController::OnAppListDismissed() {
  button_->SetToggled(false);
}

void HomeButtonController::InitializeAssistantOverlay() {
  DCHECK_EQ(nullptr, assistant_overlay_);
  assistant_overlay_ = new AssistantOverlay(button_);
  button_->AddChildView(assistant_overlay_.get());
  assistant_overlay_->SetVisible(false);
  assistant_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
