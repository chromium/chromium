// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_read_writer.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/fusebox/fusebox_copy_to_fd.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace fusebox {

namespace {

void RunFlushCallback(ReadWriter::FlushCallback callback,
                      int posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FlushResponseProto response_proto;
  if (posix_error_code) {
    response_proto.set_posix_error_code(posix_error_code);
  }
  std::move(callback).Run(response_proto);
}

void RunRead2CallbackFailure(ReadWriter::Read2Callback callback,
                             base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Read2ResponseProto response_proto;
  response_proto.set_posix_error_code(FileErrorToErrno(error_code));
  std::move(callback).Run(response_proto);
}

void RunRead2CallbackTypical(ReadWriter::Read2Callback callback,
                             scoped_refptr<net::IOBuffer> buffer,
                             int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Read2ResponseProto response_proto;
  if (length < 0) {
    response_proto.set_posix_error_code(NetErrorToErrno(length));
  } else {
    *response_proto.mutable_data() = std::string(buffer->data(), length);
  }
  std::move(callback).Run(response_proto);

  content::GetIOThreadTaskRunner({})->ReleaseSoon(FROM_HERE, std::move(buffer));
}

void RunWrite2CallbackFailure(ReadWriter::Write2Callback callback,
                              int posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Write2ResponseProto response_proto;
  response_proto.set_posix_error_code(posix_error_code);
  std::move(callback).Run(response_proto);
}

void RunWrite2CallbackTypical(ReadWriter::Write2Callback callback, int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Write2ResponseProto response_proto;
  if (length < 0) {
    response_proto.set_posix_error_code(NetErrorToErrno(length));
  }
  std::move(callback).Run(response_proto);
}

ReadWriter::WriteTempFileResult WriteTempFile(
    base::ScopedFD&& scoped_fd,
    scoped_refptr<net::StringIOBuffer> buffer,
    int64_t offset,
    int length) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int fd = scoped_fd.get();
  char* ptr = buffer->data();
  if ((HANDLE_EINTR(lseek(fd, static_cast<off_t>(offset), SEEK_SET)) == -1) ||
      (HANDLE_EINTR(write(fd, ptr, static_cast<size_t>(length))) == -1)) {
    return std::make_pair(std::move(scoped_fd), errno);
  }
  return std::make_pair(std::move(scoped_fd), 0);
}

void SaveCallback2(base::ScopedFD scoped_fd,
                   scoped_refptr<storage::FileSystemContext> fs_context,
                   ReadWriter::Close2Callback callback,
                   base::File::Error file_error) {
  Close2ResponseProto response_proto;
  if (file_error != base::File::Error::FILE_OK) {
    response_proto.set_posix_error_code(FileErrorToErrno(file_error));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response_proto));
}

void SaveCallback1(const std::string src_path,
                   storage::FileSystemURL fs_url,
                   base::ScopedFD scoped_fd,
                   scoped_refptr<storage::FileSystemContext> fs_context,
                   ReadWriter::Close2Callback callback,
                   base::File::Error file_error) {
  // Ignore file_error. We're essentially doing "/usr/bin/rm -f" instead
  // of a bare "/usr/bin/rm".

  fs_context->operation_runner()->CopyInForeignFile(
      base::FilePath(src_path), fs_url,
      base::BindOnce(&SaveCallback2, std::move(scoped_fd), fs_context,
                     std::move(callback)));
}

using EOFFlushFsWriterCallback = base::OnceCallback<void(
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    int posix_error_code)>;

// Calls fs_writer->Flush(kEndOfFile), running the callback afterwards, if
// needs_eof_flushing is true. If false, it just runs the callback immediately.
//
// The fs_writer and write_offset are passed through to the callback.
void EOFFlushFsWriterIfNecessary(
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    int64_t write_offset,
    bool needs_eof_flushing,
    EOFFlushFsWriterCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!fs_writer || !needs_eof_flushing) {
    std::move(callback).Run(std::move(fs_writer), 0);
    return;
  }

  // Save the pointer before we std::move fs_writer into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamWriter alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamWriter would get destroyed at the end of this function.
  storage::FileStreamWriter* fs_writer_ptr = fs_writer.get();

  auto pair = base::SplitOnceCallback(base::BindOnce(
      [](std::unique_ptr<storage::FileStreamWriter> fs_writer,
         int64_t write_offset, EOFFlushFsWriterCallback callback, int result) {
        int posix_error_code = (result < 0) ? NetErrorToErrno(result) : 0;
        std::move(callback).Run(std::move(fs_writer), posix_error_code);
      },
      std::move(fs_writer), write_offset, std::move(callback)));

  int result = fs_writer_ptr->Flush(storage::FlushMode::kEndOfFile,
                                    std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The flush was synchronous.
    std::move(pair.second).Run(result);
  }
}

}  // namespace

