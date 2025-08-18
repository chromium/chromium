// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_OBSERVER_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "components/session_manager/session_manager_types.h"

class AccountId;
class PrefService;

namespace ash {

enum class LoginStatus;

class ASH_PUBLIC_EXPORT SessionObserver : public base::CheckedObserver {
 public:
  // Called when the active user session has changed.
  virtual void OnActiveUserSessionChanged(const AccountId& account_id) {}

  // Called when a user session gets added to the existing session.
  virtual void OnUserSessionAdded(const AccountId& account_id) {}

  // Called when the first user session starts. Note this is called before the
  // first user session is fully initialized. Post login works might still be
  // pending.
  virtual void OnFirstSessionStarted() {}

  // Called when the first user session finishes post login works.
  virtual void OnFirstSessionReady() {}

  // Called when the session state is changed.
  virtual void OnSessionStateChanged(session_manager::SessionState state) {}

  // Called when the Kiosk user session starts. This happens during Kiosk sign
  // in before the profile is ready, quite early during Kiosk launch.
  //
  // This signal complements `OnSessionStateChanged` because `SessionState`
  // remains `LOGIN_PRIMARY` during most of the Kiosk launch process.
  //
  // The expected ordering of events is:
  // * `OnSessionStateChanged(LOGIN_PRIMARY)` -> Device is in the login screen.
  // * User launches a Kiosk app from the apps menu, or an app is auto launched.
  // * `OnAppModeSessionStarted()` -> Kiosk launch started.
  // * `KioskLaunchController` loads the user profile and installs the app.
  // * `OnSessionStateChanged(LOGGED_IN_NOT_ACTIVE)` -> Kiosk is ready to
  //   launch the app.
  // * `OnSessionStateChanged(ACTIVE)` -> The Kiosk app has launched.
  virtual void OnAppModeSessionStarted() {}

  // Called when the login status is changed. |login_status| is the new status.
  virtual void OnLoginStatusChanged(LoginStatus login_status) {}

  // Called when the lock state is changed. |locked| is the current lock stated.
  virtual void OnLockStateChanged(bool locked) {}

  // Called when chrome is terminating.
  virtual void OnChromeTerminating() {}

  // Called when the limit becomes available and when it changes.
  virtual void OnSessionLengthLimitChanged() {}

  // Called when the signin screen profile |prefs| are ready.
  virtual void OnSigninScreenPrefServiceInitialized(PrefService* prefs) {}

  // Called when the PrefService for the active user session changes, i.e.
  // after the first user logs in, after a multiprofile user is added, and after
  // switching to a different multiprofile user. Happens later than
  // OnActiveUserSessionChanged() because the PrefService is asynchronously
  // initialized. Never called with null.
  virtual void OnActiveUserPrefServiceChanged(PrefService* pref_service) {}

  // Called when the user is going to be removed soon.
  virtual void OnUserToBeRemoved(const AccountId& account_id) {}

 protected:
  ~SessionObserver() override = default;
};

// A class to attach / detach an object as a session state observer.
//
// NOTE: Both ash::Shell and ash::SessionControllerImpl must outlive your
// object. You may find it clearer to manually add and remove your observer.
class ASH_PUBLIC_EXPORT ScopedSessionObserver {
 public:
  explicit ScopedSessionObserver(SessionObserver* observer);

  ScopedSessionObserver(const ScopedSessionObserver&) = delete;
  ScopedSessionObserver& operator=(const ScopedSessionObserver&) = delete;

  virtual ~ScopedSessionObserver();

 private:
  const raw_ptr<SessionObserver> observer_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_OBSERVER_H_
