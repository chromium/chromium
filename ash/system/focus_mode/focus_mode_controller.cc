// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/status_area_widget.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

FocusModeController* g_instance = nullptr;

// The default Focus Mode session duration.
constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);

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

  if (in_focus_session_) {
    timer_.Stop();

    SetFocusTrayVisibility(false);

    // Restore previous DND state.
    message_center->SetQuietMode(previous_do_not_disturb_state_);
  } else {
    SaveSettingsToUserPrefs();

    // Update DND to the user selected state, and keep track of the state we
    // want to revert back to after the Focus Mode session ends.
    previous_do_not_disturb_state_ = message_center->IsQuietMode();
    message_center->SetQuietMode(turn_on_do_not_disturb_);

    // Start timer for the specified `session_duration_`.
    end_time_ = base::Time::Now() + session_duration_;
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &FocusModeController::OnTimerTick, base::TimeTicks::Now());

    SetFocusTrayVisibility(true);
  }

  in_focus_session_ = !in_focus_session_;

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
