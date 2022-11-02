// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // The Mac does not support forcing a launch on startup.
}

void BackgroundModeManager::DisplayClientInstalledNotification(
    const std::u16string& name) {
  // TODO(http://crbug.com/74970): Display a platform-appropriate notification
  // here.
}

scoped_refptr<base::SequencedTaskRunner>
BackgroundModeManager::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}
