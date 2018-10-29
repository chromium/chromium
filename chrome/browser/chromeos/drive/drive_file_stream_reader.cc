// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/drive.pb.h"
#include "components/drive/local_file_reader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"

using content::BrowserThread;

namespace drive {
namespace {

// Converts FileError code to net::Error code.
int FileErrorToNetError(FileError error) {
  return net::FileErrorToNetError(FileErrorToBaseFileError(error));
}

// Computes the concrete |start| offset and the |length| of |range| in a file
// of |total| size.
//
// This is a thin wrapper of HttpByteRange::ComputeBounds, extended to allow
// an empty range at the end of the file, like "Range: bytes 0-" on a zero byte
// file. This is for convenience in unifying implementation with the seek
// operation of stream reader. HTTP doesn't allow such ranges but we want to
// treat such seeking as valid.
bool ComputeConcretePosition(net::HttpByteRange range,
                             int64_t total,
                             int64_t* start,
                             int64_t* length) {
  // The special case when empty range in the end of the file is selected.
  if (range.HasFirstBytePosition() && range.first_byte_position() == total) {
    *start = range.first_byte_position();
    *length = 0;
    return true;
  }

  // Otherwise forward to HttpByteRange::ComputeBounds.
  if (!range.ComputeBounds(total))
    return false;
  *start = range.first_byte_position();
  *length = range.last_byte_position() - range.first_byte_position() + 1;
  return true;
}

}  // namespace

namespace internal {
namespace {

// Copies the content in |pending_data| into |buffer| at most
// |buffer_length| bytes, and erases the copied data from
// |pending_data|. Returns the number of copied bytes.
int ReadInternal(std::vector<std::unique_ptr<std::string>>* pending_data,
                 net::IOBuffer* buffer,
                 int buffer_length) {
  size_t index = 0;
  int offset = 0;
  for (; index < pending_data->size() && offset < buffer_length; ++index) {
    const std::string& chunk = *(*pending_data)[index];
    DCHECK(!chunk.empty());

    size_t bytes_to_read = std::min(
        chunk.size(), static_cast<size_t>(buffer_length - offset));
    std::memmove(buffer->data() + offset, chunk.data(), bytes_to_read);
    offset += bytes_to_read;
    if (bytes_to_read < chunk.size()) {
      // The chunk still has some remaining data.
      // So remove leading (copied) bytes, and quit the loop so that
      // the remaining data won't be deleted in the following erase().
      (*pending_data)[index]->erase(0, bytes_to_read);
      break;
    }
  }

  // Consume the copied data.
  pending_data->erase(pending_data->begin(), pending_data->begin() + index);

  return offset;
}

}  // namespace

LocalReaderProxy::LocalReaderProxy(
    std::unique_ptr<util::LocalFileReader> file_reader,
    int64_t length)
    : file_reader_(std::move(file_reader)),
      remaining_length_(length),
      weak_ptr_factory_(this) {
  DCHECK(file_reader_);
}

LocalReaderProxy::~LocalReaderProxy() = default;

int LocalReaderProxy::Read(net::IOBuffer* buffer,
                           int buffer_length,
                           net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_reader_);

  if (buffer_length > remaining_length_) {
    // Here, narrowing is safe.
    buffer_length = static_cast<int>(remaining_length_);
  }

  if (!buffer_length)
    return 0;

  file_reader_->Read(
      buffer, buffer_length,
      base::BindOnce(&LocalReaderProxy::OnReadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return net::ERR_IO_PENDING;
}

void LocalReaderProxy::OnGetContent(std::unique_ptr<std::string> data) {
  // This method should never be called, because no data should be received
  // from the network during the reading of local-cache file.
  NOTREACHED();
}

void LocalReaderProxy::OnCompleted(FileError error) {
  // If this method is called, no network error should be happened.
  DCHECK_EQ(FILE_ERROR_OK, error);
}

void LocalReaderProxy::OnReadCompleted(net::CompletionOnceCallback callback,
                                       int read_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_reader_);

  if (read_result >= 0) {
    // |read_result| bytes data is read.
    DCHECK_LE(read_result, remaining_length_);
    remaining_length_ -= read_result;
  } else {
    // An error occurs. Close the |file_reader_|.
    file_reader_.reset();
  }
  std::move(callback).Run(read_result);
}

NetworkReaderProxy::NetworkReaderProxy(int64_t offset,
                                       int64_t content_length,
                                       int64_t full_content_length,
                                       const base::Closure& job_canceller)
    : remaining_offset_(offset),
      remaining_content_length_(content_length),
      is_full_download_(offset + content_length == full_content_length),
      error_code_(net::OK),
      buffer_length_(0),
      job_canceller_(job_canceller) {}

NetworkReaderProxy::~NetworkReaderProxy() {
  if (!job_canceller_.is_null()) {
    job_canceller_.Run();
  }
}

int NetworkReaderProxy::Read(net::IOBuffer* buffer,
                             int buffer_length,
                             net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Check if there is no pending Read operation.
  DCHECK(!buffer_.get());
  DCHECK_EQ(buffer_length_, 0);
  DCHECK(callback_.is_null());
  // Validate the arguments.
  DCHECK(buffer);
  DCHECK_GT(buffer_length, 0);
  DCHECK(callback);

  if (error_code_ != net::OK) {
    // An error is already found. Return it immediately.
    return error_code_;
  }

  if (remaining_content_length_ == 0) {
    // If no more data, return immediately.
    return 0;
  }

  if (buffer_length > remaining_content_length_) {
    // Here, narrowing cast should be safe.
    buffer_length = static_cast<int>(remaining_content_length_);
  }

  if (pending_data_.empty()) {
    // No data is available. Keep the arguments, and return pending status.
    buffer_ = buffer;
    buffer_length_ = buffer_length;
    callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }

  int result = ReadInternal(&pending_data_, buffer, buffer_length);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);

