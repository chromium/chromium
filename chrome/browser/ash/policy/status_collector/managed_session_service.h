// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_load_failed_observer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"

namespace ash {
class UserSessionManager;
}  // namespace ash

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace policy {

class ManagedSessionService : public ash::AuthStatusConsumer,
                              public ash::KioskProfileLoadFailedObserver,
                              public ash::SessionTerminationManager::Observer,
                              public ash::UserAuthenticatorObserver,
                              public chromeos::PowerManagerClient::Observer,
                              public ProfileObserver,
                              public session_manager::SessionManagerObserver,
                              public user_manager::UserManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Occurs when a user's login attempt fails.
    virtual void OnLoginFailure(const ash::AuthFailure& error) {}

    // Occurs when a user has logged in.
    virtual void OnLogin(Profile* profile) {}

    // Occurs when a user has logged out.
    // TODO(b/194215634):: Check if this function can be replaced by
    // `OnSessionTerminationStarted`
    virtual void OnLogout(Profile* profile) {}

    // Occurs when the active user has locked the user session.
    virtual void OnLocked() {}

    // TODO(b/247595531): Merge both Unlock functions into one.
    // Occurs when the active user has unlocked the user session.
    virtual void OnUnlocked() {}

    // Occurs when the active user attempts to unlock the user session.
    virtual void OnUnlockAttempt(
        const bool success,
        const session_manager::UnlockType unlock_type) {}

    // Occurs when the device recovers from a suspend state, where
    // |suspend_time| is the time when the suspend state
    // first occurred. Short duration suspends are not reported.
    virtual void OnResumeActive(base::Time suspend_time) {}

    // Occurs in the beginning of the session termination process.
    virtual void OnSessionTerminationStarted(const user_manager::User* user) {}

    // Occurs just before a user's account will be removed.
    virtual void OnUserToBeRemoved(const AccountId& account_id) {}

    // Occurs after a user's account is removed.
    virtual void OnUserRemoved(const AccountId& account_id,
                               user_manager::UserRemovalReason) {}

    virtual void OnKioskLoginFailure() {}
  };

  explicit ManagedSessionService(
      base::Clock* clock = base::DefaultClock::GetInstance());
  ManagedSessionService(const ManagedSessionService&) = delete;
  ManagedSessionService& operator=(const ManagedSessionService&) = delete;
  ~ManagedSessionService() override;

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  // session_manager::SessionManagerObserver::Observer
  void OnSessionStateChanged() override;
  void OnUserProfileLoaded(const AccountId& account_id) override;
  void OnUnlockScreenAttempt(
      const bool success,
      const session_manager::UnlockType unlock_type) override;

  // user_manager::Observer
  void OnUserToBeRemoved(const AccountId& account_id) override;
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // chromeos::PowerManagerClient::Observer
  void SuspendDone(base::TimeDelta sleep_duration) override;

  void OnOnlinePasswordUnusable(std::unique_ptr<ash::UserContext> user_context,
                                bool) override {}
  void OnPasswordChangeDetectedFor(const AccountId& account) override {}
  void OnOldEncryptionDetected(std::unique_ptr<ash::UserContext> user_context,
                               bool has_incomplete_migration) override {}
  void OnAuthSuccess(const ash::UserContext& user_context) override {}

  void OnAuthFailure(const ash::AuthFailure& error) override;

  void OnAuthAttemptStarted() override;

  void OnSessionWillBeTerminated() override;

  void OnKioskProfileLoadFailed() override;

 private:
  void SetLoginStatus();

  bool is_session_locked_;

  bool is_logged_in_observed_ = false;

  raw_ptr<base::Clock> clock_;

  base::ObserverList<Observer> observers_;

  const raw_ptr<session_manager::SessionManager> session_manager_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};
  base::ScopedObservation<ash::UserSessionManager,
                          ash::UserAuthenticatorObserver>
      authenticator_observation_{this};

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_MANAGED_SESSION_SERVICE_H_
