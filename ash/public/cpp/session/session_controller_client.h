// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "components/account_id/account_id.h"

class AccountId;
class PrefService;

namespace ash {

class ASH_PUBLIC_EXPORT SessionControllerClient {
 public:
  // Requests to lock screen.
  virtual void RequestLockScreen() = 0;

  // Requests signing out all users, ending the current session.
  virtual void RequestSignOut() = 0;

  // Switch to the active user with |account_id| (if the user has already signed
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

 protected:
  virtual ~SessionControllerClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_CLIENT_H_
