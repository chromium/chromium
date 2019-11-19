// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/file_flusher.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// FileFlusher::Job

class FileFlusher::Job {
 public:
  Job(const base::WeakPtr<FileFlusher>& master,
      const base::FilePath& path,
      bool recursive,
      const FileFlusher::OnFlushCallback& on_flush_callback,
      const base::Closure& callback);
  ~Job() = default;

  void Start();
  void Cancel();

  const base::FilePath& path() const { return path_; }
  bool started() const { return started_; }

 private:
  // Flush files on a blocking pool thread.
  void FlushAsync();

  // Schedule a FinishOnUIThread task to run on the UI thread.
  void ScheduleFinish();

  // Finish the job by notifying |master_| and self destruct on the UI thread.
  void FinishOnUIThread();

  base::WeakPtr<FileFlusher> master_;
  const base::FilePath path_;
  const bool recursive_;
  const FileFlusher::OnFlushCallback on_flush_callback_;
  const base::Closure callback_;

  bool started_ = false;
  base::AtomicFlag cancel_flag_;
  bool finish_scheduled_ = false;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

FileFlusher::Job::Job(const base::WeakPtr<FileFlusher>& master,
                      const base::FilePath& path,
                      bool recursive,
                      const FileFlusher::OnFlushCallback& on_flush_callback,
                      const base::Closure& callback)
    : master_(master),
      path_(path),
      recursive_(recursive),
      on_flush_callback_(on_flush_callback),
      callback_(callback) {}

void FileFlusher::Job::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!started());

  started_ = true;

  if (cancel_flag_.IsSet()) {
    ScheduleFinish();
    return;
  }

  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&FileFlusher::Job::FlushAsync, base::Unretained(this)),
      base::Bind(&FileFlusher::Job::FinishOnUIThread, base::Unretained(this)));
}

void FileFlusher::Job::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  cancel_flag_.Set();

  // Cancel() could be called in an iterator/range loop in master thus don't
  // invoke FinishOnUIThread in-place.
  if (!started())
    ScheduleFinish();
}

void FileFlusher::Job::FlushAsync() {
  VLOG(1) << "Flushing files under " << path_.value();

  base::FileEnumerator traversal(path_, recursive_,
                                 base::FileEnumerator::FILES);
  for (base::FilePath current = traversal.Next();
       !current.empty() && !cancel_flag_.IsSet(); current = traversal.Next()) {
    base::File currentFile(current,
                           base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    if (!currentFile.IsValid()) {
      VLOG(1) << "Unable to flush file:" << current.value();
      continue;
    }

    currentFile.Flush();
    currentFile.Close();

    if (!on_flush_callback_.is_null())
      on_flush_callback_.Run(current);
  }
}

void FileFlusher::Job::ScheduleFinish() {
  if (finish_scheduled_)
    return;

  finish_scheduled_ = true;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Job::FinishOnUIThread, base::Unretained(this)));
}

void FileFlusher::Job::FinishOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!callback_.is_null())
    callback_.Run();

  if (master_)
    master_->OnJobDone(this);

  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// FileFlusher

FileFlusher::FileFlusher() {}

FileFlusher::~FileFlusher() {
  for (auto* job : jobs_)
    job->Cancel();
}

void FileFlusher::RequestFlush(const base::FilePath& path,
                               bool recursive,
                               const base::Closure& callback) {
  for (auto* job : jobs_) {
    if (path == job->path() || path.IsParent(job->path()))
      job->Cancel();
  }

  jobs_.push_back(new Job(weak_factory_.GetWeakPtr(), path, recursive,
                          on_flush_callback_for_test_, callback));
  ScheduleJob();
}

void FileFlusher::PauseForTest() {
  DCHECK(std::none_of(jobs_.begin(), jobs_.end(),
                      [](const Job* job) { return job->started(); }));
  paused_for_test_ = true;
}

void FileFlusher::ResumeForTest() {
  paused_for_test_ = false;
  ScheduleJob();
}

void FileFlusher::ScheduleJob() {
  if (jobs_.empty() || paused_for_test_)
    return;

  auto* job = jobs_.front();
  if (!job->started())
    job->Start();
}

void FileFlusher::OnJobDone(FileFlusher::Job* job) {
  for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
    if (*it == job) {
      jobs_.erase(it);
      break;
    }
  }

  ScheduleJob();
}

}  // namespace chromeos
