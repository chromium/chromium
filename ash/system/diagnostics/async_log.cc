// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/async_log.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {
namespace diagnostics {

namespace {

// Create the log file. Called on the first write to the file.
void CreateFile(const base::FilePath& file_path) {
  DCHECK(!base::PathExists(file_path));

  if (!base::PathExists(file_path.DirName())) {
    const bool create_dir_success = base::CreateDirectory(file_path.DirName());
    if (!create_dir_success) {
      LOG(ERROR) << "Failed to create diagnostics log directory "
                 << file_path.DirName();
      return;
    }
  }

  const bool create_file_success = base::WriteFile(file_path, "");
  if (!create_file_success) {
    LOG(ERROR) << "Failed to create diagnostics log file " << file_path;
  }
}

// Append log to the file. Run on the task runner.
void AppendImpl(const base::FilePath& file_path, const std::string& text) {
  // Ensure file exists.
  if (!base::PathExists(file_path)) {
    CreateFile(file_path);
  }

  // Append text to file.
  base::AppendToFile(file_path, text);
}

}  // namespace

AsyncLog::AsyncLog(const base::FilePath& file_path) : file_path_(file_path) {
  sequenced_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

AsyncLog::~AsyncLog() = default;

void AsyncLog::Append(const std::string& text) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendImpl, file_path_, text));
}

std::string AsyncLog::GetContents() const {
  if (!base::PathExists(file_path_)) {
    return "";
  }

  std::string contents;
  base::ReadFileToString(file_path_, &contents);

  return contents;
}

void AsyncLog::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  sequenced_task_runner_ = std::move(task_runner);
}

}  // namespace diagnostics
}  // namespace ash
