// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"

#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
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

// Calls base::File::ReadAtCurrentPosNoBestEffort with the given buffer.
int ReadFile(base::File* file,
             scoped_refptr<net::IOBuffer> buffer,
             int buffer_length) {
  return file->ReadAtCurrentPosNoBestEffort(buffer->data(), buffer_length);
}

// Seeks the file, returns 0 on success, or errno on an error.
int SeekFile(base::File* file, size_t offset) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // lseek() instead of |file|'s method for errno.
  off_t result = lseek(file->GetPlatformFile(), offset, SEEK_SET);
  return result < 0 ? errno : 0;
}

}  // namespace

ArcContentFileSystemFileStreamReader::ArcContentFileSystemFileStreamReader(
    const GURL& arc_url,
    int64_t offset)
    : arc_url_(arc_url), offset_(offset) {
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ArcContentFileSystemFileStreamReader::~ArcContentFileSystemFileStreamReader() {
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
  file_system_operation_runner_util::OpenFileToReadOnIOThread(
      arc_url_,
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnOpenFile,
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

void ArcContentFileSystemFileStreamReader::ReadInternal(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_);
  DCHECK(file_->IsValid());
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadFile, file_.get(), base::WrapRefCounted(buffer),
                     buffer_length),
      base::BindOnce(&ArcContentFileSystemFileStreamReader::OnRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcContentFileSystemFileStreamReader::OnRead(
    net::CompletionOnceCallback callback,
    int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(result < 0 ? net::ERR_FAILED : result);
}

void ArcContentFileSystemFileStreamReader::OnGetFileSize(
    net::Int64CompletionOnceCallback callback,
    int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(size < 0 ? net::ERR_FAILED : size);
}

void ArcContentFileSystemFileStreamReader::OnOpenFile(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    net::CompletionOnceCallback callback,
    mojo::ScopedHandle handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_);

  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(handle));
  if (!platform_handle.is_valid()) {
    LOG(ERROR) << "PassWrappedInternalPlatformHandle failed";
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  file_ = std::make_unique<base::File>(platform_handle.ReleaseFD());
  if (!file_->IsValid()) {
    LOG(ERROR) << "Invalid file.";
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&SeekFile, file_.get(), offset_),
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
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
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
    int read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (read_result < 0) {
    LOG(ERROR) << "Failed to consume the file stream.";
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  DCHECK_GE(num_bytes_to_consume, read_result);
  num_bytes_to_consume -= read_result;
  ConsumeFileContents(buf, buffer_length, std::move(callback), temporary_buffer,
                      num_bytes_to_consume);
}

}  // namespace arc