ReadWriter::ReadWriter(const storage::FileSystemURL& fs_url,
                       const std::string& profile_path,
                       bool use_temp_file,
                       bool temp_file_starts_with_copy)
    : fs_url_(fs_url),
      profile_path_(profile_path),
      use_temp_file_(use_temp_file),
      temp_file_starts_with_copy_(temp_file_starts_with_copy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ReadWriter::~ReadWriter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// Some functions (marked with a §) below, take an fs_context argument that
// looks unused, but we need to keep the storage::FileSystemContext reference
// alive until the callbacks are run.

void ReadWriter::Close(scoped_refptr<storage::FileSystemContext> fs_context,
                       Close2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (closed_) {
    Close2ResponseProto response_proto;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }
  closed_ = true;

  EOFFlushFsWriterIfNecessary(
      std::move(fs_writer_), std::exchange(write_offset_, -1),
      std::exchange(fs_writer_needs_eof_flushing_, false),
      base::BindOnce(&ReadWriter::OnEOFFlushBeforeActualClose,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(fs_context)));
}

// static
void ReadWriter::OnEOFFlushBeforeActualClose(
    base::WeakPtr<ReadWriter> weak_ptr,
    Close2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    int flush_posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    flush_posix_error_code = EBUSY;
  }
  if (flush_posix_error_code) {
    Close2ResponseProto response_proto;
    response_proto.set_posix_error_code(flush_posix_error_code);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  if (!self->use_temp_file_) {
    Close2ResponseProto response_proto;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  self->close2_fs_context_ = std::move(fs_context);
  self->close2_callback_ = std::move(callback);
  if (!self->is_loaning_temp_file_scoped_fd_) {
    self->Save();
  }
}

void ReadWriter::Save() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(close2_fs_context_);
  DCHECK(close2_callback_);
  DCHECK(!is_loaning_temp_file_scoped_fd_);
  DCHECK(use_temp_file_);

  if (write_posix_error_code_ != 0) {
    Close2ResponseProto response_proto;
    response_proto.set_posix_error_code(write_posix_error_code_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(close2_callback_), response_proto));
    return;
  }

  std::string src_path =
      created_temp_file_
          ? base::StringPrintf("/proc/self/fd/%d", temp_file_.get())
          : "/dev/null";

  // Delete the file if it already it exists, or a no-op if it doesn't. Either
  // way, SaveCallback1 will then copy from src_path to fs_url_, before
  // SaveCallback2 runs the close2_callback_.
  close2_fs_context_->operation_runner()->RemoveFile(
      fs_url_,
      base::BindOnce(&SaveCallback1, src_path, fs_url_, std::move(temp_file_),
                     close2_fs_context_, std::move(close2_callback_)));
}

void ReadWriter::Flush(scoped_refptr<storage::FileSystemContext> fs_context,
                       FlushCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!is_in_flight_);
  is_in_flight_ = true;

  if (closed_) {
    is_in_flight_ = false;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RunFlushCallback, std::move(callback), EFAULT));
    return;
  }

  if (use_temp_file_ || !fs_writer_) {
    is_in_flight_ = false;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RunFlushCallback, std::move(callback), 0));
    return;
  }

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnDefaultFlush, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), fs_context));

  int result =
      fs_writer_->Flush(storage::FlushMode::kDefault, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The flush was synchronous.
    std::move(pair.second).Run(result);
  }
}

void ReadWriter::Read(scoped_refptr<storage::FileSystemContext> fs_context,
                      int64_t offset,
                      int64_t length,
                      Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!is_in_flight_);
  is_in_flight_ = true;

  if (closed_) {
    is_in_flight_ = false;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RunRead2CallbackFailure, std::move(callback),
                                  base::File::Error::FILE_ERROR_FAILED));
    return;
  }

  // See if we can re-use the previous storage::FileStreamReader.
  std::unique_ptr<storage::FileStreamReader> fs_reader;
  if (fs_reader_ && (read_offset_ == offset)) {
    fs_reader = std::move(fs_reader_);
    read_offset_ = -1;
  } else {
    fs_reader = fs_context->CreateFileStreamReader(fs_url_, offset, INT64_MAX,
                                                   base::Time());
    if (!fs_reader) {
      is_in_flight_ = false;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&RunRead2CallbackFailure, std::move(callback),
                         base::File::Error::FILE_ERROR_INVALID_URL));
      return;
    }
  }

  constexpr int64_t min_length = 256;
  constexpr int64_t max_length = 262144;  // 256 KiB.
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      std::max(min_length, std::min(max_length, length)));

  // Save the pointer before we std::move fs_reader into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamReader alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamReader would get destroyed at the end of this function.
  auto* saved_fs_reader = fs_reader.get();

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnRead, weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      fs_context, std::move(fs_reader), buffer, offset));

  int result =
      saved_fs_reader->Read(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The read was synchronous.
    std::move(pair.second).Run(result);
  }
}

