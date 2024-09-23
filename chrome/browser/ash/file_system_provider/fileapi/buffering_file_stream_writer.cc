// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/fileapi/buffering_file_stream_writer.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace ash::file_system_provider {

BufferingFileStreamWriter::BufferingFileStreamWriter(
    std::unique_ptr<storage::FileStreamWriter> file_stream_writer,
    int intermediate_buffer_length)
    : file_stream_writer_(std::move(file_stream_writer)),
      intermediate_buffer_length_(intermediate_buffer_length),
      intermediate_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
          intermediate_buffer_length_)),
      buffered_bytes_(0) {}

BufferingFileStreamWriter::~BufferingFileStreamWriter() {
  if (buffered_bytes_)
    LOG(ERROR) << "File stream writer not flushed. Data will be lost.";
}

int BufferingFileStreamWriter::Write(net::IOBuffer* buffer,
                                     int buffer_length,
                                     net::CompletionOnceCallback callback) {
  // If |buffer_length| is larger than the intermediate buffer, then call the
  // inner file stream writer directly. Note, that the intermediate buffer
  // (used for buffering) must be flushed first.
  if (buffer_length > intermediate_buffer_length_) {
    if (buffered_bytes_) {
      FlushIntermediateBuffer(base::BindOnce(
          &BufferingFileStreamWriter::
              OnFlushIntermediateBufferForDirectWriteCompleted,
          weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(buffer),
          buffer_length, std::move(callback)));
    } else {
      // Nothing to flush, so skip it.
      OnFlushIntermediateBufferForDirectWriteCompleted(
          base::WrapRefCounted(buffer), buffer_length, std::move(callback),
          net::OK);
    }
    return net::ERR_IO_PENDING;
  }

  // Buffer consecutive writes to larger chunks.
  const int buffer_bytes =
      std::min(intermediate_buffer_length_ - buffered_bytes_, buffer_length);

  CopyToIntermediateBuffer(base::WrapRefCounted(buffer), /*buffer_offset=*/0,
                           buffer_bytes);
  const int bytes_left = buffer_length - buffer_bytes;

  if (buffered_bytes_ == intermediate_buffer_length_) {
    FlushIntermediateBuffer(base::BindOnce(
        &BufferingFileStreamWriter::
            OnFlushIntermediateBufferForBufferedWriteCompleted,
        weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(buffer),
        buffer_bytes, bytes_left, std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  // Optimistically return a success.
  return buffer_length;
}

int BufferingFileStreamWriter::Cancel(net::CompletionOnceCallback callback) {
  // Since there is no any asynchronous call in this class other than on
  // |file_stream_writer_|, then there must be an in-flight operation going on.
  return file_stream_writer_->Cancel(std::move(callback));
}

int BufferingFileStreamWriter::Flush(storage::FlushMode flush_mode,
                                     net::CompletionOnceCallback callback) {
  // Flush all the buffered bytes first, then invoke Flush() on the inner file
  // stream writer.
  FlushIntermediateBuffer(base::BindOnce(
      &BufferingFileStreamWriter::OnFlushIntermediateBufferForFlushCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), flush_mode));
  return net::ERR_IO_PENDING;
}

void BufferingFileStreamWriter::CopyToIntermediateBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_offset,
    int buffer_length) {
  DCHECK_GE(intermediate_buffer_length_, buffer_length + buffered_bytes_);
  memcpy(intermediate_buffer_->data() + buffered_bytes_,
         buffer->data() + buffer_offset,
         buffer_length);
  buffered_bytes_ += buffer_length;
}

void BufferingFileStreamWriter::FlushIntermediateBuffer(
    net::CompletionOnceCallback callback) {
  const int result = file_stream_writer_->Write(
      intermediate_buffer_.get(), buffered_bytes_,
      base::BindOnce(
          &BufferingFileStreamWriter::OnFlushIntermediateBufferCompleted,
          weak_ptr_factory_.GetWeakPtr(), buffered_bytes_,
          std::move(callback)));
  DCHECK_EQ(net::ERR_IO_PENDING, result);
}

void BufferingFileStreamWriter::OnFlushIntermediateBufferCompleted(
    int length,
    net::CompletionOnceCallback callback,
    int result) {
  if (result < 0) {
    std::move(callback).Run(result);
    return;
  }

  DCHECK_EQ(length, result) << "Partial writes are not supported.";
  buffered_bytes_ = 0;

  std::move(callback).Run(net::OK);
}

void BufferingFileStreamWriter::
    OnFlushIntermediateBufferForDirectWriteCompleted(
        scoped_refptr<net::IOBuffer> buffer,
        int length,
        net::CompletionOnceCallback callback,
        int result) {
  if (result < 0) {
    std::move(callback).Run(result);
    return;
  }

  // The following logic is only valid if the intermediate buffer is empty.
  DCHECK_EQ(0, buffered_bytes_);

  const int write_result =
      file_stream_writer_->Write(buffer.get(), length, std::move(callback));
  DCHECK_EQ(net::ERR_IO_PENDING, write_result);
}

void BufferingFileStreamWriter::
    OnFlushIntermediateBufferForBufferedWriteCompleted(
        scoped_refptr<net::IOBuffer> buffer,
        int buffered_bytes,
        int bytes_left,
        net::CompletionOnceCallback callback,
        int result) {
  if (result < 0) {
    std::move(callback).Run(result);
    return;
  }

  // Copy the rest of bytes to the buffer.
  DCHECK_EQ(net::OK, result);
  DCHECK_EQ(0, buffered_bytes_);
  DCHECK_GE(intermediate_buffer_length_, bytes_left);
  CopyToIntermediateBuffer(buffer, buffered_bytes, bytes_left);

  std::move(callback).Run(buffered_bytes + bytes_left);
}

void BufferingFileStreamWriter::OnFlushIntermediateBufferForFlushCompleted(
    net::CompletionOnceCallback callback,
    storage::FlushMode flush_mode,
    int result) {
  if (result < 0) {
    std::move(callback).Run(result);
    return;
  }

  const int flush_result =
      file_stream_writer_->Flush(flush_mode, std::move(callback));
  DCHECK_EQ(net::ERR_IO_PENDING, flush_result);
}

}  // namespace ash::file_system_provider
