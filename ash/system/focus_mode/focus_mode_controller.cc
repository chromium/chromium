// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/status_area_widget.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

FocusModeController* g_instance = nullptr;

// The default Focus Mode session duration.
constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);

// The amount of time to extend the focus session by when the focus session
// duration is extended during a currently active focus session.
constexpr base::TimeDelta kExtendDuration = base::Minutes(10);

bool IsQuietModeOnSetByFocusMode() {
  auto* message_center = message_center::MessageCenter::Get();
  return message_center->IsQuietMode() &&
         message_center->GetLastQuietModeChangeSourceType() ==
             message_center::QuietModeSourceType::kFocusMode;
}

}  // namespace

FocusModeController::FocusModeController()
    : session_duration_(kDefaultSessionDuration) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  Shell::Get()->session_controller()->AddObserver(this);
}

FocusModeController::~FocusModeController() {
  Shell::Get()->session_controller()->RemoveObserver(this);

  if (in_focus_session_) {
    ToggleFocusMode();
  }

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FocusModeController* FocusModeController::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
void FocusModeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(prefs::kFocusModeSessionDuration,
                                  /*default_value=*/kDefaultSessionDuration);
  registry->RegisterBooleanPref(prefs::kFocusModeDoNotDisturb,
                                /*default_value=*/true);
}

void FocusModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeController::ToggleFocusMode() {
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);

  in_focus_session_ = !in_focus_session_;
  if (in_focus_session_) {
    SaveSettingsToUserPrefs();

    // Start timer for the specified `session_duration_`. Set `end_time_` before
    // `SetQuietMode` called, because we may indirectly use `end_time_` to
    // create a notification.
    end_time_ = base::Time::Now() + session_duration_;
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &FocusModeController::OnTimerTick, base::TimeTicks::Now());

    // Only for the case DND is not enabled before starting a session and
    // `turn_on_do_not_disturb_` is true, we set `QuietModeSourceType` with
    // `kFocusMode` type.
    if (!message_center->IsQuietMode() && turn_on_do_not_disturb_) {
      message_center->SetQuietMode(
          true, message_center::QuietModeSourceType::kFocusMode);
    }

    SetFocusTrayVisibility(true);
  } else {
    timer_.Stop();

    SetFocusTrayVisibility(false);

    if (IsQuietModeOnSetByFocusMode()) {
      message_center->SetQuietMode(
          false, message_center::QuietModeSourceType::kFocusMode);
    }

    // Reset the `session_duration_` as it may have been changed during the
    // focus session.
    session_duration_ = Shell::Get()
                            ->session_controller()
                            ->GetActivePrefService()
                            ->GetTimeDelta(prefs::kFocusModeSessionDuration);
  }

  for (auto& observer : observers_) {
    observer.OnFocusModeChanged(in_focus_session_);
  }
}

void FocusModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  if (in_focus_session_) {
    ToggleFocusMode();
  }

  UpdateFromUserPrefs();
}

void FocusModeController::ExtendActiveSessionDuration() {
  CHECK(in_focus_session_);
  SetSessionDuration(session_duration_ + kExtendDuration);

  // Update all observers that may be using `end_time_` or `session_duration_`,
  // the countdown view UI timers for example, so they don't have to wait for
  // the next timer tick to update the UI.
  OnTimerTick();

  // Only update the notification if DND was turned on by the focus mode.
  if (!IsQuietModeOnSetByFocusMode()) {
    return;
  }

  if (auto* notification_controller =
          DoNotDisturbNotificationController::Get()) {
    notification_controller->MaybeUpdateNotification();
  }
}

void FocusModeController::SetSessionDuration(
    const base::TimeDelta& new_session_duration) {
  const base::TimeDelta valid_new_session_duration =
      std::clamp(new_session_duration, focus_mode_util::kMinimumDuration,
                 focus_mode_util::kMaximumDuration);
  if (session_duration_ == valid_new_session_duration) {
    return;
  }

  // Update `end_time_` only during an active focus session.
  if (in_focus_session_) {
    end_time_ += (valid_new_session_duration - session_duration_);
  }

  // We do not immediately commit the change directly to the user prefs because
  // the user has not yet indicated their preferred timer duration by starting
  // the timer.
  session_duration_ = valid_new_session_duration;
}

void FocusModeController::OnTimerTick() {
  if (in_focus_session_ && base::Time::Now() >= end_time_) {
    ToggleFocusMode();
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTimerTick();
  }
}

void FocusModeController::UpdateFromUserPrefs() {
  PrefService* primary_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  session_duration_ =
      primary_user_prefs->GetTimeDelta(prefs::kFocusModeSessionDuration);
  turn_on_do_not_disturb_ =
      primary_user_prefs->GetBoolean(prefs::kFocusModeDoNotDisturb);

  if (session_duration_ <= base::TimeDelta()) {
    session_duration_ = kDefaultSessionDuration;
  }
}

void FocusModeController::SaveSettingsToUserPrefs() {
  if (PrefService* primary_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    primary_user_prefs->SetTimeDelta(prefs::kFocusModeSessionDuration,
                                     session_duration_);
    primary_user_prefs->SetBoolean(prefs::kFocusModeDoNotDisturb,
                                   turn_on_do_not_disturb_);
  }
}

void FocusModeController::SetFocusTrayVisibility(bool visible) {
  for (auto* root_window : Shell::GetAllRootWindows()) {
    if (auto* status_area_widget = RootWindowController::ForWindow(root_window)
                                       ->GetStatusAreaWidget()) {
      status_area_widget->focus_mode_tray()->SetVisiblePreferred(visible);
    }
  }
}

}  // namespace ash
