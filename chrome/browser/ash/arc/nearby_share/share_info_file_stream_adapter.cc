// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_uma.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

namespace arc {

ShareInfoFileStreamAdapter::ShareInfoFileStreamAdapter(
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t file_size,
    int buf_size,
    base::ScopedFD dest_fd,
    ResultCallback result_callback)
    : context_(context),
      url_(url),
      offset_(offset),
      bytes_remaining_(file_size),
      dest_fd_(std::move(dest_fd)),
      result_callback_(std::move(result_callback)),
      net_iobuf_(base::MakeRefCounted<net::IOBufferWithSize>(buf_size)) {
  DCHECK(url_.is_valid());
  DCHECK(dest_fd_.is_valid());
  DCHECK_GT(net_iobuf_->size(), 0);
}

void ShareInfoFileStreamAdapter::StartRunner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      // USER_BLOCKING because of downing files requested by the user and
      // the completion of these tasks will prevent UI from displaying.
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       // The tasks posted by |this| task runner should not block shutdown.
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ShareInfoFileStreamAdapter::StartFileStreaming, this));
}

void ShareInfoFileStreamAdapter::StartRunnerForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()});
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShareInfoFileStreamAdapter::StartFileStreaming,
                                base::Unretained(this)));
}

ShareInfoFileStreamAdapter::~ShareInfoFileStreamAdapter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // There was an abort mid-operation and did not complete streaming.
  if (result_callback_)
    OnStreamingFinished(false);
}

void ShareInfoFileStreamAdapter::StartFileStreaming() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  stream_reader_ = context_->CreateFileStreamReader(
      url_, offset_, bytes_remaining_, base::Time());
  if (!stream_reader_) {
    LOG(ERROR) << "Failed to create FileStreamReader.";
    UpdateNearbyShareIOFail(IOErrorResult::kFileReaderFailed);
    OnStreamingFinished(false);
    return;
  }

  PerformReadFileStream();
}

void ShareInfoFileStreamAdapter::PerformReadFileStream() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (bytes_remaining_ == 0) {
    OnStreamingFinished(true);
    return;
  }

  // Read only the minimum between the bytes remaining or the IO buffer size.
  const int bytes_read = stream_reader_->Read(
      net_iobuf_.get(), std::min<int64_t>(net_iobuf_->size(), bytes_remaining_),
      base::BindOnce(&ShareInfoFileStreamAdapter::OnReadFile,
                     weak_ptr_factory_.GetWeakPtr()));

  // Make sure we don't have pending IO before continuing to read virtual file.
  if (bytes_read != net::ERR_IO_PENDING) {
    OnReadFile(bytes_read);
  }
}

void ShareInfoFileStreamAdapter::WriteToFile(int bytes_read) {
  DCHECK(dest_fd_.is_valid());
  DCHECK_GT(bytes_read, 0);

  auto write_fd_func = base::BindOnce(
      [](int fd, scoped_refptr<net::IOBuffer> buf, int size) -> bool {
        const bool result =
            base::WriteFileDescriptor(fd, std::string_view(buf->data(), size));
        PLOG_IF(ERROR, !result) << "Failed writing to fd.";
        return result;
      },
      dest_fd_.get(), net_iobuf_, bytes_read);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(write_fd_func),
      base::BindOnce(&ShareInfoFileStreamAdapter::OnWriteFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShareInfoFileStreamAdapter::OnReadFile(int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (bytes_read < 0) {
    LOG(ERROR) << " Reached EOF even though there are remaining bytes: "
               << bytes_remaining_;
    UpdateNearbyShareIOFail(IOErrorResult::kEOFWithRemainingBytes);
    OnStreamingFinished(false);
    return;
  }
  if (bytes_read == 0) {
    LOG(ERROR) << "Read failed with error: " << net::ErrorToString(bytes_read);
    UpdateNearbyShareIOFail(IOErrorResult::kReadFailed);
    OnStreamingFinished(false);
    return;
  }
  bytes_remaining_ -= bytes_read;
  DCHECK_GE(bytes_remaining_, 0);

  if (dest_fd_.is_valid()) {
    WriteToFile(bytes_read);
  } else {
    LOG(ERROR) << "Unexpected could not find valid endpoint for streamed data.";
    UpdateNearbyShareIOFail(IOErrorResult::kStreamedDataInvalidEndpoint);
    OnStreamingFinished(false);
  }
}

void ShareInfoFileStreamAdapter::OnWriteFinished(bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!result) {
    OnStreamingFinished(false);
    return;
  }

  // Finish reading rest of the file.
  PerformReadFileStream();
}

void ShareInfoFileStreamAdapter::OnStreamingFinished(bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(result_callback_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback_), result));
}

}  // namespace arc
