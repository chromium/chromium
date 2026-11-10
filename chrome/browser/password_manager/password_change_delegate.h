// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"

namespace content {
class WebContents;
}

// This class is responsible for controlling password change process.
class PasswordChangeDelegate {
 public:
  // An interface used to notify clients (observers) of delegate lifecycle and
  // fallback events. Register the observer via
  // `PasswordChangeDelegate::AddObserver`.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked before `delegate` is destroyed. Should be used to stop observing.
    virtual void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) {}

    // Invoked when password change finishes successfully.
    virtual void OnPasswordChangeFinishedSuccessfully(
        PasswordChangeDelegate* delegate) {}

    // Invoked when login check fails with a server error.
    virtual void OnLoginCheckFailedWithServerError(
        PasswordChangeDelegate* delegate) {}
  };

  virtual ~PasswordChangeDelegate() = default;

  // Responds whether password change is ongoing for a given `web_contents`.
  // This is true both for originator and a tab where password change is
  // performed.
  virtual bool IsPasswordChangeOngoing(content::WebContents* web_contents) = 0;

  // Terminates password change operation immediately. Delegate shouldn't be
  // invoked after this function is called as the object will soon be destroyed.
  virtual void Stop() = 0;

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
