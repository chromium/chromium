// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/time/time.h"

namespace platform_experience {

// Outcome status of executing a delegated task.
// LINT.IfChange(DelegatedTaskStatus)
enum class DelegatedTaskStatus {
  kSuccess = 0,
  kPehNotFound = 1,
  kProcessLaunchFailure = 2,
  kRunnerDestroyedBeforeTaskCompletion = 3,
  kInvalidExitCode = 4,
  kWatchProcessHandleFailure = 5,
  kTaskTimeout = 6,
  kInvalidTaskType = 7,
  kInvalidArgs = 8,
  kPehValidationFailure = 9,
  kUnsupportedVersion = 10,
  kMaxValue = kUnsupportedVersion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:DelegatedTaskStatus)

// LINT.IfChange(DelegatedTaskType)
enum class DelegatedTaskType {
  kRegisterSearchPromotion = 0,
  kMaxValue = kRegisterSearchPromotion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/histograms.xml:PlatformExperienceDelegatedTaskName)

// Standard exit codes returned by PEH.
enum class PehExitCode {
  kInvalidTaskType = 1,
  kInvalidArgs = 2,
};

std::optional<DelegatedTaskType> ParseDelegatedTaskType(
    std::string_view task_name);

// Interface/Base class defining a delegated logic task. Concrete
// implementations define the task type, timeout, and exit code interpretation.
class DelegatedTask {
 public:
  virtual ~DelegatedTask() = default;

  // The delegated task type to be executed.
  virtual DelegatedTaskType GetTaskType() const = 0;

  // Returns the task name.
  virtual std::string_view GetTaskName() const = 0;

  // Optionally appends task-specific command line switches. Defaults to no-op.
  virtual void AppendCommandLineSwitches(base::CommandLine& cmd_line) const;

  // The execution timeout for this task. Defaults to 10 seconds.
  virtual base::TimeDelta GetTimeout() const;
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_H_