  // Although OnCompleted() should reset |job_canceller_| when download is done,
  // due to timing issues the ReaderProxy instance may be destructed before the
  // notification. To fix the case we reset here earlier.
  if (is_full_download_ && remaining_content_length_ == 0)
    job_canceller_.Reset();

  return result;
}

void NetworkReaderProxy::OnGetContent(std::unique_ptr<std::string> data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(data && !data->empty());

  if (remaining_offset_ >= static_cast<int64_t>(data->length())) {
    // Skip unneeded leading data.
    remaining_offset_ -= data->length();
    return;
  }

  if (remaining_offset_ > 0) {
    // Erase unnecessary leading bytes.
    data->erase(0, static_cast<size_t>(remaining_offset_));
    remaining_offset_ = 0;
  }

  pending_data_.push_back(std::move(data));
  if (!buffer_.get()) {
    // No pending Read operation.
    return;
  }

  int result = ReadInternal(&pending_data_, buffer_.get(), buffer_length_);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);

  if (is_full_download_ && remaining_content_length_ == 0)
    job_canceller_.Reset();

  buffer_ = nullptr;
  buffer_length_ = 0;
  DCHECK(!callback_.is_null());
  base::ResetAndReturn(&callback_).Run(result);
}

void NetworkReaderProxy::OnCompleted(FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The downloading is completed, so we do not need to cancel the job
  // in the destructor.
  job_canceller_.Reset();

  if (error == FILE_ERROR_OK) {
    return;
  }

  error_code_ = FileErrorToNetError(error);
  pending_data_.clear();

  if (callback_.is_null()) {
    // No pending Read operation.
    return;
  }

  buffer_ = nullptr;
  buffer_length_ = 0;
  base::ResetAndReturn(&callback_).Run(error_code_);
}

}  // namespace internal

namespace {

// Calls FileSystemInterface::GetFileContent if the file system
// is available. If not, the |completion_callback| is invoked with
// FILE_ERROR_FAILED.
base::Closure GetFileContentOnUIThread(
    const DriveFileStreamReader::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_file_path,
    GetFileContentInitializedCallback initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FileSystemInterface* file_system = file_system_getter.Run();
  if (!file_system) {
    completion_callback.Run(FILE_ERROR_FAILED);
    return base::Closure();
  }

  return google_apis::CreateRelayCallback(file_system->GetFileContent(
      drive_file_path, std::move(initialized_callback), get_content_callback,
      completion_callback));
}

// Helper to run FileSystemInterface::GetFileContent on UI thread.
void GetFileContent(
    const DriveFileStreamReader::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_file_path,
    GetFileContentInitializedCallback initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback,
    const base::Callback<void(const base::Closure&)>& reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&GetFileContentOnUIThread, file_system_getter, drive_file_path,
                 base::Passed(google_apis::CreateRelayCallback(
                     std::move(initialized_callback))),
                 google_apis::CreateRelayCallback(get_content_callback),
                 google_apis::CreateRelayCallback(completion_callback)),
      reply_callback);
}

}  // namespace

DriveFileStreamReader::DriveFileStreamReader(
    const FileSystemGetter& file_system_getter,
    base::SequencedTaskRunner* file_task_runner)
    : file_system_getter_(file_system_getter),
      file_task_runner_(file_task_runner),
      weak_ptr_factory_(this) {
}

DriveFileStreamReader::~DriveFileStreamReader() = default;

bool DriveFileStreamReader::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return reader_proxy_.get() != nullptr;
}

