// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_OBSERVER_H_
#define ASH_SESSION_SESSION_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list_types.h"
#include "components/session_manager/session_manager_types.h"

class AccountId;
class PrefService;

namespace ash {

enum class LoginStatus;

class ASH_EXPORT SessionObserver : public base::CheckedObserver {
 public:
  // Called when the active user session has changed.
  virtual void OnActiveUserSessionChanged(const AccountId& account_id) {}

  // Called when a user session gets added to the existing session.
  virtual void OnUserSessionAdded(const AccountId& account_id) {}

  // Called once the first time a user session starts.
  virtual void OnFirstSessionStarted() {}

  // Called when a user session is updated, such as avatar change.
  virtual void OnUserSessionUpdated(const AccountId& account_id) {}

  // Called when the session state is changed.
  virtual void OnSessionStateChanged(session_manager::SessionState state) {}

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

 protected:
  ~SessionObserver() override {}
};

// A class to attach / detach an object as a session state observer.
//
// NOTE: Both ash::Shell and ash::SessionControllerImpl must outlive your
// object. You may find it clearer to manually add and remove your observer.
class ASH_EXPORT ScopedSessionObserver {
 public:
  explicit ScopedSessionObserver(SessionObserver* observer);
  virtual ~ScopedSessionObserver();

 private:
  SessionObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSessionObserver);
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_OBSERVER_H_
