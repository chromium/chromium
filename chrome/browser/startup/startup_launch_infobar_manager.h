// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_

class StartupLaunchInfoBarManager {
 public:
  enum class InfoBarType {
    kForegroundOptIn,
    kForegroundOptOut,
  };

  virtual ~StartupLaunchInfoBarManager() = default;

  virtual void ShowInfoBars(InfoBarType infobar_type) = 0;
  virtual void CloseAllInfoBars() = 0;
};

#endif  // CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_
