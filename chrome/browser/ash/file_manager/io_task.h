// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager {

namespace io_task {

enum class State {
  // Task has been queued, but not yet started.
  kQueued,

  // Task is currently running.
  kInProgress,

  // Task has been successfully completed.
  kSuccess,

  // Task has completed with errors.
  kError,

  // Task has been canceled without finishing.
  kCancelled,
};

enum class OperationType {
  kCopy,
  kMove,
  kDelete,
  kZip,
};

// Represents the current progress of an I/O task.
struct ProgressStatus {
  // Out-of-line constructors to appease the style linter.
  ProgressStatus();
  ProgressStatus(const ProgressStatus& other) = delete;
  ProgressStatus& operator=(const ProgressStatus& other) = delete;
  ~ProgressStatus();

  // Allow ProgressStatus to be moved.
  ProgressStatus(ProgressStatus&& other);
  ProgressStatus& operator=(ProgressStatus&& other);

  // Task state.
  State state;

  // I/O Operation type (e.g. copy, move).
  OperationType type;

  // Files the operation processes.
  std::vector<storage::FileSystemURL> source_urls;

  // One error per source_url. Absence of value indicates file has not been
  // processed yet.
  std::vector<absl::optional<base::File::Error>> errors;

  // Optional destination folder for operations that transfer files to a
  // directory (e.g. copy or move).
  storage::FileSystemURL destination_folder;

  // ProgressStatus over all |source_urls|.
  int64_t bytes_transferred;

  // Total size of all |source_urls|.
  int64_t total_bytes;
};

// An IOTask represents an I/O operation over multiple files, and is responsible
// for executing the operation and providing progress/completion reports.
class IOTask {
 public:
  virtual ~IOTask() = default;

  using ProgressCallback = base::RepeatingCallback<void(const ProgressStatus&)>;
  using CompleteCallback = base::OnceCallback<void(ProgressStatus)>;

  // Executes the task. |progress_callback| should be called every so often to
  // give updates, and |complete_callback| should be only called once at the end
  // to signify completion with a |kSuccess|, |kError| or |kCancelled| state.
  // |progress_callback| should be called on the same sequeuence Execute() was.
  virtual void Execute(ProgressCallback progress_callback,
                       CompleteCallback complete_callback) = 0;

  // Cancels the task. This should set the progress state to be |kCancelled|,
  // but not call any of Execute()'s callbacks. The task will be deleted
  // synchronously after this call returns.
  virtual void Cancel() = 0;

  // Gets the current progress status of the task.
  virtual const ProgressStatus& progress() = 0;

 protected:
  IOTask() = default;
  IOTask(const IOTask& other) = delete;
  IOTask& operator=(const IOTask& other) = delete;
};

// No-op IO Task for testing.
// TODO(austinct): Move into io_task_controller_unittest file when the other
// IOTasks have been implemented.
class DummyIOTask : public IOTask {
 public:
  DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
              storage::FileSystemURL destination_folder,
              OperationType type);
  ~DummyIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

  const ProgressStatus& progress() override;

 private:
  void DoProgress();
  void DoComplete();

  ProgressStatus progress_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<DummyIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
