// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "base/files/file_path.h"
#include "components/account_id/account_id.h"

class AccountId;
class PrefService;

namespace ash {

class ASH_PUBLIC_EXPORT SessionControllerClient {
 public:
  // Requests to lock screen.
  virtual void RequestLockScreen() = 0;

  // Requests to dismiss the lock screen.
  virtual void RequestHideLockScreen() = 0;

  // Requests signing out all users, ending the current session.
  virtual void RequestSignOut() = 0;

  // Requests to restart the system for OS update.
  virtual void RequestRestartForUpdate() = 0;

  // Attempts to restart the chrome browser.
  virtual void AttemptRestartChrome() = 0;

  // Switch to the active user with `account_id` (if the user has already signed
  // in).
  virtual void SwitchActiveUser(const AccountId& account_id) = 0;

  // Switch the active user to the next or previous user.
  virtual void CycleActiveUser(CycleUserDirection direction) = 0;

  // Show the multi-profile login UI to add another user to this session.
  virtual void ShowMultiProfileLogin() = 0;

  // Emits the ash-initialized upstart signal to start Chrome OS tasks that
  // expect that Ash is listening to D-Bus signals they emit.
  virtual void EmitAshInitialized() = 0;

  // Returns the sign-in screen pref service if available.
  virtual PrefService* GetSigninScreenPrefService() = 0;

  // Returns the pref service for the given user if available.
  virtual PrefService* GetUserPrefService(const AccountId& account_id) = 0;

  // Returns the profile path for `account_id` or empty if one does not exist.
  virtual base::FilePath GetProfilePath(const AccountId& account_id) = 0;

  // Returns a tuple of whether
  // <IsVcBackgroundSupported, IsVcBackgroundAllowedByEnterprise>.
  // TODO(b/333767964): this is only a temporary solution. Having a function
  // here for each project does not sound ideal; this should be replaced with
  // more general approach.
  virtual std::tuple<bool, bool> IsEligibleForSeaPen(
      const AccountId& account_id) = 0;

  // Return the number of users that have previously logged in on the device.
  // Returns nullopt in the event where we cannot query the number of existing
  // users, for instance, when `UserManager` is uninitialized.
  virtual std::optional<int> GetExistingUsersCount() const = 0;

 protected:
  virtual ~SessionControllerClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_
