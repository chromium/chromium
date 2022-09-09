// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_TERMINATION_MANAGER_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_TERMINATION_MANAGER_H_

namespace apps {

// This class listens for when Chrome attempts to quit while apps are running.
// If Chrome's quit attempt is cancelled because an app was running, then this
// class will make Chrome quit once that app quits (unless a new Chrome window
// has opened).
class AppShimTerminationManager {
 public:
  static AppShimTerminationManager* Get();

  // Terminate Chrome if a browser window has never been opened, there are no
  // shell windows, and the app list is not visible.
  virtual void MaybeTerminate() = 0;

  // Whether browser sessions should be restored right now. This is true if
  // the browser has been quit but kept alive because Chrome Apps are still
  // running.
  virtual bool ShouldRestoreSession() = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_TERMINATION_MANAGER_H_
