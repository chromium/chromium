// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

FocusModeController* g_instance = nullptr;

// The default Focus Mode session duration.
constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);

bool IsQuietModeOnSetByFocusMode() {
  auto* message_center = message_center::MessageCenter::Get();
  return message_center->IsQuietMode() &&
         message_center->GetLastQuietModeChangeSourceType() ==
             message_center::QuietModeSourceType::kFocusMode;
}

// Updates the notification if DND was turned on by the focus mode.
void MaybeUpdateDndNotification() {
  if (!IsQuietModeOnSetByFocusMode()) {
    return;
  }

  if (auto* notification_controller =
          DoNotDisturbNotificationController::Get()) {
    notification_controller->MaybeUpdateNotification();
  }
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
  ResetFocusSession();

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FocusModeController* FocusModeController::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool FocusModeController::CanExtendSessionDuration(
    const FocusModeSession::Snapshot& snapshot) {
  return snapshot.session_duration < focus_mode_util::kMaximumDuration;
}

// static
void FocusModeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(
      prefs::kFocusModeSessionDuration,
      /*default_value=*/kDefaultSessionDuration,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kFocusModeDoNotDisturb,
      /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void FocusModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeController::ToggleFocusMode(
    focus_mode_histogram_names::ToggleSource source) {
  if (in_focus_session()) {
    base::UmaHistogramEnumeration(
        /*name=*/focus_mode_histogram_names::
            kToggleEndButtonDuringSessionHistogramName,
        /*sample=*/source);
    ResetFocusSession();
    return;
  }
  StartFocusSession();
}

void FocusModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  ResetFocusSession();
  UpdateFromUserPrefs();
}

void FocusModeController::ExtendSessionDuration() {
  CHECK(current_session_);
  const base::Time now = base::Time::Now();
  // We call this with `now` to make sure that all the actions taken are synced
  // to the same time, since the state depends on `now`.
  current_session_->ExtendSession(now);

  const auto session_snapshot = current_session_->GetSnapshot(now);
  for (auto& observer : observers_) {
    observer.OnActiveSessionDurationChanged(session_snapshot);
  }

  if (!timer_.IsRunning()) {
    // Start the `session_duration_` timer again.
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &FocusModeController::OnTimerTick, base::TimeTicks::Now());

    for (auto& observer : observers_) {
      observer.OnFocusModeChanged(/*in_focus_session=*/true);
    }
  }

  MaybeUpdateDndNotification();
}

void FocusModeController::ResetFocusSession() {
  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  SetFocusTrayVisibility(false);

  if (IsQuietModeOnSetByFocusMode()) {
    message_center::MessageCenter::Get()->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  }

  const bool was_in_focus_session = in_focus_session();
  current_session_.reset();

  if (was_in_focus_session) {
    for (auto& observer : observers_) {
      observer.OnFocusModeChanged(/*in_focus_session=*/false);
    }
  }
}

void FocusModeController::EnablePersistentEnding() {
  // This is only used right now for when we click the tray icon to open the
  // bubble during the ending moment. This prevents the bubble from being closed
  // automatically.
  if (!in_ending_moment()) {
    return;
  }

  if (timer_.IsRunning()) {
    timer_.Stop();
  }
  // Update the session to stay in the ending moment state.
  current_session_->set_persistent_ending();
}

void FocusModeController::SetInactiveSessionDuration(
    const base::TimeDelta& new_session_duration) {
  CHECK(!in_focus_session());
  const base::TimeDelta valid_new_session_duration =
      std::clamp(new_session_duration, focus_mode_util::kMinimumDuration,
                 focus_mode_util::kMaximumDuration);

  if (session_duration_ == valid_new_session_duration) {
    return;
  }

  // We do not immediately commit the change directly to the user prefs because
  // the user has not yet indicated their preferred timer duration by starting
  // the timer.
  session_duration_ = valid_new_session_duration;

  for (auto& observer : observers_) {
    observer.OnInactiveSessionDurationChanged(session_duration_);
  }
}

bool FocusModeController::HasStartedSessionBefore() const {
  // Since `kFocusModeDoNotDisturb` is always set whenever a focus session is
  // started, we can use this as an indicator of if the user has ever started a
  // focus session before.
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    return active_user_prefs->HasPrefPath(prefs::kFocusModeDoNotDisturb);
  }
  return false;
}

FocusModeSession::Snapshot FocusModeController::GetSnapshot(
    const base::Time& now) const {
  return current_session_ ? current_session_->GetSnapshot(now)
                          : FocusModeSession::Snapshot{};
}

base::TimeDelta FocusModeController::GetSessionDuration() const {
  return in_focus_session() ? current_session_->session_duration()
                            : session_duration_;
}

base::Time FocusModeController::GetActualEndTime() const {
  if (!current_session_) {
    return base::Time();
  }

  return in_ending_moment() ? current_session_->end_time() +
                                  focus_mode_util::kEndingMomentDuration
                            : current_session_->end_time();
}

