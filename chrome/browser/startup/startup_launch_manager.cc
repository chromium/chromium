// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup/startup_launch_manager.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"

using auto_launch_util::StartupLaunchMode;

std::optional<StartupLaunchMode> StartupLaunchManager::GetStartupLaunchMode()
    const {
  if (registered_launch_reasons_.empty()) {
    return std::nullopt;
  }
  return StartupLaunchMode::kBackground;
}

void StartupLaunchManager::RegisterLaunchOnStartup(StartupLaunchReason reason) {
  const auto previous_startup_mode = GetStartupLaunchMode();
  registered_launch_reasons_.insert(reason);
  const auto current_startup_mode = GetStartupLaunchMode();

  if (previous_startup_mode != current_startup_mode) {
    UpdateLaunchOnStartup(current_startup_mode);
  }
}

void StartupLaunchManager::UnregisterLaunchOnStartup(
    StartupLaunchReason reason) {
  const auto previous_startup_mode = GetStartupLaunchMode();
  registered_launch_reasons_.erase(reason);
  const auto current_startup_mode = GetStartupLaunchMode();

  if (!current_startup_mode.has_value() ||
      previous_startup_mode != current_startup_mode) {
    UpdateLaunchOnStartup(current_startup_mode);
  }
}

DEFINE_USER_DATA(StartupLaunchManager);

StartupLaunchManager::StartupLaunchManager(BrowserProcess* browser_process)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {}

StartupLaunchManager::~StartupLaunchManager() = default;

// static
StartupLaunchManager* StartupLaunchManager::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

void StartupLaunchManager::UpdateLaunchOnStartup(
    std::optional<StartupLaunchMode> startup_launch_mode) {
#if BUILDFLAG(IS_WIN)
  // This functionality is only defined for default profile, currently.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUserDataDir)) {
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, startup_launch_mode.has_value()
                     ? base::BindOnce(auto_launch_util::EnableStartAtLogin,
                                      *startup_launch_mode)
                     : base::BindOnce(auto_launch_util::DisableStartAtLogin));
#elif BUILDFLAG(IS_MAC)
// Mac does not support forcing launch on startup.
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_WIN)
}
