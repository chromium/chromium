// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_STARTUP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_STARTUP_LAUNCH_MANAGER_H_

#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"

// Reason why a Chrome should be launched on startup.
enum class StartupLaunchReason { kExtensions, kGlic };

// StartupLaunchManager registers with the OS so that Chrome
// launches in the background on device startup when there is at least one
// reason why Chrome should launch on startup.
class StartupLaunchManager {
 public:
  static StartupLaunchManager* GetInstance();

  static void SetInstanceForTesting(StartupLaunchManager* manager);

  void RegisterLaunchOnStartup(StartupLaunchReason reason);
  void UnregisterLaunchOnStartup(StartupLaunchReason reason);

 protected:
  StartupLaunchManager();
  virtual ~StartupLaunchManager();

  virtual void UpdateLaunchOnStartup(bool should_launch_on_startup);

 private:
  friend class base::NoDestructor<StartupLaunchManager>;

  // Task runner for making startup/login configuration changes that may
  // require file system or registry access.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::set<StartupLaunchReason> registered_launch_reasons_;
};

#endif  // CHROME_BROWSER_BACKGROUND_STARTUP_LAUNCH_MANAGER_H_
