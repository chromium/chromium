// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_

#include <memory>
#include <string>

#include "base/observer_list_types.h"
#include "chrome/browser/password_manager/password_change_delegate.h"

namespace content {
class WebContents;
}

// Interface for performing the actual password change actuation on a website.
// This decouples the UI layer (PasswordChangeDelegateImpl) from the specific
// actuation mechanism (DOM/Script-based vs Glic/Actor-based).
class PasswordChangeActuator {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that the actuation state has changed.
    virtual void OnActuationStateChanged(
        PasswordChangeDelegate::State new_state) = 0;
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

  // Returns the password generated during the password change flow, if any.
  virtual std::u16string GetGeneratedPassword() const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_ACTUATOR_H_
