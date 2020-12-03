// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_AFFILIATED_SESSION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_AFFILIATED_SESSION_SERVICE_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace policy {

class AffiliatedSessionService : public session_manager::SessionManagerObserver,
                                 public ProfileObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Occurs when an affiliated primary user has logged in.
    virtual void OnAffiliatedLogin(Profile* profile) {}

    // Occurs when an affiliated primary user has logged out.
    virtual void OnAffiliatedLogout(Profile* profile) {}

    // Occurs when the active user has locked the user session.
    virtual void OnLocked() {}

    // Occurs when the active user has unlocked the user session.
    virtual void OnUnlocked() {}

    // Occurs when the device recovers from a suspend state, where
    // |suspend_time| is the time when the suspend state
    // first occurred. Short duration suspends are not reported.
    virtual void OnResumeActive(base::Time suspend_time) {}
  };

  explicit AffiliatedSessionService(
      base::Clock* clock = base::DefaultClock::GetInstance());
  AffiliatedSessionService(const AffiliatedSessionService&) = delete;
  AffiliatedSessionService& operator=(const AffiliatedSessionService&) = delete;
  ~AffiliatedSessionService() override;

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  // session_manager::SessionManagerObserver::Observer
  void OnSessionStateChanged() override;
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // chromeos::PowerManagerClient::Observer
  void SuspendDone(base::TimeDelta sleep_duration) override;

 private:
  bool is_session_locked_;

  base::Clock* clock_;

  base::ObserverList<Observer> observers_;

  session_manager::SessionManager* const session_manager_;

  ScopedObserver<Profile, ProfileObserver> profile_observer_{this};
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_manager_observer_{this};
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_observer_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_AFFILIATED_SESSION_SERVICE_H_