// static
void ReadWriter::OnRead(
    base::WeakPtr<ReadWriter> weak_ptr,
    Read2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamReader> fs_reader,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    Read2ResponseProto response_proto;
    response_proto.set_posix_error_code(EBUSY);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  DCHECK(self->is_in_flight_);
  self->is_in_flight_ = false;

  if (length >= 0) {
    self->fs_reader_ = std::move(fs_reader);
    self->read_offset_ = offset + length;
  } else {
    self->fs_reader_.reset();
    self->read_offset_ = -1;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RunRead2CallbackTypical, std::move(callback),
                                std::move(buffer), length));
}

void ReadWriter::Write(scoped_refptr<storage::FileSystemContext> fs_context,
                       scoped_refptr<net::StringIOBuffer> buffer,
                       int64_t offset,
                       int length,
                       Write2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!is_in_flight_);
  is_in_flight_ = true;

  if (closed_ || (write_posix_error_code_ != 0)) {
    is_in_flight_ = false;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RunWrite2CallbackFailure, std::move(callback),
                       closed_ ? EFAULT : write_posix_error_code_));
    return;
  }

  if (use_temp_file_) {
    if (!temp_file_.is_valid()) {
      // Create the temporary file via open and O_TMPFILE, instead of
      // base::CreateTemporaryFile, to simplify clean-up. With the latter,
      // there is a garbage collection problem of when to delete no-longer-used
      // files (as base::File::DeleteOnClose is Windows only). That problem can
      // be tricky if Chrome crashes before we're done with the temporary file.
      // With O_TMPFILE, the kernel deletes the file automatically when the
      // file descriptor is closed, whether via base::ScopedFD destructor or,
      // for crashes, at process exit.
      temp_file_.reset(open(profile_path_.c_str(),
                            O_CLOEXEC | O_EXCL | O_TMPFILE | O_RDWR, 0600));
      if (!temp_file_.is_valid()) {
        PLOG(WARNING) << "could not create O_TMPFILE file";
        is_in_flight_ = false;
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&RunWrite2CallbackFailure,
                                      std::move(callback), ENOSPC));
        return;
      }
      created_temp_file_ = true;
      if (temp_file_starts_with_copy_) {
        auto outer_callback = base::BindOnce(
            OnTempFileInitialized, weak_ptr_factory_.GetWeakPtr(),
            std::move(buffer), offset, length, std::move(callback));
        is_loaning_temp_file_scoped_fd_ = true;
        CopyToFileDescriptor(fs_context, fs_url_, std::move(temp_file_),
                             std::move(outer_callback));
        return;
      }
    }

    CallWriteTempFile(weak_ptr_factory_.GetWeakPtr(), std::move(buffer), offset,
                      length, std::move(callback));
    return;
  }

  // See if we can re-use the previous storage::FileStreamWriter.
  //
  // If so, go on to CallWriteDirect immediately. Otherwise, go on to
  // EOFFlushFsWriterIfNecessary, OnEOFFlushBeforeCallWriteDirect and then
  // CallWriteDirect.
  if (fs_writer_ && (write_offset_ == offset)) {
    CallWriteDirect(std::move(callback), std::move(fs_context),
                    std::move(fs_writer_), std::move(buffer),
                    std::exchange(write_offset_, -1), length);
    return;
  }

  // We will need a new storage::FileStreamWriter, so destroy the previous one
  // saved to fs_writer_, if it is non-null. However, we might need to Flush it
  // (with FlushMode::kEndOfFile) before destroying it.
  //
  // Calling EOFFlushFsWriterIfNecessary, with std::move(fs_writer_) and with
  // OnEOFFlushBeforeCallWriteDirect, will Flush (if needed) and destroy that
  // FileStreamWriter (positioned at write_offset_), if it is non-null.
  //
  // Afterwards, OnEOFFlushBeforeCallWriteDirect creates a new FileStreamWriter
  // (positioned at offset) and passes that on to CallWriteDirect.
  EOFFlushFsWriterIfNecessary(
      std::move(fs_writer_), std::exchange(write_offset_, -1),
      std::exchange(fs_writer_needs_eof_flushing_, false),
      base::BindOnce(&ReadWriter::OnEOFFlushBeforeCallWriteDirect,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(fs_context), std::move(buffer), offset, length));
}

