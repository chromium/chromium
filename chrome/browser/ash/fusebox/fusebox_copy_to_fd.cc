// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_copy_to_fd.h"

#include "base/posix/eintr_wrapper.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace fusebox {

namespace {

using Expected = base::expected<base::ScopedFD, int>;

Expected WriteToScopedFDOffTheIOThread(base::ScopedFD scoped_fd,
                                       scoped_refptr<net::IOBuffer> buffer,
                                       int length) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int fd = scoped_fd.get();
  char* ptr = buffer->data();
  if (HANDLE_EINTR(write(fd, ptr, static_cast<size_t>(length))) == -1) {
    return base::unexpected(errno);
  }
  return std::move(scoped_fd);
}

class FDCopier {
 public:
  FDCopier(scoped_refptr<storage::FileSystemContext> fs_context,
           const storage::FileSystemURL& fs_url,
           base::ScopedFD scoped_fd,
           base::OnceCallback<void(Expected)> callback);

  void CallRead();

 private:
  // The int is *not* a POSIX error code. If negative, it's a net error code.
  // If non-negative, it's the number of bytes read.
  void OnRead(int result);
  void OnWrite(Expected result);
  void Finish(Expected result);

  static constexpr size_t kBufferLen = 65536;

  std::unique_ptr<storage::FileStreamReader> fs_reader_;
  scoped_refptr<net::IOBuffer> buffer_;
  base::ScopedFD scoped_fd_;
  base::OnceCallback<void(Expected)> callback_;
};

FDCopier::FDCopier(scoped_refptr<storage::FileSystemContext> fs_context,
                   const storage::FileSystemURL& fs_url,
                   base::ScopedFD scoped_fd,
                   base::OnceCallback<void(Expected)> callback)
    : fs_reader_(fs_context->CreateFileStreamReader(fs_url,
                                                    0,
                                                    INT64_MAX,
                                                    base::Time())),
      buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kBufferLen)),
      scoped_fd_(std::move(scoped_fd)),
      callback_(std::move(callback)) {}

void FDCopier::CallRead() {
  int read_result =
      fs_reader_->Read(buffer_.get(), kBufferLen,
                       base::BindOnce(&FDCopier::OnRead,
                                      // base::Unretained is safe because |this|
                                      // isn't deleted until Finish is called.
                                      base::Unretained(this)));
  if (read_result != net::ERR_IO_PENDING) {  // The read was synchronous.
    OnRead(read_result);
  }
}

void FDCopier::OnRead(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (result == 0) {
    Finish(std::move(scoped_fd_));
    return;
  } else if (result < 0) {
    Finish(base::unexpected(NetErrorToErrno(result)));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteToScopedFDOffTheIOThread, std::move(scoped_fd_),
                     buffer_, result),
      base::BindOnce(&FDCopier::OnWrite,
                     // base::Unretained is safe because |this| isn't deleted
                     // until Finish is called.
                     base::Unretained(this)));
}

void FDCopier::OnWrite(Expected result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!result.has_value()) {
    Finish(std::move(result));
    return;
  }
  scoped_fd_ = std::move(result.value());
  CallRead();
}

void FDCopier::Finish(Expected result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback_).Run(std::move(result));
  delete this;
}

}  // namespace

void CopyToFileDescriptor(scoped_refptr<storage::FileSystemContext> fs_context,
                          const storage::FileSystemURL& src_fs_url,
                          base::ScopedFD dst_scoped_fd,
                          base::OnceCallback<void(Expected)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // This new-ly created object is deleted in FDCopier::Finish.
  FDCopier* copier =
      new FDCopier(std::move(fs_context), src_fs_url, std::move(dst_scoped_fd),
                   std::move(callback));

  copier->CallRead();
}

}  // namespace fusebox
