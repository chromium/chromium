// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_DELEGATED_TASK_RUNNER_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_DELEGATED_TASK_RUNNER_H_

#include <memory>
#include <string_view>

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace platform_experience {

class MockDelegatedTaskRunner : public DelegatedTaskRunner {
 public:
  MockDelegatedTaskRunner();
  ~MockDelegatedTaskRunner() override;

  MOCK_METHOD(void,
              Run,
              (std::unique_ptr<DelegatedTask> task,
               std::string_view min_version,
               DelegatedTaskCompletionCallback callback),
              (override));
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_TEST_SUPPORT_MOCK_DELEGATED_TASK_RUNNER_H_
