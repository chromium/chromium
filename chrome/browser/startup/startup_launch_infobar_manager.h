// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_

#include "base/observer_list_types.h"

class StartupLaunchInfoBarManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnInfoBarDismissed() = 0;
  };

  // Infobar types that can be shown to users.
  //
  // LINT.IfChange(InfoBarType)
  enum class InfoBarType {
    kForegroundOptIn,
    kForegroundOptOut,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/startup/histograms.xml:StartupLaunchInfoBarType)

  virtual ~StartupLaunchInfoBarManager() = default;

  virtual void ShowInfoBars(InfoBarType infobar_type) = 0;
  virtual void CloseAllInfoBars() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_H_
