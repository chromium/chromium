// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_writer.h"

#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace arc {

namespace {

// Calls base::File::WriteAtCurrentPosNoBestEffort with the given buffer.
// Returns the number of bytes written, or -1 on error.
int WriteFile(base::File* file,
              scoped_refptr<net::IOBuffer> buffer,
              int buffer_length) {
  return file->WriteAtCurrentPosNoBestEffort(buffer->data(), buffer_length);
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
      task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      has_pending_operation_(false) {}

ArcContentFileSystemFileStreamWriter::~ArcContentFileSystemFileStreamWriter() {
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
  file_system_operation_runner_util::OpenFileToWriteOnIOThread(
      arc_url_,
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnOpenFile,
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
    net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  // Write() is not called yet, so there's nothing to flush.
  if (!file_)
    return net::OK;

  has_pending_operation_ = true;

  // |file_| is alive on FlushFile(), since the descructor will destruct
  // |task_runner_| along with |file_| and FlushFile() won't be called.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&FlushFile, file_.get()),
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnFlushFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return net::ERR_IO_PENDING;
}

void ArcContentFileSystemFileStreamWriter::WriteInternal(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  DCHECK(has_pending_operation_);

  // |file_| is alive on WriteFile(), since the descructor will destruct
  // |task_runner_| along with |file_| and WriteFile() won't be called.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&WriteFile, file_.get(), base::WrapRefCounted(buffer),
                     buffer_length),
      base::BindOnce(&ArcContentFileSystemFileStreamWriter::OnWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcContentFileSystemFileStreamWriter::OnWrite(
    net::CompletionOnceCallback callback,
    int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;

  has_pending_operation_ = false;
  std::move(callback).Run(result < 0 ? net::ERR_FAILED : result);
}

void ArcContentFileSystemFileStreamWriter::OnOpenFile(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    mojo::ScopedHandle handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_);
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;

  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(handle));
  if (!platform_handle.is_valid()) {
    has_pending_operation_ = false;
    LOG(ERROR) << "PassWrappedInternalPlatformHandle failed";
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  file_ = std::make_unique<base::File>(platform_handle.ReleaseFD());
  DCHECK(file_->IsValid());
  if (offset_ == 0) {
    // We can skip the step to seek the file.
    OnSeekFile(buf, buffer_length, std::move(callback), 0);
    return;
  }
  // |file_| is alive on SeekFile(), since the descructor will destruct
  // |task_runner_| along with |file_| and SeekFile() won't be called.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&SeekFile, file_.get(), offset_),
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
  std::move(cancel_callback_).Run(net::OK);
  return true;
}

}  // namespace arc
