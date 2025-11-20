// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_

#include <optional>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/installer/util/auto_launch_util.h"

// Reason why a Chrome should be launched on startup.
enum class StartupLaunchReason { kExtensions, kGlic };

// StartupLaunchManager registers with the OS so that Chrome
// launches on device startup depending on the the reason why Chrome should
// launch on startup.
class StartupLaunchManager {
 public:
  static StartupLaunchManager* GetInstance();

  static void SetInstanceForTesting(StartupLaunchManager* manager);

  void RegisterLaunchOnStartup(StartupLaunchReason reason);
  void UnregisterLaunchOnStartup(StartupLaunchReason reason);

 protected:
  StartupLaunchManager();
  virtual ~StartupLaunchManager();

  virtual void UpdateLaunchOnStartup(
      std::optional<auto_launch_util::StartupLaunchMode> startup_launch_mode);

 private:
  friend class base::NoDestructor<StartupLaunchManager>;

  // Returns `std::nullopt` if startup launch should be disabled.
  std::optional<auto_launch_util::StartupLaunchMode> GetStartupLaunchMode()
      const;

  // Task runner for making startup/login configuration changes that may
  // require file system or registry access.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::set<StartupLaunchReason> registered_launch_reasons_;
};

#endif  // CHROME_BROWSER_STARTUP_STARTUP_LAUNCH_MANAGER_H_
