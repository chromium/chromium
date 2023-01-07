// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/task_logger.h"

namespace base {
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class SyncTaskManager;
struct TaskBlocker;

// Represents a running sequence of SyncTasks.  Owned by a callback chain that
// should run exclusively, and held by SyncTaskManager when no task is running.
class SyncTaskToken {
 public:
  static const int64_t kTestingTaskTokenID;
  static const int64_t kForegroundTaskTokenID;
  static const int64_t kMinimumBackgroundTaskTokenID;

  static std::unique_ptr<SyncTaskToken> CreateForTesting(
      SyncStatusCallback callback);
  static std::unique_ptr<SyncTaskToken> CreateForForegroundTask(
      const base::WeakPtr<SyncTaskManager>& manager,
      base::SequencedTaskRunner* task_runner);
  static std::unique_ptr<SyncTaskToken> CreateForBackgroundTask(
      const base::WeakPtr<SyncTaskManager>& manager,
      base::SequencedTaskRunner* task_runner,
      int64_t token_id,
      std::unique_ptr<TaskBlocker> task_blocker);

  void UpdateTask(const base::Location& location, SyncStatusCallback callback);

  const base::Location& location() const { return location_; }

  SyncTaskToken(const SyncTaskToken&) = delete;
  SyncTaskToken& operator=(const SyncTaskToken&) = delete;

  virtual ~SyncTaskToken();

  static SyncStatusCallback WrapToCallback(
      std::unique_ptr<SyncTaskToken> token);

  SyncTaskManager* manager() { return manager_.get(); }

  SyncStatusCallback take_callback() { return std::move(callback_); }

  void set_task_blocker(std::unique_ptr<TaskBlocker> task_blocker);
  const TaskBlocker* task_blocker() const;
  void clear_task_blocker();

  int64_t token_id() const { return token_id_; }

  void InitializeTaskLog(const std::string& task_description);
  void FinalizeTaskLog(const std::string& result_description);
  void RecordLog(const std::string& message);

  bool has_task_log() const { return !!task_log_; }
  void SetTaskLog(std::unique_ptr<TaskLogger::TaskLog> task_log);
  std::unique_ptr<TaskLogger::TaskLog> PassTaskLog();

 private:
  SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager,
                const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                int64_t token_id,
                std::unique_ptr<TaskBlocker> task_blocker,
                SyncStatusCallback callback);

  base::WeakPtr<SyncTaskManager> manager_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::Location location_;
  int64_t token_id_;
  SyncStatusCallback callback_;

  std::unique_ptr<TaskLogger::TaskLog> task_log_;
  std::unique_ptr<TaskBlocker> task_blocker_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_
