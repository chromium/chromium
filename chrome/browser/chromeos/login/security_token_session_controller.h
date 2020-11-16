// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace chromeos {
namespace login {

// A controller that implements the combined behavior of the
// SecurityTokenSessionBehavior and SecurityTokenSessionNotificationSeconds
// preferences. When a user is authenticating via a security token (e.g., with a
// smart card), SecurityTokenSessionBehavior dictates what should happen if the
// certificate ceases to be present while the user is logged in.
// SecurityTokenSessionNotificationSeconds determines if and how long the user
// is getting informed what is going to happen when the certificate vanishes.
class SecurityTokenSessionController : public KeyedService {
 public:
  enum class Behavior { kIgnore, kLogout, kLock };

  SecurityTokenSessionController(PrefService* local_state,
                                 PrefService* profile_prefs,
                                 const user_manager::User* user);
  SecurityTokenSessionController(const SecurityTokenSessionController& other) =
      delete;
  SecurityTokenSessionController& operator=(
      const SecurityTokenSessionController& other) = delete;
  ~SecurityTokenSessionController() override;

  // KeyedService
  void Shutdown() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // If this controller logged the user out just before, display a notification
  // explaining why this happened. This is only done the first time this
  // happens for a user on a device.
  static void MaybeDisplayLoginScreenNotification();

 private:
  Behavior GetBehaviorFromPref() const;
  void UpdateBehaviorPref();
  void UpdateNotificationPref();

  void AddLockNotification() const;
  void ScheduleLogoutNotification() const;

  PrefService* const local_state_;
  PrefService* const profile_prefs_;
  const user_manager::User* const user_;
  PrefChangeRegistrar pref_change_registrar_;
  Behavior behavior_ = Behavior::kIgnore;
  base::TimeDelta notification_seconds_;
};

}  // namespace login
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_
