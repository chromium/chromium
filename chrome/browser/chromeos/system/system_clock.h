// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_H_

#include <memory>

#include "base/callback_list.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/user_manager/user_manager.h"

class PrefChangeRegistrar;

namespace user_manager {
class User;
}

namespace chromeos {
namespace system {

class SystemClockObserver;

// This is a global singleton (actually a member of BrowserProcessPlatformPart)
// that is responsible for correct time formatting. It listens to events that
// modify on-screen time representation (like ActiveUserChanged) and notifies
// observers.
class SystemClock : public chromeos::LoginState::Observer,
                    public ProfileObserver,
                    public user_manager::UserManager::UserSessionStateObserver {
 public:
  SystemClock();
  ~SystemClock() override;

  void SetLastFocusedPodHourClockType(base::HourClockType hour_clock_type);

  void AddObserver(SystemClockObserver* observer);
  void RemoveObserver(SystemClockObserver* observer);

  bool ShouldUse24HourClock() const;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

 private:
  // Should be the same as CrosSettings::ObserverSubscription.
  typedef base::CallbackList<void(void)>::Subscription
      CrosSettingsObserverSubscription;

  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  void OnSystemPrefChanged();

  void UpdateClockType();

  bool user_pod_was_focused_ = false;
  base::HourClockType last_focused_pod_hour_clock_type_ = base::k12HourClock;

  Profile* user_profile_ = nullptr;
  ScopedObserver<Profile, ProfileObserver> profile_observer_{this};
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;

  base::ObserverList<SystemClockObserver>::Unchecked observer_list_;

  std::unique_ptr<CrosSettingsObserverSubscription> device_settings_observer_;

  base::WeakPtrFactory<SystemClock> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemClock);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSTEM_CLOCK_H_
