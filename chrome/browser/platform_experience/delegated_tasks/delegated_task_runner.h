// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "base/win/object_watcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"

namespace platform_experience {

class DelegatedTaskRunner;
class PehLauncher;

using DelegatedTaskExitCodeOrStatus = base::expected<int, DelegatedTaskStatus>;

struct DelegatedTaskResult {
  DelegatedTaskExitCodeOrStatus exit_code_or_status;
  base::TimeDelta execution_time;
};

using DelegatedTaskCompletionCallback =
    base::OnceCallback<void(DelegatedTaskResult)>;

// `DelegatedTaskRunner` handles executing tasks, waiting for task
// completion/timeout and UMA telemetry.
// The runner is implemented to run one task in its lifecycle.
class DelegatedTaskRunner : public base::win::ObjectWatcher::Delegate {
 public:
  // Creates the `DelegatedTaskRunner` with the default `PehLauncher` instance.
  DelegatedTaskRunner();

  // Creates the `DelegatedTaskRunner` with custom `PehLauncher` instance.
  // Useful for injecting mock launcher in tests.
  explicit DelegatedTaskRunner(std::unique_ptr<PehLauncher> peh_launcher);

  ~DelegatedTaskRunner() override;

  DelegatedTaskRunner(const DelegatedTaskRunner&) = delete;
  DelegatedTaskRunner& operator=(const DelegatedTaskRunner&) = delete;

  // Runs the provided task and asynchronously returns the task completion
  // result in the `callback`.
  // `min_version` enforces a minimum binary version requirement for PEH. It
  // is mandatory and must be a valid version string (e.g. "152.0.0.0").
  // An invalid version string will cause the task to fail immediately.
  virtual void Run(std::unique_ptr<DelegatedTask> task,
                   std::string_view min_version,
                   DelegatedTaskCompletionCallback callback);

 private:
  void OnProcessLaunched(base::Process process);
  void OnBinaryPathRetrieved(const base::FilePath& peh_binary_path);
  void OnBinaryVerificationComplete(const base::FilePath& peh_binary_path,
                                    bool is_verified);
  void OnBinaryVersionRetrieved(const base::FilePath& peh_binary_path,
                                const base::Version& version);

  void CleanupAndReturnResult(
      DelegatedTaskExitCodeOrStatus exit_code_or_status);

  // base::win::ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override;

  base::TimeTicks task_start_time_;
  base::Process process_;
  base::win::ObjectWatcher watcher_;
  DelegatedTaskCompletionCallback completion_callback_;
  base::Version min_version_;

  std::unique_ptr<DelegatedTask> task_;
  base::SequenceBound<std::unique_ptr<PehLauncher>> peh_launcher_;

  base::WeakPtrFactory<DelegatedTaskRunner> weak_factory_{this};
};

}  // namespace platform_experience

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_DELEGATED_TASKS_DELEGATED_TASK_RUNNER_H_
