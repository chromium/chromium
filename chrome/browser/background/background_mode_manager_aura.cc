// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_manager.h"

#include "base/task/sequenced_task_runner.h"

// No background jobs for aura for now.

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  NOTIMPLEMENTED();
}

void BackgroundModeManager::DisplayClientInstalledNotification(
    const std::u16string& name) {
  NOTIMPLEMENTED();
}

// static
scoped_refptr<base::SequencedTaskRunner>
BackgroundModeManager::CreateTaskRunner() {
  return nullptr;
}
