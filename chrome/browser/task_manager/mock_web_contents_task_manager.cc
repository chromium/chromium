// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"

#include "base/containers/contains.h"
#include "build/build_config.h"

namespace task_manager {

MockWebContentsTaskManager::MockWebContentsTaskManager() = default;

MockWebContentsTaskManager::~MockWebContentsTaskManager() = default;

void MockWebContentsTaskManager::TaskAdded(Task* task) {
  DCHECK(task);
  DCHECK(!base::Contains(tasks_, task));
  tasks_.push_back(task);
}

void MockWebContentsTaskManager::TaskRemoved(Task* task) {
  DCHECK(task);
  DCHECK(base::Contains(tasks_, task));
  tasks_.erase(std::find(tasks_.begin(), tasks_.end(), task));
}

void MockWebContentsTaskManager::StartObserving() {
  provider_.SetObserver(this);
}

void MockWebContentsTaskManager::StopObserving() {
  provider_.ClearObserver();
}

}  // namespace task_manager
