// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_MOCK_WEB_CONTENTS_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_MOCK_WEB_CONTENTS_TASK_MANAGER_H_

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"

namespace task_manager {

// Defines a test class that will act as a task manager that is designed to
// only observe the WebContents-based tasks.
class MockWebContentsTaskManager : public TaskProviderObserver {
 public:
  MockWebContentsTaskManager();
  MockWebContentsTaskManager(const MockWebContentsTaskManager&) = delete;
  MockWebContentsTaskManager& operator=(const MockWebContentsTaskManager&) =
      delete;
  ~MockWebContentsTaskManager() override;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;

  // Start / Stop observing the WebContentsTaskProvider.
  void StartObserving();
  void StopObserving();

  const std::vector<raw_ptr<Task, VectorExperimental>>& tasks() const {
    return tasks_;
  }

  // Helper to get the titles of all `tasks_`. Can be used with
  // `EXPECT_THAT(TaskTitles(), ElementsAre(...))`.
  std::vector<std::string> TaskTitles() {
    return MockWebContentsTaskManager::TaskTitles(tasks_);
  }

  static std::vector<std::string> TaskTitles(
      std::vector<raw_ptr<Task, VectorExperimental>> tasks) {
    std::vector<std::string> task_titles;
    std::ranges::transform(
        tasks, std::back_inserter(task_titles),
        [&](const auto& task) { return base::UTF16ToUTF8(task->title()); });
    return task_titles;
  }

 private:
  std::vector<raw_ptr<Task, VectorExperimental>> tasks_;
  WebContentsTaskProvider provider_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_MOCK_WEB_CONTENTS_TASK_MANAGER_H_
