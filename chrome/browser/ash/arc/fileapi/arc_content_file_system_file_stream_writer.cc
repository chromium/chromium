// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_file_stream_writer.h"

#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/files/file.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace arc {

namespace {

// Calls base::File::WriteAtCurrentPosNoBestEffort with the given buffer.
// Returns the number of bytes written, or std::nullopt on error.
std::optional<size_t> WriteFile(base::File* file,
                                scoped_refptr<net::IOBuffer> buffer,
                                int buffer_length) {
  return file->WriteAtCurrentPosNoBestEffort(
      buffer->span().first(base::checked_cast<size_t>(buffer_length)));
}

// Seeks the file, returns 0 on success, or errno on an error.
int SeekFile(base::File* file, size_t offset) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // lseek() instead of |file|'s method for errno.
  off_t result = lseek(file->GetPlatformFile(), offset, SEEK_SET);
  return result < 0 ? errno : 0;
}

// Flushes the file, returns 0 on success, or errno on an error.
int FlushFile(base::File* file) {
  bool success = file->Flush();
  return success ? 0 : errno;
}

}  // namespace

ArcContentFileSystemFileStreamWriter::ArcContentFileSystemFileStreamWriter(
    const GURL& arc_url,
    int64_t offset)
    : arc_url_(arc_url),
      offset_(offset),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      has_pending_operation_(false) {}

ArcContentFileSystemFileStreamWriter::~ArcContentFileSystemFileStreamWriter() {
  CloseInternal(CloseStatus::kStatusOk);
  // Use the task runner to destruct |file_| after the completion of all
  // in-flight operations.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&base::DeletePointer<base::File>, file_.release()));
}

int ArcContentFileSystemFileStreamWriter::Write(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  has_pending_operation_ = true;
  if (file_) {
    WriteInternal(buffer, buffer_length, std::move(callback));
    return net::ERR_IO_PENDING;
  }
  file_system_operation_runner_util::OpenFileSessionToWriteOnIOThread(
      arc_url_,
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnOpenFileSession,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::WrapRefCounted(buffer), buffer_length,
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

int ArcContentFileSystemFileStreamWriter::Cancel(
    net::CompletionOnceCallback callback) {
  if (!has_pending_operation_)
    return net::ERR_UNEXPECTED;

  DCHECK(!callback.is_null());
  cancel_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

int ArcContentFileSystemFileStreamWriter::Flush(
    storage::FlushMode /*flush_mode*/,
    net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  // Write() is not called yet, so there's nothing to flush.
  if (!file_)
    return net::OK;

  has_pending_operation_ = true;

  // |file_| is alive on FlushFile(), since the destructor will destruct
  // |task_runner_| along with |file_| and FlushFile() won't be called.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&FlushFile, file_.get()),
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnFlushFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return net::ERR_IO_PENDING;
}

void ArcContentFileSystemFileStreamWriter::CloseInternal(
    const CloseStatus status) {
  // Don't propagate the success status if there was a prior error or cancel.
  if (!session_id_.empty()) {
    file_system_operation_runner_util::CloseFileSession(session_id_, status);
    // Clear the session to guarantee only one close per session.
    session_id_.clear();
  }
}

void ArcContentFileSystemFileStreamWriter::WriteInternal(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  DCHECK(has_pending_operation_);

  // |file_| is alive on WriteFile(), since the destructor will destruct
  // |task_runner_| along with |file_| and WriteFile() won't be called.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteFile, file_.get(), base::WrapRefCounted(buffer),
                     buffer_length),
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcContentFileSystemFileStreamWriter::OnWrite(
    net::CompletionOnceCallback callback,
    std::optional<size_t> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(has_pending_operation_);

  if (CancelIfRequested()) {
    return;
  }
  has_pending_operation_ = false;
  if (!result.has_value()) {
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  std::move(callback).Run(result.value());
}

void ArcContentFileSystemFileStreamWriter::OnOpenFileSession(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    mojom::FileSessionPtr file_session) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_);
  DCHECK(has_pending_operation_);
  DCHECK(session_id_.empty());

  if (CancelIfRequested())
    return;

  if (file_session.is_null() || file_session->url_id.empty()) {
    // The file_session and its url_id are required from the file system.
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  session_id_ = std::move(file_session->url_id);
  DCHECK(session_id_.length() > 0);
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(file_session->fd));
  if (!platform_handle.is_valid()) {
    has_pending_operation_ = false;
    LOG(ERROR) << "PassWrappedInternalPlatformHandle failed";
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_INVALID_HANDLE);
    return;
  }
  file_ = std::make_unique<base::File>(platform_handle.ReleaseFD());
  DCHECK(file_->IsValid());
  if (offset_ == 0) {
    // We can skip the step to seek the file.
    OnSeekFile(buf, buffer_length, std::move(callback), 0);
    return;
  }
  // |file_| is alive on SeekFile(), since the destructor will destruct
  // |task_runner_| along with |file_| and SeekFile() won't be called.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SeekFile, file_.get(), offset_),
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnSeekFile,
                     weak_ptr_factory_.GetWeakPtr(), buf, buffer_length,
                     std::move(callback)));
}

void ArcContentFileSystemFileStreamWriter::OnSeekFile(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    int seek_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;

  if (seek_result == 0) {
    // File stream is ready. Resume Write().
    WriteInternal(buf.get(), buffer_length, std::move(callback));
  } else {
    has_pending_operation_ = false;
    LOG(ERROR) << "Failed to seek: "
               << logging::SystemErrorCodeToString(seek_result);
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(
        net::FileErrorToNetError(base::File::OSErrorToFileError(seek_result)));
  }
}

void ArcContentFileSystemFileStreamWriter::OnFlushFile(
    net::CompletionOnceCallback callback,
    int flush_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;
  has_pending_operation_ = false;
  std::move(callback).Run(flush_result);
}

bool ArcContentFileSystemFileStreamWriter::CancelIfRequested() {
  DCHECK(has_pending_operation_);

  if (cancel_callback_.is_null())
    return false;

  has_pending_operation_ = false;
  CloseInternal(CloseStatus::kStatusCancel);
  std::move(cancel_callback_).Run(net::OK);
  return true;
}

}  // namespace arc
