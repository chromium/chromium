// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"

#include "base/stl_util.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#endif  // defined(OS_CHROMEOS)

namespace task_manager {

MockWebContentsTaskManager::MockWebContentsTaskManager() {}

MockWebContentsTaskManager::~MockWebContentsTaskManager() {}

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
#if defined(OS_CHROMEOS)
  // On ChromeOS, the ResourceReporter needs to be turned off so as not to
  // interfere with the tests.
  chromeos::ResourceReporter::GetInstance()->StopMonitoring();
#endif  // defined(OS_CHROMEOS)

  provider_.SetObserver(this);
}

void MockWebContentsTaskManager::StopObserving() {
  provider_.ClearObserver();
}

}  // namespace task_manager
