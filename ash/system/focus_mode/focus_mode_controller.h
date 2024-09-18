// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/focus_mode/focus_mode_delegate.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_tasks_model.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;

namespace base {
class UnguessableToken;
}  // namespace base

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AshWebView;
class FocusModeMetricsRecorder;
class FocusModeSoundsController;

// Controls starting and ending a Focus Mode session and its behavior. Also
// keeps track of the system state to restore after a Focus Mode session ends.
// Has a timer that runs while a session is active and notifies `observers_` on
// every timer tick.
class ASH_EXPORT FocusModeController
    : public SessionObserver,
      public FocusModeSoundsController::Observer,
      public FocusModeTasksModel::Observer,
      public FocusModeTasksModel::Delegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever Focus Mode changes as a result of user action or when the
    // session duration expires.
    virtual void OnFocusModeChanged(FocusModeSession::State session_state) = 0;

    // Called every `timer_` tick for updating UI elements during a Focus Mode
    // session.
    virtual void OnTimerTick(
        const FocusModeSession::Snapshot& session_snapshot) {}

    // Notifies when the session duration is changed in the focus panel without
    // an active session.
    virtual void OnInactiveSessionDurationChanged(
        const base::TimeDelta& session_duration) {}

    // Notifies clients every time the session duration is changed during an
    // active session.
    virtual void OnActiveSessionDurationChanged(
        const FocusModeSession::Snapshot& session_snapshot) {}
  };

  explicit FocusModeController(std::unique_ptr<FocusModeDelegate> delegate);
  FocusModeController(const FocusModeController&) = delete;
  FocusModeController& operator=(const FocusModeController&) = delete;
  ~FocusModeController() override;

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static FocusModeController* Get();

  // Verifies that the session duration hasn't reached `kMaximumDuration`.
  static bool CanExtendSessionDuration(
      const FocusModeSession::Snapshot& snapshot);

  // Registers user profile prefs with the specified `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  bool in_focus_session() const {
    return current_session_ && current_session_->GetState(base::Time::Now()) ==
                                   FocusModeSession::State::kOn;
  }
  bool in_ending_moment() const {
    return current_session_ && current_session_->GetState(base::Time::Now()) ==
                                   FocusModeSession::State::kEnding;
  }
  base::TimeDelta session_duration() const { return session_duration_; }
  bool turn_on_do_not_disturb() const { return turn_on_do_not_disturb_; }
  void set_turn_on_do_not_disturb(bool turn_on) {
    turn_on_do_not_disturb_ = turn_on;
  }
  const std::optional<FocusModeSession>& current_session() const {
    return current_session_;
  }
  size_t congratulatory_index() const { return congratulatory_index_; }

  FocusModeTasksModel& tasks_model() { return tasks_model_; }
  FocusModeSoundsController* focus_mode_sounds_controller() const {
    return focus_mode_sounds_controller_.get();
  }
  FocusModeDelegate* delegate() { return delegate_.get(); }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts or ends a focus session by a toggle `source`.
  void ToggleFocusMode(
      focus_mode_histogram_names::ToggleSource source =
          focus_mode_histogram_names::ToggleSource::kFocusPanel);

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // FocusModeSoundsController::Observer:
  // Will close/create the media widget for an active focus session depending on
  // if there is a selected playlist or not.
  void OnSelectedPlaylistChanged() override;

  // FocusModeTasksModel::Observer:
  void OnSelectedTaskChanged(const std::optional<FocusModeTask>& task) override;
  void OnTasksUpdated(const std::vector<FocusModeTask>& tasks) override;
  void OnTaskCompleted(const FocusModeTask& completed_task) override;

  // FocusModeTasksModel::Delegate:
  void FetchTask(
      const TaskId& task_id,
      FocusModeTasksModel::Delegate::FetchTaskCallback callback) override;
  void FetchTasks() override;
  void AddTask(
      const FocusModeTasksModel::TaskUpdate& update,
      FocusModeTasksModel::Delegate::FetchTaskCallback callback) override;
  void UpdateTask(const FocusModeTasksModel::TaskUpdate& update) override;

  // Extends an active focus session by ten minutes by clicking the `+10 min`
  // button.
  void ExtendSessionDuration();

  // Resets the focus session state for when the session needs to end (i.e. the
  // user manually ends the session, or when the ending moment is terminated).
  // This ensures that states are all reverted (especially DND and UI elements).
  void ResetFocusSession();

  // Used to clear some controller states when the user clicks the tray icon to
  // show the ending moment bubble.
  void OnEndingBubbleShown();

  // Sets a specific value for `session_duration_`. We have two different
  // notions of a session, so this one is only in charge of updating the session
  // duration that will be applied to the next active session. Also notifies
  // observers that the session duration was changed. An "inactive" session can
  // either be no `current_session_`, or if we are in the ending moment, since
  // the user should still be able to adjust and start a new session during that
  // time.
  void SetInactiveSessionDuration(const base::TimeDelta& new_session_duration);

  // Returns whether the user has ever started a focus session previously.
  bool HasStartedSessionBefore() const;

  // Creates and returns a snapshot of the current session based on `now`.
  // Returns a default struct if there is no session.
  FocusModeSession::Snapshot GetSnapshot(const base::Time& now) const;

  // Returns the session duration of either the current session, or what the
  // upcoming session will be set to.
  base::TimeDelta GetSessionDuration() const;

  // Returns the end time of an active session. This end time is meant to be
  // displayed, and may be different depending on the session state (e.g. the
  // ending moment needs to account for the extra duration).
  base::Time GetActualEndTime() const;

  // Stores the provided `task`.
  void SetSelectedTask(const FocusModeTask& task);

  // Returns whether there is a currently selected task.
  bool HasSelectedTask() const;

  // Marks the task as completed in the model.
  void CompleteTask();

  // Shows the ending moment nudge that is anchored to the focus mode tray. Only
  // show if there isn't already showing and if there is no tray bubble open.
  void MaybeShowEndingMomentNudge();

  // This is currently only used in testing to trigger an ending moment
  // immediately if there is an ongoing session.
  void TriggerEndingMomentImmediately();

  // Helper functions for enabling/disabling DND.
  void MaybeEnableDND();

  // Used during the ending moment to disable DND after a timeout, instead of
  // waiting until the persistent ending moment is dismissed.
  void MaybeDisableDND();

  // When an active session ends, we bounce the tray icon and show the ending
  // moment nudge. This is triggered every `kEndingMomentBounceAnimationDelay`
  // as long as the ending moment hasn't been terminated by the user.
  void BounceTrayIcon();

  // Get the request id for the media session played for Focus Sounds.
  const base::UnguessableToken& GetMediaSessionRequestId();

  // If `create_media_widget` is true, we will assign a valid value to
  // `test_media_request_id_`; otherwise, we will reset it due to simulating no
  // media widget exists.
  void SetMediaSessionRequestIdForTesting(bool create_media_widget) {
    test_media_request_id_ = create_media_widget
                                 ? base::UnguessableToken::Create()
                                 : base::UnguessableToken::Null();
  }

  void RequestTasksUpdateForTesting();
  bool TasksProviderHasCachedTasksForTesting() const;

  media_session::mojom::MediaSessionInfoPtr GetSystemMediaSessionInfo();
  void SetSystemMediaSessionInfoForTesting(
      media_session::mojom::MediaSessionInfoPtr media_session_info) {
    test_media_session_info_ = std::move(media_session_info);
  }

 private:
  // Starts a focus session by updating UI elements, starting `timer_`, and
  // setting `current_session_` to the desired session duration and end time.
  void StartFocusSession(focus_mode_histogram_names::ToggleSource source);

  // Called every time a second passes on `timer_` while the session is active.
  void OnTimerTick();

  // This is called when the active user changes, and is important to update our
  // cached values in case different users have different stored preferences.
  void UpdateFromUserPrefs();

  // Called by `UpdateFromUserPrefs` to update our cached values for the active
  // user about the selected task.
  void UpdateSelectedTaskFromUserPrefs();

  // Called once a session starts. Saves the current selected settings to user
  // prefs so we can provide the same set-up the next time the user comes back
  // to Focus Mode.
  void SaveSettingsToUserPrefs();

  // Called once a session starts, and when a task is selected or deselected in
  // focus session.
  void SaveSelectedTaskSettingsToUserPrefs(
      const std::optional<FocusModeTask>& task);

  // Closes any open system tray bubbles. This is done whenever we start a focus
  // session.
  void CloseSystemTrayBubble();

  // Sets the visibility of the focus tray on the shelf.
  void SetFocusTrayVisibility(bool visible);

  // This tells us if there is an open focus mode tray bubble on any of the
  // displays.
  bool IsFocusTrayBubbleVisible() const;

  // Creates the media widget if one doesn't already exist and if there is a
  // selected playlist. Returns true if we create a new media widget.
  bool MaybeCreateMediaWidget();
  void CloseMediaWidget();

  // Called when the user extends the ending moment. This function will create a
  // new media widget, or resume playing the existing media.
  void PerformActionsForMusic();

  void OnTasksReceived(const std::vector<FocusModeTask>& tasks);

  // Gives Focus Mode access to the Google Tasks API.
  FocusModeTasksProvider tasks_provider_;

  FocusModeTasksModel tasks_model_;

  // This is the expected duration of a Focus Mode session once it starts.
  // Depends on previous session data (from user prefs) or user input.
  base::TimeDelta session_duration_;

  // This will dictate whether DND will be turned on when a Focus Mode session
  // starts. Depends on previous session data (from user prefs) or user input.
  bool turn_on_do_not_disturb_ = true;

  // This timer is used for keeping track of the Focus Mode session duration and
  // will trigger a callback every second during a session. It will terminate
  // once the session goes into the `kEnding` state, or if a user toggles off
  // Focus Mode.
  base::MetronomeTimer timer_;

  // This is used to track the current session, if any.
  std::optional<FocusModeSession> current_session_;

  // A random value between 0 and `focus_mode_util::kCongratulatoryTitleNum -
  // 1`.
  size_t congratulatory_index_ = 0;

  std::unique_ptr<FocusModeMetricsRecorder> focus_mode_metrics_recorder_;

  // This is used to display focus mode playlists. Playback controls will be
  // added later.
  std::unique_ptr<FocusModeSoundsController> focus_mode_sounds_controller_;

  // The media widget and its contents view.
  std::unique_ptr<views::Widget> media_widget_;
  raw_ptr<AshWebView> focus_mode_media_view_ = nullptr;

  // True if a playing selected playlist was paused automatically when entering
  // the ending moment. If `paused_by_ending_moment_` is true, after the user
  // extended the session, the selected playlist will resume playing if it's
  // still selected.
  bool paused_by_ending_moment_ = false;

  // The info about the current media session for testing. It will be null if
  // there isn't a current media session.
  media_session::mojom::MediaSessionInfoPtr test_media_session_info_;
  // The media session request id for testing.
  base::UnguessableToken test_media_request_id_ =
      base::UnguessableToken::Null();

  std::unique_ptr<FocusModeDelegate> delegate_;

  base::ScopedObservation<FocusModeTasksModel, FocusModeController>
      tasks_model_observation_{this};
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<FocusModeController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CONTROLLER_H_
