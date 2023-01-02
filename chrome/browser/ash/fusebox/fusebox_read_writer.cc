// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_read_writer.h"

#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

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

}  // namespace

ReadWriter::ReadWriter(const storage::FileSystemURL& fs_url) : fs_url_(fs_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ReadWriter::~ReadWriter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// Some functions (marked with a §) below, take an fs_context argument that
// looks unused, but we need to keep the storage::FileSystemContext reference
// alive until the callbacks are run.

void ReadWriter::Read(scoped_refptr<storage::FileSystemContext> fs_context,
                      int64_t offset,
                      int64_t length,
                      Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

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
    Read2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamReader> fs_reader,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (length >= 0) {
    fs_reader_ = std::move(fs_reader);
    read_offset_ = offset + length;
  } else {
    fs_reader_.reset();
    read_offset_ = -1;
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

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &ReadWriter::OnWrite, weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      fs_context, std::move(fs_writer), buffer, offset));

  int result =
      saved_fs_writer->Write(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The write was synchronous.
    std::move(pair.second).Run(result);
  }
}

void ReadWriter::OnWrite(
    Write2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (length >= 0) {
    fs_writer_ = std::move(fs_writer);
    write_offset_ = offset + length;
  } else {
    fs_writer_.reset();
    write_offset_ = -1;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RunWrite2CallbackTypical, std::move(callback), length));
}

}  // namespace fusebox
