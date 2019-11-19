// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/callback_forward.h"
#include "base/time/time.h"

class AccountId;

namespace ash {

class LoginScreenClient;
class LoginScreenModel;
class ScopedGuestButtonBlocker;

enum class ParentAccessRequestReason;

// Allows clients (e.g. the browser process) to send messages to the ash
// login/lock/user-add screens.
// TODO(estade): move more of mojom::LoginScreen here.
class ASH_PUBLIC_EXPORT LoginScreen {
 public:
  // Returns the singleton instance.
  static LoginScreen* Get();

  virtual void SetClient(LoginScreenClient* client) = 0;

  virtual LoginScreenModel* GetModel() = 0;

  // Displays the lock screen.
  virtual void ShowLockScreen() = 0;

  // Displays the login screen.
  virtual void ShowLoginScreen() = 0;

  // Display a toast describing the latest kiosk app launch error.
  virtual void ShowKioskAppError(const std::string& message) = 0;

  // Transitions focus to the shelf area. If |reverse|, focuses the status area.
  virtual void FocusLoginShelf(bool reverse) = 0;

  // Returns if the login/lock screen is ready for a password. Currently only
  // used for testing.
  virtual bool IsReadyForPassword() = 0;

  // Sets whether users can be added from the login screen.
  virtual void EnableAddUserButton(bool enable) = 0;

  // Sets whether shutdown button is enabled in the login screen.
  virtual void EnableShutdownButton(bool enable) = 0;

  // Shows or hides the guest button on the login shelf during OOBE.
  virtual void ShowGuestButtonInOobe(bool show) = 0;

  // Shows or hides the parent access button on the login shelf.
  virtual void ShowParentAccessButton(bool show) = 0;

  // Shows a standalone Parent Access dialog. If |child_account_id| is valid, it
  // validates the parent access code for that child only, when it is  empty it
  // validates the code for any child signed in the device. |callback| is
  // invoked when the back button is clicked or the correct code was entered.
  // |reason| contains information about why the parent access view is
  // necessary, it is used to modify the view appearance by changing the title
  // and description strings and background color. The parent access widget is a
  // modal and already contains a dimmer, however when another modal is the
  // parent of the widget, the dimmer will be placed behind the two windows.
  // |extra_dimmer| will create an extra dimmer between the two.
  // |validation_time| is the time that will be used to validate the code, if
  // null the system's time will be used. Note: this is intended for children
  // only. If a non child account id is provided, the validation will
  // necessarily fail.
  virtual void ShowParentAccessWidget(
      const AccountId& child_account_id,
      base::RepeatingCallback<void(bool success)> callback,
      ParentAccessRequestReason reason,
      bool extra_dimmer,
      base::Time validation_time) = 0;

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  virtual void SetAllowLoginAsGuest(bool allow_guest) = 0;

  // Returns scoped object to temporarily disable Browse as Guest button.
  virtual std::unique_ptr<ScopedGuestButtonBlocker>
  GetScopedGuestButtonBlocker() = 0;

  // Called to request the user to enter the PIN of the security token (e.g.,
  // the smart card).
  virtual void RequestSecurityTokenPin(SecurityTokenPinRequest request) = 0;

  // Called to close the UI previously opened with RequestSecurityTokenPin().
  virtual void ClearSecurityTokenPinRequest() = 0;

 protected:
  LoginScreen();
  virtual ~LoginScreen();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
