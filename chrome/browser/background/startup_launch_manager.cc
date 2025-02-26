// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/startup_launch_manager.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/auto_launch_util.h"

namespace {
StartupLaunchManager* g_instance_for_testing = nullptr;
}

// static
StartupLaunchManager* StartupLaunchManager::GetInstance() {
  static base::NoDestructor<StartupLaunchManager> instance;
  return g_instance_for_testing ? g_instance_for_testing : instance.get();
}

// static
void StartupLaunchManager::SetInstanceForTesting(
    StartupLaunchManager* manager) {
  g_instance_for_testing = manager;
}

void StartupLaunchManager::RegisterLaunchOnStartup(StartupLaunchReason reason) {
  const bool was_empty = registered_launch_reasons_.empty();
  registered_launch_reasons_.insert(reason);
  if (was_empty) {
    UpdateLaunchOnStartup(true);
  }
}

void StartupLaunchManager::UnregisterLaunchOnStartup(
    StartupLaunchReason reason) {
  registered_launch_reasons_.erase(reason);
  if (registered_launch_reasons_.empty()) {
    UpdateLaunchOnStartup(false);
  }
}

StartupLaunchManager::StartupLaunchManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

StartupLaunchManager::~StartupLaunchManager() {
  if (this == g_instance_for_testing) {
    g_instance_for_testing = nullptr;
  }
}

void StartupLaunchManager::UpdateLaunchOnStartup(
    bool should_launch_on_startup) {
#if BUILDFLAG(IS_WIN)
  // This functionality is only defined for default profile, currently.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUserDataDir)) {
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      should_launch_on_startup
          ? base::BindOnce(auto_launch_util::EnableBackgroundStartAtLogin)
          : base::BindOnce(auto_launch_util::DisableBackgroundStartAtLogin));
#elif BUILDFLAG(IS_MAC)
// Mac does not support forcing launch on startup.
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_WIN)
}