void DriveFileStreamReader::Initialize(
    const base::FilePath& drive_file_path,
    const net::HttpByteRange& byte_range,
    InitializeCompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  init_callback_ = std::move(callback);
  GetFileContent(
      file_system_getter_, drive_file_path,
      base::Bind(
          &DriveFileStreamReader ::InitializeAfterGetFileContentInitialized,
          weak_ptr_factory_.GetWeakPtr(), byte_range),
      base::Bind(&DriveFileStreamReader::OnGetContent,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DriveFileStreamReader::OnGetFileContentCompletion,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DriveFileStreamReader::StoreCancelDownloadClosure,
                 weak_ptr_factory_.GetWeakPtr()));
}

int DriveFileStreamReader::Read(net::IOBuffer* buffer,
                                int buffer_length,
                                net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(reader_proxy_);
  DCHECK(buffer);
  DCHECK(callback);
  return reader_proxy_->Read(buffer, buffer_length, std::move(callback));
}

void DriveFileStreamReader::StoreCancelDownloadClosure(
    const base::Closure& cancel_download_closure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cancel_download_closure_ = cancel_download_closure;
}

void DriveFileStreamReader::InitializeAfterGetFileContentInitialized(
    const net::HttpByteRange& byte_range,
    FileError error,
    const base::FilePath& local_cache_file_path,
    std::unique_ptr<ResourceEntry> entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // StoreCancelDownloadClosure() should be called before this function.
  DCHECK(!cancel_download_closure_.is_null());

  if (error != FILE_ERROR_OK) {
    std::move(init_callback_)
        .Run(FileErrorToNetError(error), std::unique_ptr<ResourceEntry>());
    return;
  }
  DCHECK(entry);

  int64_t range_start = 0, range_length = 0;
  if (!ComputeConcretePosition(byte_range, entry->file_info().size(),
                               &range_start, &range_length)) {
    // If |byte_range| is invalid (e.g. out of bounds), return with an error.
    // At the same time, we cancel the in-flight downloading operation if
    // needed and and invalidate weak pointers so that we won't
    // receive unwanted callbacks.
    cancel_download_closure_.Run();
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(init_callback_)
        .Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
             std::unique_ptr<ResourceEntry>());
    return;
  }

  if (local_cache_file_path.empty()) {
    // The file is not cached, and being downloaded.
    reader_proxy_ = std::make_unique<internal::NetworkReaderProxy>(
        range_start, range_length, entry->file_info().size(),
        cancel_download_closure_);
    std::move(init_callback_).Run(net::OK, std::move(entry));
    return;
  }

  // Otherwise, open the stream for file.
  std::unique_ptr<util::LocalFileReader> file_reader(
      new util::LocalFileReader(file_task_runner_.get()));
  util::LocalFileReader* file_reader_ptr = file_reader.get();
  file_reader_ptr->Open(
      local_cache_file_path,
      range_start,
      base::Bind(
          &DriveFileStreamReader::InitializeAfterLocalFileOpen,
          weak_ptr_factory_.GetWeakPtr(),
          range_length,
          base::Passed(&entry),
          base::Passed(&file_reader)));
}

void DriveFileStreamReader::InitializeAfterLocalFileOpen(
    int64_t length,
    std::unique_ptr<ResourceEntry> entry,
    std::unique_ptr<util::LocalFileReader> file_reader,
    int open_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (open_result != net::OK) {
    std::move(init_callback_)
        .Run(net::ERR_FAILED, std::unique_ptr<ResourceEntry>());
    return;
  }

  reader_proxy_ = std::make_unique<internal::LocalReaderProxy>(
      std::move(file_reader), length);
  std::move(init_callback_).Run(net::OK, std::move(entry));
}

void DriveFileStreamReader::OnGetContent(
    google_apis::DriveApiErrorCode error_code,
    std::unique_ptr<std::string> data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(reader_proxy_);
  reader_proxy_->OnGetContent(std::move(data));
}

void DriveFileStreamReader::OnGetFileContentCompletion(
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (reader_proxy_) {
    // If the proxy object available, send the error to it.
    reader_proxy_->OnCompleted(error);
  } else {
    // Here the proxy object is not yet available.
    // There are two cases. 1) Some error happens during the initialization.
    // 2) the cache file is found, but the proxy object is not *yet*
    // initialized because the file is being opened.
    // We are interested in 1) only. The callback for 2) will be called
    // after opening the file is completed.
    // Note: due to the same reason, LocalReaderProxy::OnCompleted may
    // or may not be called. This is timing issue, and it is difficult to avoid
    // unfortunately.
    if (error != FILE_ERROR_OK) {
      std::move(init_callback_)
          .Run(FileErrorToNetError(error), std::unique_ptr<ResourceEntry>());
    }
  }
}

}  // namespace drive
