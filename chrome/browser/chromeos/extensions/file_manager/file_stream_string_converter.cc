// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_stream_string_converter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/dcheck_is_on.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace storage {

namespace {

constexpr int kFileBufferSize = 512 * 1024;  // 512 kB.

}  // namespace

FileStreamStringConverter::FileStreamStringConverter()
    : buffer_(base::MakeRefCounted<net::IOBuffer>(kFileBufferSize)) {}

FileStreamStringConverter::~FileStreamStringConverter() = default;

void FileStreamStringConverter::ConvertFileStreamToString(
    std::unique_ptr<storage::FileStreamReader> stream_reader,
    ResultCallback callback) {
  // Only one converter can be running at a time.
  DCHECK(callback_.is_null());
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback_ = std::move(callback);
  reader_ = std::move(stream_reader);

  // Start the read.
  ReadNextChunk();
}

void FileStreamStringConverter::ReadNextChunk() {
  const int result =
      reader_->Read(buffer_.get(), kFileBufferSize,
                    base::BindOnce(&FileStreamStringConverter::OnChunkRead,
                                   base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnChunkRead(result);
}

void FileStreamStringConverter::OnChunkRead(int bytes_read) {
  if (bytes_read < 0) {
    // Error - just return empty string.
    std::move(callback_).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  } else if (bytes_read == 0) {
    // EOF.
    std::move(callback_).Run(
        base::RefCountedString::TakeString(&local_buffer_));
    return;
  }

  local_buffer_.append(buffer_->data(), bytes_read);

  // Kick off the next read.
  ReadNextChunk();
}

}  // namespace storage
