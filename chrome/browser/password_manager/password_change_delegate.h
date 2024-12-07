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
  // Internal state of a password change flow.
  enum class State {
    // Delegate is waiting for change password form to appear.
    kWaitingForChangePasswordForm,

    // Change password form is detected. Generating and filling password fields.
    // Delegate waits for submission confirmation.
    kChangingPassword,

    // Password is successfully updated.
    kPasswordSuccessfullyChanged,

    // Password change failed.
    kPasswordChangeFailed,
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

  // Responds whether password change is ongoing for a given |web_contents|.
  // This is true both for originator and a tab where password change is
  // performed.
  virtual bool IsPasswordChangeOngoing(content::WebContents* web_contents) = 0;

  // Returns current state.
  virtual State GetCurrentState() const = 0;

  // Terminates password change operation immediately. Delegate shouldn't be
  // invoked after this function is called as the object will soon be destroyed.
  virtual void Stop() = 0;

  // Informs delegate about successful form submission.
  virtual void SuccessfulSubmissionDetected(
      content::WebContents* web_contents) = 0;

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
