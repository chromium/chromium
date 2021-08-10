// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer_cleaner.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

namespace {

base::Time GetUpperBoundTime() {
#if defined(OS_ANDROID) || defined(OS_IOS) || defined(OS_FUCHSIA)
  // If process creation time is not available then use instance creation
  // time as the upper-bound for old files. Modification times may be
  // rounded-down to coarse-grained increments, e.g. FAT has 2s granularity,
  // so it is necessary to set the upper-bound earlier than Now() by at least
  // that margin to account for modification times being rounded-down.
  return Time::Now() - TimeDelta::FromSeconds(2);
#else
  return Process::Current().CreationTime() - TimeDelta::FromSeconds(2);
#endif
}

}  // namespace

// static
ImportantFileWriterCleaner& ImportantFileWriterCleaner::GetInstance() {
  static NoDestructor<ImportantFileWriterCleaner> instance;
  return *instance;
}

// static
void ImportantFileWriterCleaner::AddDirectory(const FilePath& directory) {
  auto& instance = GetInstance();
  scoped_refptr<SequencedTaskRunner> task_runner;
  {
    AutoLock scoped_lock(instance.task_runner_lock_);
    task_runner = instance.task_runner_;
  }
  if (!task_runner)
    return;
  if (task_runner->RunsTasksInCurrentSequence()) {
    instance.AddDirectoryImpl(directory);
  } else {
    // Unretained is safe here since the cleaner instance is never destroyed.
    task_runner->PostTask(
        FROM_HERE, BindOnce(&ImportantFileWriterCleaner::AddDirectoryImpl,
                            Unretained(&instance), directory));
  }
}

void ImportantFileWriterCleaner::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AutoLock scoped_lock(task_runner_lock_);
  DCHECK(!task_runner_ || task_runner_ == SequencedTaskRunnerHandle::Get());
  task_runner_ = SequencedTaskRunnerHandle::Get();
}

void ImportantFileWriterCleaner::Start() {
#if DCHECK_IS_ON()
  {
    AutoLock scoped_lock(task_runner_lock_);
    DCHECK(task_runner_);
  }
#endif
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_started())
    return;

  started_ = true;

  if (!pending_directories_.empty())
    ScheduleTask();
}

void ImportantFileWriterCleaner::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started())
    return;

  if (is_running())
    stop_flag_.store(true, std::memory_order_relaxed);
  else
    DoStop();
}

void ImportantFileWriterCleaner::UninitializeForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_started());
  {
    AutoLock scoped_lock(task_runner_lock_);
    task_runner_ = nullptr;
  }
  // AddDirectory may have been called after Stop. Clear the containers just in
  // case.
  important_directories_.clear();
  pending_directories_.clear();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::Time ImportantFileWriterCleaner::GetUpperBoundTimeForTest() const {
  return upper_bound_time_;
}

ImportantFileWriterCleaner::ImportantFileWriterCleaner()
    : upper_bound_time_(GetUpperBoundTime()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ImportantFileWriterCleaner::AddDirectoryImpl(const FilePath& directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!important_directories_.insert(directory).second)
    return;  // This directory has already been seen.

  pending_directories_.push_back(directory);

  if (!is_started())
    return;  // Nothing more to do if Start() has not been called.

  // Start the background task if it's not already running. If it is running, a
  // new task will be posted on completion of the current one by
  // OnBackgroundTaskFinished to handle all directories added while it was
  // running.
  if (!is_running())
    ScheduleTask();
}

void ImportantFileWriterCleaner::ScheduleTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_started());
  DCHECK(!is_running());
  DCHECK(!pending_directories_.empty());
  DCHECK(!stop_flag_.load(std::memory_order_relaxed));

  // Pass the set of directories to be processed.
  running_ = ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {TaskPriority::BEST_EFFORT, TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
       MayBlock()},
      BindOnce(&ImportantFileWriterCleaner::CleanInBackground,
               upper_bound_time_, std::move(pending_directories_),
               std::ref(stop_flag_)),
      // Unretained is safe here since the cleaner instance is never destroyed.
      BindOnce(&ImportantFileWriterCleaner::OnBackgroundTaskFinished,
               Unretained(this)));
}

// static
bool ImportantFileWriterCleaner::CleanInBackground(
    Time upper_bound_time,
    std::vector<FilePath> directories,
    std::atomic_bool& stop_flag) {
  DCHECK(!directories.empty());
  for (auto scan = directories.begin(), end = directories.end(); scan != end;
       ++scan) {
    const auto& directory = *scan;
    FileEnumerator file_enum(
        directory, /*recursive=*/false, FileEnumerator::FILES,
        FormatTemporaryFileName(FILE_PATH_LITERAL("*")).value());
    for (FilePath path = file_enum.Next(); !path.empty();
         path = file_enum.Next()) {
      const FileEnumerator::FileInfo info = file_enum.GetInfo();
      if (info.GetLastModifiedTime() >= upper_bound_time)
        continue;
      // Cleanup is a best-effort process, so ignore any failures here and
      // continue to clean as much as possible. Metrics tell us that ~98.4% of
      // directories are cleaned with no failures.
      DeleteFile(path);
      // Break out without checking for the next file if a stop is requested.
      if (stop_flag.load(std::memory_order_relaxed))
        return false;
    }
  }
  return true;
}

void ImportantFileWriterCleaner::OnBackgroundTaskFinished(
    bool processing_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  running_ = false;

  // There are no other accessors of |stop_flag_| at this point, so atomic
  // operations aren't needed. There is no way to read it without such, so use
  // the same (relaxed) ordering as elsewhere.
  const bool stop = stop_flag_.exchange(false, std::memory_order_relaxed);
  DCHECK(stop || processing_completed);

  if (stop) {
    DoStop();
  } else if (!pending_directories_.empty()) {
    // Run the task again with the new directories.
    ScheduleTask();
  }  // else do nothing until a new directory is added.
}

void ImportantFileWriterCleaner::DoStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_started());
  DCHECK(!is_running());

  important_directories_.clear();
  pending_directories_.clear();
  started_ = false;
}

}  // namespace base
