// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_TASK_EXECUTOR_H_
#define CHROME_BROWSER_RECORD_REPLAY_TASK_EXECUTOR_H_

#include <string>

#include "base/containers/flat_map.h"

class Profile;
class BrowserWindowInterface;

namespace record_replay {

class TaskDefinition;

using TaskParameterValues =
    base::flat_map<int, base::flat_map<std::string, std::string>>;

class TaskExecutor {
 public:
  TaskExecutor() = delete;

  // Executes a task by formatting the task definition and parameters into
  // a prompt and passing it to Glic for the given profile and browser window.
  static void ExecuteTask(Profile* profile,
                          BrowserWindowInterface* browser_window,
                          const TaskDefinition& definition,
                          const TaskParameterValues& parameter_values);
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_TASK_EXECUTOR_H_
