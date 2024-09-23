// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_file_stream_reader.h"

#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace arc {

namespace {

// Calls base::File::ReadAtCurrentPosNoBestEffort with the given buffer.
// Return the number of bytes read, or std::nullopt on error.
std::optional<size_t> ReadFile(base::File* file,
                               scoped_refptr<net::IOBuffer> buffer,
                               int buffer_length) {
  return file->ReadAtCurrentPosNoBestEffort(
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

void OnGetSizeFromFileHandle(net::Int64CompletionOnceCallback callback,
                             base::File::Error error,
                             int64_t size) {
  std::move(callback).Run(error == base::File::FILE_OK ? size
                                                       : net::ERR_FAILED);
}

}  // namespace

ArcContentFileSystemFileStreamReader::ArcContentFileSystemFileStreamReader(
    const GURL& arc_url,
    int64_t offset)
    : arc_url_(arc_url), offset_(offset) {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ArcContentFileSystemFileStreamReader::~ArcContentFileSystemFileStreamReader() {
  CloseInternal(CloseStatus::kStatusOk);
  // Use the task runner to destruct |file_| after the completion of all
  // in-flight operations.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&base::DeletePointer<base::File>, file_.release()));
}

int ArcContentFileSystemFileStreamReader::Read(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (file_) {
    ReadInternal(buffer, buffer_length, std::move(callback));
    return net::ERR_IO_PENDING;
  }
  file_system_operation_runner_util::OpenFileSessionToReadOnIOThread(
      arc_url_,
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnOpenFileSession,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::WrapRefCounted(buffer), buffer_length,
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

int64_t ArcContentFileSystemFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_operation_runner_util::GetFileSizeOnIOThread(
      arc_url_,
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnGetFileSize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return net::ERR_IO_PENDING;
}

void ArcContentFileSystemFileStreamReader::CloseInternal(
    const CloseStatus status) {
  // Don't propagate the success status if there was a prior error.
  if (!session_id_.empty()) {
    file_system_operation_runner_util::CloseFileSession(session_id_, status);
    // Clear the session to guarantee only one close per session.
    session_id_.clear();
  }
}

void ArcContentFileSystemFileStreamReader::ReadInternal(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());

  // |file_| is alive on ReadFile(), since the destructor will destruct
  // |task_runner_| along with |file_| and ReadFile() won't be called.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadFile, file_.get(), base::WrapRefCounted(buffer),
                     buffer_length),
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcContentFileSystemFileStreamReader::OnRead(
    net::CompletionOnceCallback callback,
    std::optional<size_t> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!result.has_value()) {
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  std::move(callback).Run(result.value());
}

void ArcContentFileSystemFileStreamReader::OnGetFileSize(
    net::Int64CompletionOnceCallback callback,
    int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (size == kUnknownFileSize) {
    GetFileSizeFromOpenFileOnIOThread(
        arc_url_,
        base::BindOnce(&OnGetSizeFromFileHandle, std::move(callback)));
  } else {
    if (size < 0) {
      CloseInternal(CloseStatus::kStatusError);
    }
    std::move(callback).Run(size < 0 ? net::ERR_FAILED : size);
  }
}

void ArcContentFileSystemFileStreamReader::OnOpenFileSession(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    mojom::FileSessionPtr file_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_);
  DCHECK(session_id_.empty());

  if (file_handle.is_null() || file_handle->url_id.empty()) {
    // The file_handle and its url_id are required from the file system.
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  session_id_ = std::move(file_handle->url_id);
  DCHECK(session_id_.length() > 0);
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(file_handle->fd));
  if (!platform_handle.is_valid()) {
    LOG(ERROR) << "PassWrappedInternalPlatformHandle failed";
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_INVALID_HANDLE);
    return;
  }
  file_ = std::make_unique<base::File>(platform_handle.ReleaseFD());
  if (!file_->IsValid()) {
    LOG(ERROR) << "Invalid file.";
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  // |file_| is alive on SeekFile(), since the destructor will destruct
  // |task_runner_| along with |file_| and SeekFile() won't be called.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SeekFile, file_.get(), offset_),
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnSeekFile,
                     weak_ptr_factory_.GetWeakPtr(), buf, buffer_length,
                     std::move(callback)));
}

void ArcContentFileSystemFileStreamReader::OnSeekFile(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    int seek_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  switch (seek_result) {
    case 0:
      // File stream is ready. Resume Read().
      ReadInternal(buf.get(), buffer_length, std::move(callback));
      break;
    case ESPIPE: {
      // Pipe is not seekable. Just consume the contents.
      const size_t kTemporaryBufferSize = 1024 * 1024;
      auto temporary_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(kTemporaryBufferSize);
      ConsumeFileContents(buf, buffer_length, std::move(callback),
                          temporary_buffer, offset_);
      break;
    }
    default:
      LOG(ERROR) << "Failed to seek: " << seek_result;
      CloseInternal(CloseStatus::kStatusError);
      std::move(callback).Run(net::FileErrorToNetError(
          base::File::OSErrorToFileError(seek_result)));
  }
}

void ArcContentFileSystemFileStreamReader::ConsumeFileContents(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    scoped_refptr<net::IOBufferWithSize> temporary_buffer,
    int64_t num_bytes_to_consume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  if (num_bytes_to_consume == 0) {
    // File stream is ready. Resume Read().
    ReadInternal(buf.get(), buffer_length, std::move(callback));
    return;
  }
  auto num_bytes_to_read = std::min(
      static_cast<int64_t>(temporary_buffer->size()), num_bytes_to_consume);
  // TODO(hashimoto): This may block the worker thread forever. crbug.com/673222
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadFile, file_.get(), temporary_buffer,
                     num_bytes_to_read),
      base::BindOnce(
          &ArcContentFileSystemFileStreamReader::OnConsumeFileContents,
          weak_ptr_factory_.GetWeakPtr(), buf, buffer_length,
          std::move(callback), temporary_buffer, num_bytes_to_consume));
}

void ArcContentFileSystemFileStreamReader::OnConsumeFileContents(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    scoped_refptr<net::IOBufferWithSize> temporary_buffer,
    int64_t num_bytes_to_consume,
    std::optional<size_t> read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!read_result.has_value()) {
    LOG(ERROR) << "Failed to consume the file stream.";
    CloseInternal(CloseStatus::kStatusError);
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  DCHECK_GE(num_bytes_to_consume, static_cast<int64_t>(read_result.value()));
  num_bytes_to_consume -= read_result.value();
  ConsumeFileContents(buf, buffer_length, std::move(callback), temporary_buffer,
                      num_bytes_to_consume);
}

}  // namespace arc
