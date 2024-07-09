// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
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
  const auto it = base::ranges::find(tasks_, task);
  CHECK(it != tasks_.end(), base::NotFatalUntil::M130);
  tasks_.erase(it);
}

void MockWebContentsTaskManager::StartObserving() {
  provider_.SetObserver(this);
}

void MockWebContentsTaskManager::StopObserving() {
  provider_.ClearObserver();
}

}  // namespace task_manager
