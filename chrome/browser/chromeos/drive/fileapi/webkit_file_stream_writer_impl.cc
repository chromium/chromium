// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/webkit_file_stream_writer_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/drive/fileapi/fileapi_worker.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/fileapi/file_stream_writer.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Creates a writable snapshot file of the |drive_path|.
void CreateWritableSnapshotFile(
    const WebkitFileStreamWriterImpl::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_path,
    const fileapi_internal::CreateWritableSnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &fileapi_internal::RunFileSystemCallback, file_system_getter,
          base::Bind(&fileapi_internal::CreateWritableSnapshotFile, drive_path,
                     google_apis::CreateRelayCallback(callback)),
          google_apis::CreateRelayCallback(
              base::Bind(callback, base::File::FILE_ERROR_FAILED,
                         base::FilePath(), base::Closure()))));
}

}  // namespace

WebkitFileStreamWriterImpl::WebkitFileStreamWriterImpl(
    const FileSystemGetter& file_system_getter,
    base::TaskRunner* file_task_runner,
    const base::FilePath& file_path,
    int64_t offset)
    : file_system_getter_(file_system_getter),
      file_task_runner_(file_task_runner),
      file_path_(file_path),
      offset_(offset),
      weak_ptr_factory_(this) {}

WebkitFileStreamWriterImpl::~WebkitFileStreamWriterImpl() {
  if (local_file_writer_) {
    // If the file is opened, close it at destructor.
    // It is necessary to close the local file in advance.
    local_file_writer_.reset();
    DCHECK(!close_callback_on_ui_thread_.is_null());
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             close_callback_on_ui_thread_);
  }
}

int WebkitFileStreamWriterImpl::Write(net::IOBuffer* buf,
                                      int buf_len,
                                      net::CompletionOnceCallback callback) {
  DCHECK(pending_write_callback_.is_null());
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(callback);

  // If the local file is already available, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Write(buf, buf_len, std::move(callback));

  // The local file is not yet ready. Create the writable snapshot.
  if (file_path_.empty())
    return net::ERR_FILE_NOT_FOUND;

  pending_write_callback_ = std::move(callback);
  CreateWritableSnapshotFile(
      file_system_getter_, file_path_,
      base::Bind(
          &WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile,
          weak_ptr_factory_.GetWeakPtr(), base::RetainedRef(buf), buf_len));
  return net::ERR_IO_PENDING;
}

int WebkitFileStreamWriterImpl::Cancel(net::CompletionOnceCallback callback) {
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(callback);

  // If LocalFileWriter is already created, just delegate the cancel to it.
  if (local_file_writer_)
    return local_file_writer_->Cancel(std::move(callback));

  // If file open operation is in-flight, wait for its completion and cancel
  // further write operation in WriteAfterCreateWritableSnapshotFile.
  if (!pending_write_callback_.is_null()) {
    // Dismiss pending write callback immediately.
    pending_write_callback_.Reset();
    pending_cancel_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }

  // Write() is not called yet.
  return net::ERR_UNEXPECTED;
}

int WebkitFileStreamWriterImpl::Flush(net::CompletionOnceCallback callback) {
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(callback);

  // If LocalFileWriter is already created, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Flush(std::move(callback));

  // There shouldn't be in-flight Write operation.
  DCHECK(pending_write_callback_.is_null());

  // Here is the case Flush() is called before any Write() invocation.
  // Do nothing.
  // Synchronization to the remote server is not done until the file is closed.
  return net::OK;
}

void WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile(
    net::IOBuffer* buf,
    int buf_len,
    base::File::Error open_result,
    const base::FilePath& local_path,
    const base::Closure& close_callback_on_ui_thread) {
  DCHECK(!local_file_writer_);

  if (!pending_cancel_callback_.is_null()) {
    DCHECK(pending_write_callback_.is_null());
    // Cancel() is called during the creation of the snapshot file.
    // Don't write to the file.
    if (open_result == base::File::FILE_OK) {
      // Here the file is internally created. To revert the operation, close
      // the file.
      DCHECK(!close_callback_on_ui_thread.is_null());
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                               close_callback_on_ui_thread);
    }

    base::ResetAndReturn(&pending_cancel_callback_).Run(net::OK);
    return;
  }

  DCHECK(!pending_write_callback_.is_null());

  if (open_result != base::File::FILE_OK) {
    DCHECK(close_callback_on_ui_thread.is_null());
    std::move(pending_write_callback_)
        .Run(net::FileErrorToNetError(open_result));
    return;
  }

  // Keep |close_callback| to close the file when the stream is destructed.
  DCHECK(!close_callback_on_ui_thread.is_null());
  close_callback_on_ui_thread_ = close_callback_on_ui_thread;
  local_file_writer_.reset(storage::FileStreamWriter::CreateForLocalFile(
      file_task_runner_.get(),
      local_path,
      offset_,
      storage::FileStreamWriter::OPEN_EXISTING_FILE));
  int result = local_file_writer_->Write(
      buf, buf_len,
      base::BindOnce(&WebkitFileStreamWriterImpl::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    OnWrite(result);
}

void WebkitFileStreamWriterImpl::OnWrite(int result) {
  std::move(pending_write_callback_).Run(result);
}

}  // namespace internal
}  // namespace drive
