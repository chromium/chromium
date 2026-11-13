// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_

#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_change/password_change_credentials_provider.h"
#include "components/password_manager/core/browser/password_string.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFormManager;
}

// Interface for performing the actual password change actuation on a website.
// This decouples the UI layer (PasswordChangeDelegateImpl) from the specific
// actuation mechanism (DOM/Script-based vs Glic/Actor-based).
class PasswordChangeActuator {
 public:
  using NewPasswordResult = base::expected<
      password_manager::PasswordString,
      password_manager::PasswordChangeCredentialsProvider::Error>;

  enum class State {
    // Actuator is waiting for change password form to appear.
    kWaitingForChangePasswordForm,

    // Change password form wasn't found.
    kChangePasswordFormNotFound,

    // Change password form is detected. Generating and filling password fields.
    // Actuator waits for submission confirmation.
    kChangingPassword,

    // Password is successfully updated.
    kPasswordSuccessfullyChanged,

    // Password change failed.
    kPasswordChangeFailed,

    // One time password (OTP) was detected on a page. User input is required.
    kOtpDetected,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that the actuation state has changed.
    virtual void OnActuationStateChanged(State new_state) = 0;
  };

  virtual ~PasswordChangeActuator() = default;

  // Starts the password change actuation flow.
  virtual void Start() = 0;

  // Cancels the password change actuation flow (e.g. triggered by user cancel).
  virtual void Cancel() = 0;

  // Returns the WebContents where the password change is being executed.
  // Can be a hidden WebContents or a foreground tab.
  virtual content::WebContents* GetExecutorWebContents() const = 0;

  // Brings the tab/WebContents where password change is occurring to the
  // foreground or opens it in the browser tab strip.
  virtual void OpenPasswordChangeTab(content::WebContents* originator) = 0;

  // Returns the new password if it was already generated during the password
  // change flow. Otherwise, if `form_manager` is not null, generates a new
  // password for the change password form observed by `form_manager`,
  // respecting its crowdsourced password requirements, and saves it as a
  // backup of the credential being changed. Returns `Error::kGenerationFailed`
  // if no password was or could be generated; the returned password is never
  // empty.
  virtual NewPasswordResult GetOrGenerateNewPassword(
      password_manager::PasswordFormManager* form_manager) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_
