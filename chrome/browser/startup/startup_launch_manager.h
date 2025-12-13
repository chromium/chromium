// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_

#include <optional>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserProcess;

// Reason why a Chrome should be launched on startup.
enum class StartupLaunchReason { kExtensions, kGlic };

// StartupLaunchManager registers with the OS so that Chrome
// launches on device startup depending on the the reason why Chrome should
// launch on startup.
class StartupLaunchManager {
 public:
  explicit StartupLaunchManager(BrowserProcess* browser_process);
  virtual ~StartupLaunchManager();

  DECLARE_USER_DATA(StartupLaunchManager);

  static StartupLaunchManager* From(BrowserProcess* browser_process);

  void RegisterLaunchOnStartup(StartupLaunchReason reason);
  void UnregisterLaunchOnStartup(StartupLaunchReason reason);

 private:
  virtual void UpdateLaunchOnStartup(
      std::optional<auto_launch_util::StartupLaunchMode> startup_launch_mode);

  // Returns `std::nullopt` if startup launch should be disabled.
  std::optional<auto_launch_util::StartupLaunchMode> GetStartupLaunchMode()
      const;

  // Task runner for making startup/login configuration changes that may
  // require file system or registry access.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::set<StartupLaunchReason> registered_launch_reasons_;

  ui::ScopedUnownedUserData<StartupLaunchManager> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
