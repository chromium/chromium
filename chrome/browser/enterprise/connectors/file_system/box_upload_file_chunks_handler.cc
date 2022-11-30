// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_upload_file_chunks_handler.h"

#include "base/base64.h"
#include "base/files/file.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {

const uint32_t kOpenFlag = base::File::FLAG_OPEN | base::File::FLAG_READ;

bool CheckChunkSize(const size_t chunks,
                    const size_t file_size,
                    const size_t chunk_size,
                    const size_t bytes_read) {
  if (chunks < file_size / chunk_size) {
    CHECK_EQ(bytes_read, chunk_size);
  } else {
    CHECK_GT(bytes_read, 0U);
    CHECK_LE(bytes_read, chunk_size);
  }
  return true;
}

}  // namespace

namespace enterprise_connectors {

using FileChunksHandler = BoxChunkedUploader::FileChunksHandler;

FileChunksHandler::FileChunksHandler(const base::FilePath& path,
                                     const size_t file_size,
                                     const size_t chunk_size)
    : sequenced_file_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      file_size_(file_size),
      chunk_size_(chunk_size) {
  sequenced_file_.AsyncCall(&base::File::Initialize).WithArgs(path, kOpenFlag);
}

FileChunksHandler::~FileChunksHandler() = default;

void FileChunksHandler::StartReading(
    FilePartiallyReadCallback file_partially_read_cb,
    FileCompletelyReadCallback file_completely_read_cb) {
  file_partially_read_cb_ = file_partially_read_cb;
  file_completely_read_cb_ = std::move(file_completely_read_cb);
  CheckFileError(base::BindOnce(&FileChunksHandler::ReadIfValid,
                                weak_factory_.GetWeakPtr()));
}

void FileChunksHandler::ReadIfValid(FileStatus file_status) {
  if (file_status != FileStatus::FILE_OK) {
    return OnFileCompletelyRead(file_status);
  }
  chunk_content_.resize(chunk_size_);
  base::SHA1Init(sha1_ctx_);
  Read();
}

void FileChunksHandler::Read() {
  auto bytes_remaining = file_size_ - byte_to_;
  DCHECK_GE(bytes_remaining, 0U);
  if (bytes_remaining == 0U) {
    return OnFileCompletelyRead(FileStatus::FILE_OK);
  } else if (bytes_remaining < chunk_content_.size()) {
    chunk_content_.resize(bytes_remaining);
  }

  byte_from_ = byte_to_;

  DCHECK(sequenced_file_);
  sequenced_file_.AsyncCall(&base::File::ReadAtCurrentPos)
      .WithArgs(&chunk_content_[0U], chunk_content_.size())
      .Then(base::BindOnce(&FileChunksHandler::OnFileChunkRead,
                           weak_factory_.GetWeakPtr()));
}

void FileChunksHandler::OnFileChunkRead(int bytes_read) {
  if (bytes_read > 0) {
    byte_to_ += bytes_read;
    ++chunks_read_;
    DCHECK(CheckChunkSize(chunks_read_, file_size_, chunk_size_,
                          static_cast<size_t>(bytes_read)));
    DCHECK_EQ(byte_to_, std::min(file_size_, chunks_read_ * chunk_size_));
    DCHECK_EQ(static_cast<size_t>(bytes_read), chunk_content_.size())
        << "at " << byte_to_ << " / total " << file_size_ << " bytes";

    base::SHA1Update(chunk_content_, sha1_ctx_);

    // Everywhere else in this class, byte_to_ is used to verify chunk size and
    // bytes already read, exclusive of the last byte read. However, the byte
    // ranges used to upload each part's content need to be non-overlapping.
    // Therefore, PartInfo::byte_to == byte_to_ = 1.
    size_t range_to = byte_to_ - 1;
    file_partially_read_cb_.Run(
        PartInfo{FileStatus::FILE_OK, chunk_content_, byte_from_, range_to});

  } else {
    DLOG(ERROR) << "Failed to read file chunk from byte " << byte_from_;
    OnFileError(base::BindOnce(&FileChunksHandler::OnFileCompletelyRead,
                               weak_factory_.GetWeakPtr()),
                FileStatus::FILE_ERROR_IO);
  }
}

void FileChunksHandler::OnFileCompletelyRead(FileStatus status) {
  sequenced_file_.Reset();

  if (status != FileStatus::FILE_OK) {
    LOG(ERROR) << "Terminating file read due to failure!";
    PartInfo info;
    info.error = status;
    file_partially_read_cb_.Run(std::move(info));
    return;
  }

  DCHECK_EQ(byte_to_, file_size_);

  // Finalize the file's SHA-1 digest and format it into base64 encoding as
  // required by box API.
  base::SHA1Digest sha1_digest;
  base::SHA1Final(sha1_ctx_, sha1_digest);
  auto digest_str = base::Base64Encode(base::span<const uint8_t>(sha1_digest));

  DCHECK(file_completely_read_cb_);
  std::move(file_completely_read_cb_).Run(digest_str);
}

void FileChunksHandler::ContinueToReadChunk(size_t n) {
  DCHECK_EQ(chunks_read_ + 1, n);
  Read();
}

void FileChunksHandler::CheckFileError(FileCheckCallback cb) {
  sequenced_file_.AsyncCall(&base::File::IsValid)
      .Then(base::BindOnce(&FileChunksHandler::OnFileChecked,
                           weak_factory_.GetWeakPtr(), std::move(cb)));
}

void FileChunksHandler::OnFileChecked(FileCheckCallback cb,
                                      bool file_valid) {
  if (!file_valid) {
    LOG(ERROR) << "File is invalid!";
    sequenced_file_.AsyncCall(&base::File::error_details)
        .Then(base::BindOnce(&FileChunksHandler::OnFileError,
                             weak_factory_.GetWeakPtr(), std::move(cb)));
  } else {
    std::move(cb).Run(FileStatus::FILE_OK);
  }
}

void FileChunksHandler::OnFileError(FileCheckCallback cb, FileStatus status) {
  LOG(ERROR) << "File Error: " << base::File::ErrorToString(status);
  sequenced_file_.Reset();
  std::move(cb).Run(status);
}

void FileChunksHandler::SkipToOnFileChunkReadForTesting(
    std::string content,
    int bytes_read,
    FilePartiallyReadCallback file_partially_read_cb,
    FileCompletelyReadCallback file_completely_read_cb) {
  chunk_content_ = content;
  file_partially_read_cb_ = file_partially_read_cb;
  file_completely_read_cb_ = std::move(file_completely_read_cb);
  OnFileChunkRead(bytes_read);
}

}  // namespace enterprise_connectors
