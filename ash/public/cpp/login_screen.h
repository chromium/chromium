// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/callback_forward.h"

namespace ash {

class LoginScreenClient;
class LoginScreenModel;
class ScopedGuestButtonBlocker;

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

  // Sets a handler for login shelf gestures. This will enable gesture detection
  // on the login shelf for upward fling from the shelf.
  // |message| - The text to be shown above login shelf drag handle.
  // |fling_callback| - The callback to be called when a fling is detected.
  // |exit_callback| - The callback to be called when the login shelf gesture
  // detection stops, for example when the session is unblocked, or the handler
  // is cleared.
  //
  // Returns whether the handler was successfully set. If not, |exit_callback|
  // will not be run. The handler will not be set if the current shelf state
  // does not support login shelf gestures, e.g. if the session is active, or
  // when not in tablet mode.
  //
  // Note that this does not support more than one handler - if another handler
  // is already set, this method will replace it (and the previous handler's
  // exit_callback will be run).
  virtual bool SetLoginShelfGestureHandler(
      const base::string16& message,
      const base::RepeatingClosure& fling_callback,
      base::OnceClosure exit_callback) = 0;

  // Stops login shelf gesture detection.
  virtual void ClearLoginShelfGestureHandler() = 0;

 protected:
  LoginScreen();
  virtual ~LoginScreen();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_H_
