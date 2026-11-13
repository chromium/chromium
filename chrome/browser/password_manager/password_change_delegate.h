// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

// This class is responsible for controlling password change process.
class PasswordChangeDelegate {
 public:
  virtual ~PasswordChangeDelegate() = default;

  // Responds whether password change is ongoing for a given `web_contents`.
  // This is true both for originator and a tab where password change is
  // performed.
  virtual bool IsPasswordChangeOngoing(content::WebContents* web_contents) = 0;

  // Terminates password change operation immediately. Delegate shouldn't be
  // invoked after this function is called as the object will soon be destroyed.
  virtual void Stop() = 0;

  virtual base::WeakPtr<PasswordChangeDelegate> AsWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DELEGATE_H_
