// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_writer.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/scoped_file_opener.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {

class FileStreamWriter::OperationRunner
    : public base::RefCountedThreadSafe<
          FileStreamWriter::OperationRunner,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  OperationRunner() : file_handle_(0) {}

  // Opens a file for writing and calls the completion callback. Must be called
  // on UI thread.
  void OpenFileOnUIThread(const storage::FileSystemURL& url,
                          storage::AsyncFileUtil::StatusCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(abort_callback_.is_null());

    util::FileSystemURLParser parser(url);
    if (!parser.Parse()) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(std::move(callback), base::File::FILE_ERROR_SECURITY));
      return;
    }

    file_system_ = parser.file_system()->GetWeakPtr();
    file_opener_.reset(new ScopedFileOpener(
        parser.file_system(), parser.file_path(), OPEN_FILE_MODE_WRITE,
        base::Bind(&OperationRunner::OnOpenFileCompletedOnUIThread, this,
                   base::Passed(&callback))));
  }

  // Requests writing bytes to the file. In case of either success or a failure
  // |callback| is executed. Must be called on UI thread.
  void WriteFileOnUIThread(scoped_refptr<net::IOBuffer> buffer,
                           int64_t offset,
                           int length,
                           storage::AsyncFileUtil::StatusCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(abort_callback_.is_null());

    // If the file system got unmounted, then abort the writing operation.
    if (!file_system_.get()) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(std::move(callback), base::File::FILE_ERROR_ABORT));
      return;
    }

    abort_callback_ = file_system_->WriteFile(
        file_handle_, buffer.get(), offset, length,
        base::BindOnce(&OperationRunner::OnWriteFileCompletedOnUIThread, this,
                       std::move(callback)));
  }

  // Aborts the most recent operation (if exists) and closes a file if opened.
  // The runner must not be used anymore after calling this method.
  void CloseRunnerOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!abort_callback_.is_null()) {
      const AbortCallback last_abort_callback = abort_callback_;
      abort_callback_ = AbortCallback();
      last_abort_callback.Run();
    }

    // Close the file (if opened).
    file_opener_.reset();
  }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<OperationRunner>;

  virtual ~OperationRunner() {}

  // Remembers a file handle for further operations and forwards the result to
  // the IO thread.
  void OnOpenFileCompletedOnUIThread(
      storage::AsyncFileUtil::StatusCallback callback,
      int file_handle,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    abort_callback_ = AbortCallback();
    if (result == base::File::FILE_OK)
      file_handle_ = file_handle;

    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(std::move(callback), result));
  }

  // Forwards a response of writing to a file to the IO thread.
  void OnWriteFileCompletedOnUIThread(
      storage::AsyncFileUtil::StatusCallback callback,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    abort_callback_ = AbortCallback();
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(std::move(callback), result));
  }

  AbortCallback abort_callback_;
  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
  std::unique_ptr<ScopedFileOpener> file_opener_;
  int file_handle_;

  DISALLOW_COPY_AND_ASSIGN(OperationRunner);
};

FileStreamWriter::FileStreamWriter(const storage::FileSystemURL& url,
                                   int64_t initial_offset)
    : url_(url),
      current_offset_(initial_offset),
      runner_(new OperationRunner),
      state_(NOT_INITIALIZED) {}

FileStreamWriter::~FileStreamWriter() {
  // Close the runner explicitly if the file streamer is
  if (state_ != CANCELLING) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&OperationRunner::CloseRunnerOnUIThread, runner_));
  }

  // If a write is in progress, mark it as completed.
  TRACE_EVENT_ASYNC_END0("file_system_provider", "FileStreamWriter::Write",
                         this);
}

void FileStreamWriter::Initialize(base::OnceClosure pending_closure,
                                  net::CompletionOnceCallback error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(NOT_INITIALIZED, state_);
  state_ = INITIALIZING;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OperationRunner::OpenFileOnUIThread, runner_, url_,
                     base::BindOnce(&FileStreamWriter::OnOpenFileCompleted,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(pending_closure),
                                    std::move(error_callback))));
}

