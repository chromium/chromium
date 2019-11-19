// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/file_stream_forwarder.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace arc {

namespace {

constexpr int kBufSize = 32 * 1024;

}  // namespace

FileStreamForwarder::FileStreamForwarder(
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t size,
    base::ScopedFD fd_dest,
    ResultCallback callback)
    : context_(context),
      url_(url),
      offset_(offset),
      remaining_size_(size),
      fd_dest_(std::move(fd_dest)),
      callback_(std::move(callback)),
      task_runner_(base::CreateSequencedTaskRunner(
          // It's safe to shutdown without waiting for the
          // completion of tasks running with this task runner.
          {base::ThreadPool(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      buf_(base::MakeRefCounted<net::IOBufferWithSize>(kBufSize)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileStreamForwarder::Start, base::Unretained(this)));
}

void FileStreamForwarder::Destroy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&FileStreamForwarder::DestroyOnIOThread,
                                base::Unretained(this)));
}

FileStreamForwarder::~FileStreamForwarder() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!callback_.is_null())  // Aborted before completion.
    NotifyCompleted(false);
  // Use the task runner to close the FD.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce([](base::ScopedFD fd) {}, std::move(fd_dest_)));
}

void FileStreamForwarder::DestroyOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  delete this;
}

void FileStreamForwarder::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  stream_reader_ = context_->CreateFileStreamReader(
      url_, offset_, remaining_size_, base::Time());
  if (!stream_reader_) {
    LOG(ERROR) << "CreateFileStreamReader failed.";
    NotifyCompleted(false);
    return;
  }
  DoRead();
}

void FileStreamForwarder::DoRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (remaining_size_ == 0) {
    NotifyCompleted(true);
    return;
  }
  const int result = stream_reader_->Read(
      buf_.get(), std::min<int64_t>(buf_->size(), remaining_size_),
      base::Bind(&FileStreamForwarder::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    // Read result is returned synchronously.
    OnReadCompleted(result);
  }
}

void FileStreamForwarder::OnReadCompleted(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result <= 0) {
    if (result == 0) {
      LOG(ERROR) << remaining_size_ << " more bytes to read, but reached EOF.";
    } else {
      LOG(ERROR) << "Read failed " << net::ErrorToString(result);
    }
    NotifyCompleted(false);
    return;
  }
  remaining_size_ -= result;
  DCHECK_GE(remaining_size_, 0);

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(
          [](int fd, scoped_refptr<net::IOBuffer> buf, int size) {
            const bool result =
                base::WriteFileDescriptor(fd, buf->data(), size);
            PLOG_IF(ERROR, !result) << "Write failed.";
            return result;
          },
          fd_dest_.get(), buf_, result),
      base::BindOnce(&FileStreamForwarder::OnWriteCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FileStreamForwarder::OnWriteCompleted(bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!result) {
    NotifyCompleted(false);
    return;
  }
  // Continue reading.
  DoRead();
}

void FileStreamForwarder::NotifyCompleted(bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback_), result));
}

}  // namespace arc
