// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

namespace reporting {

// Frequency that session activity is periodically reported.
static constexpr base::TimeDelta kReportingFrequency = base::Hours(1);

// Frequency with which we collect the active/idle of the device.
static constexpr base::TimeDelta kActiveIdleStateCollectionFrequency =
    base::Seconds(30);

// This class reports user session start/end events and the user's active/idle
// states during the session. Reports are sent periodically during a session,
// and when the session ends.
//
// The following events are considered session start events:
// * Login
// * Unlock
//
// The following events are considered session end events:
// * Logout
// * Lock
//
// The following events imply that the user is active
// * Keyboard, mouse interaction
// * Media playing
// * Changing the device's power source
//
// The following types of user sessions are reported:
// * Managed guest sessions
// * Regular affiliated users
// * Regular unaffiliated users
class UserSessionActivityReporter
    : public policy::ManagedSessionService::Observer,
      user_manager::UserManager::UserSessionStateObserver {
 public:
  // The delegate is responsible for managing the internal session activity
  // state, reporting the state, and retrieving active/idle status from the
  // device. Implemented as a virtual class for easy mocking in tests.
  //
  // Important note: the delegate does NOT persist state to disk, so if Chrome
  // crashes in the middle of a session, data is lost. This is a conscious
  // choice to tradeoff safety for performance (with respect to the number of
  // events we send to the reporting server) since this data is not
  // considered security-critical; Login/logout and lock/unlock events are
  // independently reported and controlled by the
  // `ash::kReportDeviceLoginLogout` setting.
  class Delegate {
   public:
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    virtual ~Delegate() = default;

    // Gets the device active/idle status.
    virtual ash::power::ml::IdleEventNotifier::ActivityData QueryIdleStatus()
        const = 0;

    // Returns true if the user has been recently active during the session.
    // False otherwise.
    virtual bool IsUserActive(
        const ash::power::ml::IdleEventNotifier::ActivityData& activity_data)
        const = 0;

    // Enqueues a record containing the session activity to the encrypted
    // reporting pipeline.
    virtual void ReportSessionActivity() = 0;

    // Adds an active or idle state to the current session activity state.
    virtual void AddActiveIdleState(bool user_is_active,
                                    const user_manager::User*) = 0;

    // Sets the session start field in the session activity state.
    virtual void SetSessionStartEvent(reporting::SessionStartEvent::Reason,
                                      const user_manager::User*) = 0;

    // Sets the session end field in the session activity state.
    virtual void SetSessionEndEvent(reporting::SessionEndEvent::Reason,
                                    const user_manager::User*) = 0;

   protected:
    Delegate() = default;
  };

  UserSessionActivityReporter(const UserSessionActivityReporter& other) =
      delete;
  UserSessionActivityReporter& operator=(
      const UserSessionActivityReporter& other) = delete;

  ~UserSessionActivityReporter() override;

  static std::unique_ptr<UserSessionActivityReporter> Create(
      policy::ManagedSessionService* managed_session_service,
      user_manager::UserManager* user_manager);

 protected:
  friend class UserSessionActivityReporterTest;

  // Constructor is protected so it can be called in tests.
  UserSessionActivityReporter(
      policy::ManagedSessionService* managed_session_service,
      user_manager::UserManager* user_manager,
      std::unique_ptr<Delegate> delegate);

  // UserManager::UserSessionStateObserver
  // Called on login, unlock, and when switching between users when
  // multiple users are logged in.
  void ActiveUserChanged(user_manager::User* active_user) override;

  // ManagedSessionService::Observer
  // Called on logout.
  void OnSessionTerminationStarted(const user_manager::User* user) override;

  // ManagedSessionService::Observer
  // Called when device is locked.
  void OnLocked() override;

 private:
  // Starts the user session. Called by OnLogin() and OnUnlocked().
  void OnSessionStart(reporting::SessionStartEvent::Reason reason,
                      const user_manager::User* user);

  // Ends the user session. Called by OnSessionTerminationStarted() and
  // OnLocked().
  void OnSessionEnd(reporting::SessionEndEvent::Reason reason,
                    const user_manager::User* user);

  void OnReportingTimerExpired();

  void OnCollectionTimerExpired();

  // Retrieves the user's active/idle status and adds it to the delegate state.
  void UpdateActiveIdleState();

  void StartTimers();

  void StopTimers();

  bool IsSessionActive() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Observes logouts, lock, and unlock events.
  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_service_observation_{this};

  // Observes logins and account switching (multi-user mode) events.
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};

  // Timer which tracks when it's time to report the current session active/idle
  // states.
  base::RepeatingTimer reporting_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer which tracks when it's time to collect the user's active/idle state.
  base::RepeatingTimer collect_idle_state_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Delegate which stores, modifies, and reports user session activity.
  // Not thread safe - must only be called on `task_runner_`
  const std::unique_ptr<Delegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Boolean to track whether or not the device is locked.
  bool is_device_locked_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // User of an active session. Null when there's no active session.
  raw_ptr<const user_manager::User> session_user_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Must be the last member.
  base::WeakPtrFactory<UserSessionActivityReporter> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_H_
