// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_CLIENT_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/time/time.h"

class AccountId;

namespace ash {

// An interface allows Ash to trigger certain login steps that Chrome is
// responsible for.
class ASH_PUBLIC_EXPORT LoginScreenClient {
 public:
  // Attempt to authenticate a user with a password or PIN.
  //
  // If auth succeeds:
  // chrome will hide the lock screen and clear any displayed error messages.
  // If auth fails:
  // chrome will request lock screen to show error messages.
  // |account_id|: The AccountId to authenticate against.
  // |password|: The submitted password.
  // |authenticated_by_pin|: True if we are using pin to authenticate.
  //
  // The result will be set to true if auth was successful, false if not.
  //
  // TODO(jdufault): Extract authenticated_by_pin into a separate method,
  //                 similar to the other Authenticate* methods
  virtual void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) = 0;

  // Attempt to authenticate the user with with an external binary.
  virtual void AuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  // Attempt to enroll a user in the external binary authentication system.
  virtual void EnrollUserWithExternalBinary(
      base::OnceCallback<void(bool)> callback) = 0;

  // Try to authenticate |account_id| using easy unlock. This can be used on the
  // login or lock screen.
  // |account_id|: The account id of the user we are authenticating.
  //
  // TODO(jdufault): Refactor this method to return an auth_success, similar to
  // the other auth methods above.
  virtual void AuthenticateUserWithEasyUnlock(const AccountId& account_id) = 0;

  // Try to authenticate |account_id| using the challenge-response protocol
  // against a security token.
  // |account_id|: The account id of the user we are authenticating.
  virtual void AuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) = 0;

  // Validates parent access code for the user identified by |account_id|. When
  // |account_id| is empty it tries to validate the access code for any child
  // that is signed in the device. Returns validation result. |validation_time|
  // is the time that will be used to validate the code, validation will succeed
  // if the code was valid this given time. Note: This should only be used for
  // child user, it will always return false when a non-child id is used.
  // TODO(crbug.com/965479): move this to a more appropriate place.
  virtual bool ValidateParentAccessCode(const AccountId& account_id,
                                        const std::string& access_code,
                                        base::Time validation_time) = 0;

  // Request to hard lock the user pod.
  // |account_id|:    The account id of the user in the user pod.
  virtual void HardlockPod(const AccountId& account_id) = 0;

  // Focus user pod of user with |account_id|.
  virtual void OnFocusPod(const AccountId& account_id) = 0;

  // Notify that no user pod is focused.
  virtual void OnNoPodFocused() = 0;

  // Load wallpaper of user with |account_id|.
  virtual void LoadWallpaper(const AccountId& account_id) = 0;

  // Sign out current user.
  virtual void SignOutUser() = 0;

  // Close add user screen.
  virtual void CancelAddUser() = 0;

  // Launches guest mode.
  virtual void LoginAsGuest() = 0;

  // User with |account_id| has reached maximum incorrect password attempts.
  virtual void OnMaxIncorrectPasswordAttempted(const AccountId& account_id) = 0;

  // Should pass the focus to the active lock screen app window, if there is
  // one. This is called when a lock screen app is reported to be active (using
  // tray_action mojo interface), and is next in the tab order.
  // |HandleFocusLeavingLockScreenApps| should be called to return focus to the
  // lock screen.
  // |reverse|:   Whether the tab order is reversed.
  virtual void FocusLockScreenApps(bool reverse) = 0;

  // Passes focus to the OOBE dialog if it is showing. No-op otherwise.
  virtual void FocusOobeDialog() = 0;

  // Show the gaia sign-in dialog. If |can_close| is true, the dialog can be
  // closed. The value in |prefilled_account| will be used to prefill the
  // sign-in dialog so the user does not need to type the account email.
  virtual void ShowGaiaSignin(bool can_close,
                              const AccountId& prefilled_account) = 0;

  // Notification that the remove user warning was shown.
  virtual void OnRemoveUserWarningShown() = 0;

  // Try to remove |account_id|.
  virtual void RemoveUser(const AccountId& account_id) = 0;

  // Launch a public session for user with |account_id|.
  // |locale|:       Locale for this user.
  //                 The value is language code like "en-US", "zh-CN"
  // |input_method|: Input method for this user.
  //                 This is the id of InputMethodDescriptor like
  //                 "t:latn-post", "pinyin".
  virtual void LaunchPublicSession(const AccountId& account_id,
                                   const std::string& locale,
                                   const std::string& input_method) = 0;

  // Request public session keyboard layouts for user with |account_id|.
  // This function send a request to chrome and the result will be returned by
  // SetPublicSessionKeyboardLayouts.
  // |locale|: Request a list of keyboard layouts that can be used by this
  //           locale.
  virtual void RequestPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale) = 0;

  // Request to show a feedback report dialog in chrome.
  virtual void ShowFeedback() = 0;

  // Show the powerwash (device reset) dialog.
  virtual void ShowResetScreen() = 0;

  // Show the help app for when users have trouble signing in to their account.
  virtual void ShowAccountAccessHelpApp() = 0;

  // Shows help app for users that have trouble using parent access code.
  virtual void ShowParentAccessHelpApp() = 0;

  // Show the lockscreen notification settings page.
  virtual void ShowLockScreenNotificationSettings() = 0;

  // Called when the keyboard focus is about to leave from the system tray in
  // the login screen / OOBE. |reverse| is true when the focus moves in the
  // reversed direction.
  virtual void OnFocusLeavingSystemTray(bool reverse) = 0;

  // Used by Ash to signal that user activity occurred on the login screen.
  virtual void OnUserActivity() = 0;

 protected:
  virtual ~LoginScreenClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_CLIENT_H_
