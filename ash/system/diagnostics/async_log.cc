// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/async_log.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {
namespace diagnostics {

AsyncLog::AsyncLog(const base::FilePath& file_path) : file_path_(file_path) {
  sequenced_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  DETACH_FROM_SEQUENCE(async_log_checker_);
}

AsyncLog::~AsyncLog() = default;

void AsyncLog::Append(const std::string& text) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AsyncLog::AppendImpl, weak_factory_.GetWeakPtr(), text));
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

void AsyncLog::AppendImpl(const std::string& text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(async_log_checker_);
  // Ensure file exists.
  if (!base::PathExists(file_path_)) {
    CreateFile();
  }

  // Append text to file.
  base::AppendToFile(file_path_, text);
}

void AsyncLog::CreateFile() {
  DCHECK(!base::PathExists(file_path_));

  if (!base::PathExists(file_path_.DirName())) {
    const bool create_dir_success = base::CreateDirectory(file_path_.DirName());
    if (!create_dir_success) {
      LOG(ERROR) << "Failed to create diagnostics log directory "
                 << file_path_.DirName();
      return;
    }
  }

  const bool create_file_success = base::WriteFile(file_path_, "");
  if (!create_file_success) {
    LOG(ERROR) << "Failed to create diagnostics log file " << file_path_;
  }
}

}  // namespace diagnostics
}  // namespace ash