void FileStreamWriter::OnOpenFileCompleted(
    base::OnceClosure pending_closure,
    net::CompletionOnceCallback error_callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == INITIALIZING || state_ == CANCELLING);
  if (state_ == CANCELLING)
    return;

  // In case of an error, return immediately using the |error_callback| of the
  // Write() pending request.
  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    std::move(error_callback).Run(net::FileErrorToNetError(result));
    return;
  }

  DCHECK_EQ(base::File::FILE_OK, result);
  state_ = INITIALIZED;

  // Run the task waiting for the initialization to be completed.
  std::move(pending_closure).Run();
}

int FileStreamWriter::Write(net::IOBuffer* buffer,
                            int buffer_length,
                            net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_BEGIN1("file_system_provider",
                           "FileStreamWriter::Write",
                           this,
                           "buffer_length",
                           buffer_length);

  write_callback_ = std::move(callback);
  switch (state_) {
    case NOT_INITIALIZED:
      // Lazily initialize with the first call to Write().
      Initialize(
          base::BindOnce(&FileStreamWriter::WriteAfterInitialized,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::WrapRefCounted(buffer), buffer_length,
                         base::BindOnce(&FileStreamWriter::OnWriteCompleted,
                                        weak_ptr_factory_.GetWeakPtr())),
          base::BindOnce(&FileStreamWriter::OnWriteCompleted,
                         weak_ptr_factory_.GetWeakPtr()));
      break;

    case INITIALIZING:
      NOTREACHED();
      break;

    case INITIALIZED:
      WriteAfterInitialized(buffer, buffer_length,
                            base::BindOnce(&FileStreamWriter::OnWriteCompleted,
                                           weak_ptr_factory_.GetWeakPtr()));
      break;

    case EXECUTING:
    case FAILED:
    case CANCELLING:
      NOTREACHED();
      break;
  }

  return net::ERR_IO_PENDING;
}

int FileStreamWriter::Cancel(net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (state_ != INITIALIZING && state_ != EXECUTING)
    return net::ERR_UNEXPECTED;

  state_ = CANCELLING;

  // Abort and optimistically return an OK result code, as the aborting
  // operation is always forced and can't be cancelled. Similarly, for closing
  // files.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OperationRunner::CloseRunnerOnUIThread, runner_));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), net::OK));

  // If a write is in progress, mark it as completed.
  TRACE_EVENT_ASYNC_END0("file_system_provider", "FileStreamWriter::Write",
                         this);

  return net::ERR_IO_PENDING;
}

int FileStreamWriter::Flush(net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(CANCELLING, state_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     state_ == INITIALIZED ? net::OK : net::ERR_FAILED));

  return net::ERR_IO_PENDING;
}

void FileStreamWriter::OnWriteFileCompleted(
    int buffer_length,
    net::CompletionOnceCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == EXECUTING || state_ == CANCELLING);
  if (state_ == CANCELLING)
    return;

  state_ = INITIALIZED;

  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    std::move(callback).Run(net::FileErrorToNetError(result));
    return;
  }

  current_offset_ += buffer_length;
  std::move(callback).Run(buffer_length);
}

void FileStreamWriter::OnWriteCompleted(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ != CANCELLING)
    std::move(write_callback_).Run(result);

  TRACE_EVENT_ASYNC_END0(
      "file_system_provider", "FileStreamWriter::Write", this);
}

void FileStreamWriter::WriteAfterInitialized(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == INITIALIZED || state_ == CANCELLING);
  if (state_ == CANCELLING)
    return;

  state_ = EXECUTING;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OperationRunner::WriteFileOnUIThread, runner_, buffer,
                     current_offset_, buffer_length,
                     base::BindOnce(&FileStreamWriter::OnWriteFileCompleted,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    buffer_length, std::move(callback))));
}

}  // namespace file_system_provider
}  // namespace chromeos
