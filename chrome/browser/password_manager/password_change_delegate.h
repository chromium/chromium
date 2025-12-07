// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_

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
  // LINT.IfChange(State)
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

    // Password change was canceled by the user.
    kCanceled = 8,

    // The initial state before any UI is displayed. Transitions automatically
    // into kOfferingPasswordChange or kWaitingForAgreement after no OTP is
    // present on a main page.
    kNoState = 9,

    // Login form was detected on a page during an ongoing password change flow.
    // The flow is not stopped, but the user action is required.
    kLoginFormDetected = 10,

    // Deprecated: kLoginFormDetectedUserCanContinue = 11,

    kMaxValue = kLoginFormDetected,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordChangeFlowState)

  // Password change flow state used for UMA and UKM. Corresponds to
  // `CoarseFinalPasswordChangeState` in enums.xml.
  //
  // These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  // LINT.IfChange(CoarseFinalPasswordChangeState)
  enum class CoarseFinalPasswordChangeState {
    // Password change is being offered to the user, waiting for the user to
    // accept or reject it or waiting for the user to accept privacy notice.
    kOffered = 0,

    // Password change was canceled by the user.
    kCanceled = 1,

    // Password is successfully updated.
    kSuccessful = 2,

    // Password change failed.
    kFailed = 3,

    // Change password form wasn't found.
    kFormNotDetected = 4,

    // One time password (OTP) was detected on a page. The flow is stopped, user
    // input is required.
    kOtpDetected = 5,

    kMaxValue = kOtpDetected,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:CoarseFinalPasswordChangeState)

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

  // Starts performing password change by looking for a change password form in
  // a hidden tab.
  virtual void StartPasswordChangeFlow() = 0;

  // Cancels any password change operation.
  virtual void CancelPasswordChangeFlow() = 0;

  // Responds whether password change is ongoing for a given `web_contents`.
  // This is true both for originator and a tab where password change is
  // performed.
  virtual bool IsPasswordChangeOngoing(content::WebContents* web_contents) = 0;

  // Returns current state.
  virtual State GetCurrentState() const = 0;

  // Terminates password change operation immediately. Delegate shouldn't be
  // invoked after this function is called as the object will soon be destroyed.
  virtual void Stop() = 0;

  // Brings a tab where password change is ongoing. Does nothing if the tab
  // doesn't exist anymore.
  virtual void OpenPasswordChangeTab() = 0;

  // Displays password change confirmation bubble. If the user navigated away
  // from the page, then navigates to password details in password settings.
  virtual void OpenPasswordDetails() = 0;

  // To be executed after a password form was submitted
  virtual void OnPasswordFormSubmission(content::WebContents* web_contents) = 0;

  virtual void OnPrivacyNoticeAccepted() = 0;

  // Called when the user declines the initial dialog offering password change.
  virtual void OnPasswordChangeDeclined() = 0;

  // Called when the user chooses to retry the login check (by clicking
  // 'Retry' on the toast).
  virtual void RetryLoginCheck() = 0;

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