void FocusModeController::SetSelectedTask(const api::Task* task) {
  if (!task) {
    selected_task_id_.clear();
    selected_task_title_.clear();
  } else {
    selected_task_id_ = task->id;
    selected_task_title_ = task->title;
  }
  // TODO(b/305089077): Update user prefs.
}

bool FocusModeController::HasSelectedTask() const {
  return !selected_task_id_.empty();
}

void FocusModeController::CompleteTask() {
  tasks_provider_.MarkAsCompleted(selected_task_id_);
  SetSelectedTask(nullptr);
}

void FocusModeController::TriggerEndingMomentImmediately() {
  if (!in_focus_session()) {
    return;
  }
  current_session_->set_end_time(base::Time::Now());
  OnTimerTick();
}

void FocusModeController::StartFocusSession() {
  current_session_ = FocusModeSession(session_duration_,
                                      session_duration_ + base::Time::Now());

  SaveSettingsToUserPrefs();

  // Start timer for the specified `session_duration_`. Set `current_session_`
  // before `SetQuietMode` called, because we may indirectly call
  // `GetActualEndTime` to create a notification.
  timer_.Start(FROM_HERE, base::Seconds(1), this,
               &FocusModeController::OnTimerTick, base::TimeTicks::Now());

  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  if (turn_on_do_not_disturb_ && !message_center->IsQuietMode()) {
    // Only turn on DND if it is not enabled before starting a session and
    // `turn_on_do_not_disturb_` is true.
    message_center->SetQuietMode(
        true, message_center::QuietModeSourceType::kFocusMode);
  } else if (!turn_on_do_not_disturb_ && IsQuietModeOnSetByFocusMode()) {
    // This is the case where a user toggles off DND in the focus panel before
    // it has been switched off by the termination of the ending moment.
    message_center->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  } else if (turn_on_do_not_disturb_ && IsQuietModeOnSetByFocusMode()) {
    // This can only happen if a new focus session is started during an ending
    // moment. If the DND state is preserved (i.e. `turn_on_do_not_disturb_` is
    // still true), then just update the notification.
    MaybeUpdateDndNotification();
  }

  CloseSystemTrayBubble();
  SetFocusTrayVisibility(true);

  for (auto& observer : observers_) {
    observer.OnFocusModeChanged(/*in_focus_session=*/true);
  }
}

void FocusModeController::OnTimerTick() {
  CHECK(current_session_);
  auto session_snapshot = current_session_->GetSnapshot(base::Time::Now());
  switch (session_snapshot.state) {
    case FocusModeSession::State::kOn:
      for (auto& observer : observers_) {
        observer.OnTimerTick(session_snapshot);
      }
      return;
    case FocusModeSession::State::kEnding:
      timer_.Stop();

      // Set a timer to terminate the ending moment. If the focus tray bubble is
      // open, the ending moment will exist until the bubble is closed.
      if (!IsFocusTrayBubbleVisible()) {
        timer_.Start(FROM_HERE, focus_mode_util::kEndingMomentDuration, this,
                     &FocusModeController::ResetFocusSession,
                     base::TimeTicks::Now());
        MaybeUpdateDndNotification();
      } else {
        current_session_->set_persistent_ending();
      }

      for (auto& observer : observers_) {
        observer.OnFocusModeChanged(/*in_focus_session=*/false);
      }
      return;
    case FocusModeSession::State::kOff:
      ResetFocusSession();
      return;
  }
}

void FocusModeController::UpdateFromUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    // Can be null in tests.
    return;
  }

  session_duration_ =
      active_user_prefs->GetTimeDelta(prefs::kFocusModeSessionDuration);
  turn_on_do_not_disturb_ =
      active_user_prefs->GetBoolean(prefs::kFocusModeDoNotDisturb);

  if (session_duration_ <= base::TimeDelta()) {
    session_duration_ = kDefaultSessionDuration;
  }
}

void FocusModeController::SaveSettingsToUserPrefs() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    active_user_prefs->SetTimeDelta(prefs::kFocusModeSessionDuration,
                                    session_duration_);
    active_user_prefs->SetBoolean(prefs::kFocusModeDoNotDisturb,
                                  turn_on_do_not_disturb_);
  }
}

void FocusModeController::CloseSystemTrayBubble() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (root_window_controller->IsSystemTrayVisible()) {
      root_window_controller->GetStatusAreaWidget()
          ->unified_system_tray()
          ->CloseBubble();
    }
  }
}

void FocusModeController::SetFocusTrayVisibility(bool visible) {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (auto* status_area_widget =
            root_window_controller->GetStatusAreaWidget()) {
      auto* tray = status_area_widget->focus_mode_tray();
      if (!visible) {
        tray->CloseBubble();
      }
      tray->SetVisiblePreferred(visible);
    }
  }
}

bool FocusModeController::IsFocusTrayBubbleVisible() const {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (root_window_controller->GetStatusAreaWidget()
            ->focus_mode_tray()
            ->GetBubbleView()) {
      return true;
    }
  }
  return false;
}

}  // namespace ash
