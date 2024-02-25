// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/copy_from_fd.h"

#include "base/posix/eintr_wrapper.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"

namespace ash {

namespace {

int ReadFromScopedFDOffTheIOThread(int fd,
                                   scoped_refptr<net::IOBuffer> buffer) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  char* ptr = buffer->data();
  int len = buffer->size();
  ssize_t n = HANDLE_EINTR(read(fd, ptr, len));
  return (n >= 0) ? static_cast<int>(n)
                  : static_cast<int>(net::Error::ERR_FAILED);
}

class FromFDCopier {
 public:
  FromFDCopier(base::ScopedFD src_scoped_fd,
               std::unique_ptr<storage::FileStreamWriter> dst_fs_writer,
               storage::FlushPolicy dst_flush_policy,
               CopyFromFileDescriptorCallback callback);

  void CallRead();

 private:
  // For the "int result" arguments: If negative, it's a net error code. If
  // non-negative, it's the number of bytes read.
  void OnRead(int result);
  void CallWrite(int result);
  void MaybeFlush(int result);
  void Finish(int result);

  static constexpr size_t kBufferLen = 65536;

  base::ScopedFD scoped_fd_;
  std::unique_ptr<storage::FileStreamWriter> fs_writer_;
  storage::FlushPolicy dst_flush_policy_;
  CopyFromFileDescriptorCallback callback_;

  scoped_refptr<net::IOBuffer> buffer_;
  scoped_refptr<net::DrainableIOBuffer> drainable_buffer_;
};

FromFDCopier::FromFDCopier(
    base::ScopedFD src_scoped_fd,
    std::unique_ptr<storage::FileStreamWriter> dst_fs_writer,
    storage::FlushPolicy dst_flush_policy,
    CopyFromFileDescriptorCallback callback)
    : scoped_fd_(std::move(src_scoped_fd)),
      fs_writer_(std::move(dst_fs_writer)),
      dst_flush_policy_(dst_flush_policy),
      callback_(std::move(callback)),
      buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kBufferLen)) {}

void FromFDCopier::CallRead() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadFromScopedFDOffTheIOThread, scoped_fd_.get(),
                     buffer_),
      base::BindOnce(&FromFDCopier::OnRead,
                     // base::Unretained is safe because |this| isn't deleted
                     // until Finish is called.
                     base::Unretained(this)));
}

void FromFDCopier::OnRead(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK(!drainable_buffer_);

  if (result <= 0) {
    MaybeFlush(result);
    return;
  }
  drainable_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
      buffer_, static_cast<size_t>(result));
  CallWrite(0);
}

void FromFDCopier::CallWrite(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK(drainable_buffer_);

  while (true) {
    if (result < 0) {
      MaybeFlush(result);
      return;
    }

    drainable_buffer_->DidConsume(result);
    if (drainable_buffer_->BytesRemaining() <= 0) {
      drainable_buffer_.reset();
      CallRead();
      return;
    }

    result = fs_writer_->Write(
        drainable_buffer_.get(), drainable_buffer_->BytesRemaining(),
        base::BindOnce(&FromFDCopier::CallWrite,
                       // base::Unretained is safe because |this|
                       // isn't deleted until Finish is called.
                       base::Unretained(this)));
    if (result == net::ERR_IO_PENDING) {  // The write was asynchronous.
      return;
    }
  }
}

void FromFDCopier::MaybeFlush(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if ((result >= 0) &&
      (dst_flush_policy_ == storage::FlushPolicy::FLUSH_ON_COMPLETION)) {
    result = fs_writer_->Flush(
        storage::FlushMode::kEndOfFile,
        base::BindOnce(&FromFDCopier::Finish,
                       // base::Unretained is safe because |this|
                       // isn't deleted until Finish is called.
                       base::Unretained(this)));
    if (result == net::ERR_IO_PENDING) {  // The flush was asynchronous.
      return;
    }
  }

  Finish(result);
}

void FromFDCopier::Finish(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback_).Run(
      std::move(scoped_fd_), std::move(fs_writer_),
      (result >= 0) ? net::OK : static_cast<net::Error>(result));

  delete this;
}

}  // namespace

void CopyFromFileDescriptor(
    base::ScopedFD src_scoped_fd,
    std::unique_ptr<storage::FileStreamWriter> dst_fs_writer,
    storage::FlushPolicy dst_flush_policy,
    CopyFromFileDescriptorCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!callback) {
    return;
  } else if (!src_scoped_fd.is_valid() || !dst_fs_writer) {
    std::move(callback).Run(std::move(src_scoped_fd), std::move(dst_fs_writer),
                            net::Error::ERR_INVALID_ARGUMENT);
    return;
  }

  // This new-ly created object is deleted in FromFDCopier::Finish.
  FromFDCopier* copier =
      new FromFDCopier(std::move(src_scoped_fd), std::move(dst_fs_writer),
                       dst_flush_policy, std::move(callback));

  copier->CallRead();
}

}  // namespace ash