// static
void ReadWriter::OnTempFileInitialized(
    base::WeakPtr<ReadWriter> weak_ptr,
    scoped_refptr<net::StringIOBuffer> buffer,
    int64_t offset,
    int length,
    Write2Callback callback,
    base::expected<base::ScopedFD, int> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!result.has_value() || !self) {
    if (self) {
      self->is_in_flight_ = false;
    }
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(result.error_or(EBUSY));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  self->is_loaning_temp_file_scoped_fd_ = false;
  self->temp_file_ = std::move(result.value());
  CallWriteTempFile(std::move(weak_ptr), std::move(buffer), offset, length,
                    std::move(callback));
}

// static
void ReadWriter::CallWriteTempFile(base::WeakPtr<ReadWriter> weak_ptr,
                                   scoped_refptr<net::StringIOBuffer> buffer,
                                   int64_t offset,
                                   int length,
                                   Write2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EBUSY);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  self->is_loaning_temp_file_scoped_fd_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteTempFile, std::move(self->temp_file_),
                     std::move(buffer), offset, length),
      base::BindOnce(&OnWriteTempFile, std::move(weak_ptr),
                     std::move(callback)));
}

// static
void ReadWriter::OnWriteTempFile(base::WeakPtr<ReadWriter> weak_ptr,
                                 Write2Callback callback,
                                 WriteTempFileResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EBUSY);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  DCHECK(self->is_in_flight_);
  self->is_in_flight_ = false;
  self->is_loaning_temp_file_scoped_fd_ = false;

  self->temp_file_ = std::move(result.first);

  Write2ResponseProto response_proto;
  if (result.second) {
    response_proto.set_posix_error_code(result.second);
    self->write_posix_error_code_ = result.second;
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response_proto));

  if (self->close2_callback_) {
    self->Save();
  }
}

// static
void ReadWriter::OnDefaultFlush(
    base::WeakPtr<ReadWriter> weak_ptr,
    FlushCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    int flush_posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    flush_posix_error_code = EBUSY;
  } else {
    self->is_in_flight_ = false;
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RunFlushCallback, std::move(callback),
                                flush_posix_error_code));
}

// static
void ReadWriter::OnEOFFlushBeforeCallWriteDirect(
    base::WeakPtr<ReadWriter> weak_ptr,
    Write2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length,
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    int flush_posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    flush_posix_error_code = EBUSY;
  }
  if (flush_posix_error_code) {
    if (self) {
      self->is_in_flight_ = false;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RunWrite2CallbackFailure,
                                  std::move(callback), flush_posix_error_code));
    return;
  }

  fs_writer = fs_context->CreateFileStreamWriter(self->fs_url_, offset);
  if (!fs_writer) {
    self->is_in_flight_ = false;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RunWrite2CallbackFailure, std::move(callback), EINVAL));
    return;
  }

  self->CallWriteDirect(std::move(callback), std::move(fs_context),
                        std::move(fs_writer), std::move(buffer), offset,
                        length);
}

void ReadWriter::CallWriteDirect(
    Write2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Save the pointer before we std::move fs_writer into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamWriter alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamWriter would get destroyed at the end of this function.
  auto* saved_fs_writer = fs_writer.get();

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnWriteDirect, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), fs_context, std::move(fs_writer), buffer, offset));

  int result =
      saved_fs_writer->Write(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The write was synchronous.
    std::move(pair.second).Run(result);
  }
}

// static
void ReadWriter::OnWriteDirect(
    base::WeakPtr<ReadWriter> weak_ptr,
    Write2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ReadWriter* self = weak_ptr.get();
  if (!self) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EBUSY);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  DCHECK(self->is_in_flight_);
  self->is_in_flight_ = false;

  if (length >= 0) {
    self->fs_writer_ = std::move(fs_writer);
    self->write_offset_ = offset + length;
    self->fs_writer_needs_eof_flushing_ =
        self->fs_writer_needs_eof_flushing_ ||
        (self->fs_url_.mount_option().flush_policy() ==
         storage::FlushPolicy::FLUSH_ON_COMPLETION);
  } else {
    self->fs_writer_.reset();
    self->write_offset_ = -1;
    self->write_posix_error_code_ = NetErrorToErrno(length);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RunWrite2CallbackTypical, std::move(callback), length));
}

}  // namespace fusebox
