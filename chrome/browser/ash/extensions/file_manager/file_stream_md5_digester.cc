// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/file_stream_md5_digester.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"

namespace drive {
namespace util {

namespace {

const int kMd5DigestBufferSize = 512 * 1024;  // 512 kB.

}  // namespace

FileStreamMd5Digester::FileStreamMd5Digester()
    : buffer_(
          base::MakeRefCounted<net::IOBufferWithSize>(kMd5DigestBufferSize)) {}

FileStreamMd5Digester::~FileStreamMd5Digester() = default;

void FileStreamMd5Digester::GetMd5Digest(
    std::unique_ptr<storage::FileStreamReader> stream_reader,
    ResultCallback callback) {
  // Only one digest can be running at a time.
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);
  reader_ = std::move(stream_reader);
  base::MD5Init(&md5_context_);

  // Start the read/hash.
  ReadNextChunk();
}

void FileStreamMd5Digester::ReadNextChunk() {
  const int result =
      reader_->Read(buffer_.get(), kMd5DigestBufferSize,
                    base::BindOnce(&FileStreamMd5Digester::OnChunkRead, this));
  if (result != net::ERR_IO_PENDING) {
    OnChunkRead(result);
  }
}

void FileStreamMd5Digester::OnChunkRead(int bytes_read) {
  if (bytes_read < 0) {
    // Error - just return empty string.
    std::move(callback_).Run("");
    return;
  } else if (bytes_read == 0) {
    // EOF.
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context_);
    std::string result = base::MD5DigestToBase16(digest);
    std::move(callback_).Run(std::move(result));
    return;
  }

  // Read data and digest it.
  base::MD5Update(&md5_context_, std::string_view(buffer_->data(), bytes_read));

  // Kick off the next read.
  ReadNextChunk();
}

}  // namespace util
}  // namespace drive
