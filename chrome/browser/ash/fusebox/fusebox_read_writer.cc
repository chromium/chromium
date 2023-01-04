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
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace fusebox {

namespace {

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
                              base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Write2ResponseProto response_proto;
  response_proto.set_posix_error_code(FileErrorToErrno(error_code));
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

}  // namespace

ReadWriter::ReadWriter(const storage::FileSystemURL& fs_url,
                       const std::string& profile_path,
                       bool use_temp_file)
    : fs_url_(fs_url),
      profile_path_(profile_path),
      use_temp_file_(use_temp_file) {
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

  bool trivial = closed_ || !use_temp_file_;
  closed_ = true;
  if (trivial) {
    Close2ResponseProto response_proto;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response_proto));
    return;
  }

  close2_fs_context_ = std::move(fs_context);
  close2_callback_ = std::move(callback);
  if (!is_loaning_temp_file_scoped_fd_) {
    Save();
  }
}

void ReadWriter::Save() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(close2_fs_context_);
  DCHECK(close2_callback_);
  DCHECK(!is_loaning_temp_file_scoped_fd_);
  DCHECK(use_temp_file_);

  std::string src_path =
      created_temp_file_
          ? base::StringPrintf("/proc/self/fd/%d", temp_file_.get())
          : "/dev/null";

  constexpr auto outer_callback =
      [](base::ScopedFD scoped_fd,
         scoped_refptr<storage::FileSystemContext> fs_context,
         Close2Callback callback, base::File::Error file_error) {
        Close2ResponseProto response_proto;
        if (file_error != base::File::Error::FILE_OK) {
          response_proto.set_posix_error_code(FileErrorToErrno(file_error));
        }
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), response_proto));
      };

  close2_fs_context_->operation_runner()->CopyInForeignFile(
      base::FilePath(src_path), fs_url_,
      base::BindOnce(outer_callback, std::move(temp_file_), close2_fs_context_,
                     std::move(close2_callback_)));
}

void ReadWriter::Read(scoped_refptr<storage::FileSystemContext> fs_context,
                      int64_t offset,
                      int64_t length,
                      Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!is_in_flight_);

  if (closed_) {
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
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&RunRead2CallbackFailure, std::move(callback),
                         base::File::Error::FILE_ERROR_INVALID_URL));
      return;
    }
  }

  constexpr int64_t min_length = 256;
  constexpr int64_t max_length = 262144;  // 256 KiB.
  scoped_refptr<net::IOBuffer> buffer = base::MakeRefCounted<net::IOBuffer>(
      std::max(min_length, std::min(max_length, length)));

  // Save the pointer before we std::move fs_reader into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamReader alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamReader would get destroyed at the end of this function.
  auto* saved_fs_reader = fs_reader.get();

  is_in_flight_ = true;
  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnRead, weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      fs_context, std::move(fs_reader), buffer, offset));

  int result =
      saved_fs_reader->Read(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The read was synchronous.
    std::move(pair.second).Run(result);
  }
}

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

  if (closed_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RunWrite2CallbackFailure, std::move(callback),
                       base::File::Error::FILE_ERROR_FAILED));
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
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&RunWrite2CallbackFailure, std::move(callback),
                           base::File::Error::FILE_ERROR_NO_SPACE));
        return;
      }
      created_temp_file_ = true;
    }

    is_in_flight_ = true;
    is_loaning_temp_file_scoped_fd_ = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&WriteTempFile, std::move(temp_file_), std::move(buffer),
                       offset, length),
        base::BindOnce(&OnWriteTempFile, weak_ptr_factory_.GetWeakPtr(),
                       std::move(callback)));
    return;
  }

  // See if we can re-use the previous storage::FileStreamWriter.
  std::unique_ptr<storage::FileStreamWriter> fs_writer;
  if (fs_writer_ && (write_offset_ == offset)) {
    fs_writer = std::move(fs_writer_);
    write_offset_ = -1;
  } else {
    fs_writer = fs_context->CreateFileStreamWriter(fs_url_, offset);
    if (!fs_writer) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&RunWrite2CallbackFailure, std::move(callback),
                         base::File::Error::FILE_ERROR_INVALID_URL));
      return;
    }
  }

  // Save the pointer before we std::move fs_writer into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamWriter alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamWriter would get destroyed at the end of this function.
  auto* saved_fs_writer = fs_writer.get();

  is_in_flight_ = true;
  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnWriteDirect, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), fs_context, std::move(fs_writer), buffer, offset));

  int result =
      saved_fs_writer->Write(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The write was synchronous.
    std::move(pair.second).Run(result);
  }
}

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
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response_proto));

  if (self->close2_callback_) {
    self->Save();
  }
}

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
  } else {
    self->fs_writer_.reset();
    self->write_offset_ = -1;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RunWrite2CallbackTypical, std::move(callback), length));
}

}  // namespace fusebox
