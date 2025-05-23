// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_

#include <string>

#include "base/observer_list_types.h"

namespace content {
class WebContents;
}

// This class is responsible for controlling password change process.
class PasswordChangeDelegate {
 public:
  // Internal state of a password change flow. Corresponds to
  // `PasswordChangeFlowState` in enums.xml. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class State {
    // Password change is being offered to the user, waiting from the to accept
    // or reject it.
    kOfferingPasswordChange = 0,

    // Waiting for the user to accept privacy notice.
    kWaitingForAgreement = 1,

    // Delegate is waiting for change password form to appear.
    kWaitingForChangePasswordForm = 2,

    // Change password form wasn't found.
    kChangePasswordFormNotFound = 3,

    // Change password form is detected. Generating and filling password fields.
    // Delegate waits for submission confirmation.
    kChangingPassword = 4,

    // Password is successfully updated.
    kPasswordSuccessfullyChanged = 5,

    // Password change failed.
    kPasswordChangeFailed = 6,

    // One time password (OTP) was detected on a page. The flow is stopped, user
    // input is required.
    kOtpDetected = 7,

    kMaxValue = kOtpDetected,
  };

  // An interface used to notify clients (observers) of delegate state. Register
  // the observer via `PasswordChangeDelegate::AddObserver`.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies listeners about new state.
    virtual void OnStateChanged(State new_state) {}

    // Invoked before `delegate` is destroyed. Should be used to stop observing.
    virtual void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) {}
  };

  virtual ~PasswordChangeDelegate() = default;

  // Starts the password change flow (including showing the privacy notice
  // agreement if necessary).
  virtual void StartPasswordChangeFlow() = 0;

  // Responds whether password change is ongoing for a given |web_contents|.
  // This is true both for originator and a tab where password change is
  // performed.
  virtual bool IsPasswordChangeOngoing(content::WebContents* web_contents) = 0;

  // Returns current state.
  virtual State GetCurrentState() const = 0;

  // Terminates password change operation immediately. Delegate shouldn't be
  // invoked after this function is called as the object will soon be destroyed.
  virtual void Stop() = 0;

  // Restarts password change flow only if the flow failed due to inability to
  // find change password form. In all other scenarios it's unsafe to restart.
  virtual void Restart() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Brings a tab where password change is ongoing. Does nothing if the tab
  // doesn't exist anymore.
  virtual void OpenPasswordChangeTab() = 0;
#endif
  // To be executed after a password form was submitted
  virtual void OnPasswordFormSubmission(content::WebContents* web_contents) = 0;

  virtual void OnPrivacyNoticeAccepted() = 0;

  virtual void OnOtpFieldDetected(content::WebContents* web_contents) = 0;

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Getters for current domain where password change is ongoing, username and a
  // newly generated password. Password exists only after it was generated.
  virtual std::u16string GetDisplayOrigin() const = 0;
  virtual const std::u16string& GetUsername() const = 0;
  virtual const std::u16string& GetGeneratedPassword() const = 0;

  virtual base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
