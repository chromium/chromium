// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_

#include <atomic>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/post_job.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

namespace safe_browsing {

// This class encapsulates a base::PostJob call made to open multiple files to
// set up multiple FileAnalysiRequests to avoid using too many system resources.
class FileOpeningJob {
 public:
  // Struct to store data necessary for a synchronized file opening task.
  struct FileOpeningTask {
    FileOpeningTask();
    ~FileOpeningTask();

    // Non-owning pointer to the request corresponding to the file to open.
    raw_ptr<safe_browsing::FileAnalysisRequest, AcrossTasksDanglingUntriaged>
        request = nullptr;

    // Indicates if this task has been taken and is owned by a thread.
    std::atomic_bool taken{false};
  };

  static size_t GetMaxFileOpeningThreads();

  // This constructor will call base::PostJob on the given tasks and only cancel
  // that job in the destructor. This means that if the result of the tasks is
  // no longer useful (if a folder upload is cancelled for instance), `this`
  // should be deleted so that resources aren't spent opening files.
  explicit FileOpeningJob(std::vector<FileOpeningTask> tasks);
  ~FileOpeningJob();

 private:
  FRIEND_TEST_ALL_PREFIXES(FileOpeningJobTest, MaxThreadsFlag);

  // Processes the next file opening task that hasn't been taken so far. This is
  // the main callback passed to base::PostJob and it will run concurrently on
  // multiple threads.
  void ProcessNextTask(base::JobDelegate* job_delegate);

  // Synchronized getter method to read `num_unopened_files_`.
  size_t num_unopened_files();

  // Returns the maximum number of threads that should be opening files. This
  // is dependant on the corresponding flags and on the number of remaining
  // files to open. As this is meant to be used by base::PostJob, this method
  // needs a `worker_count` argument in its signature, even though it is unused.
  size_t MaxConcurrentThreads(size_t /*worker_count*/);

  // Initialized with the size of `tasks_` in the constructor. This should
  // always be accessed through its corresponding getter method to avoid
  // synchronization bugs.
  std::atomic_size_t num_unopened_files_;

  // Points to the job initialized in the constructor.
  base::JobHandle file_opening_job_handle_;

  std::vector<FileOpeningTask> tasks_;
  size_t max_threads_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_
