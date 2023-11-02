// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/snapshot_file_details.h"

#include <limits>

#include "base/numerics/safe_conversions.h"

////////////////////////////////////////////////////////////////////////////////
//                             SnapshotRequestInfo                            //
////////////////////////////////////////////////////////////////////////////////

SnapshotRequestInfo::SnapshotRequestInfo(
    uint32_t file_id,
    const base::FilePath& snapshot_file_path,
    MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback success_callback,
    MTPDeviceAsyncDelegate::ErrorCallback error_callback)
    : file_id(file_id),
      snapshot_file_path(snapshot_file_path),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

SnapshotRequestInfo::SnapshotRequestInfo(SnapshotRequestInfo&& other) = default;

SnapshotRequestInfo::~SnapshotRequestInfo() = default;

////////////////////////////////////////////////////////////////////////////////
//                             SnapshotFileDetails                            //
////////////////////////////////////////////////////////////////////////////////

SnapshotFileDetails::SnapshotFileDetails(SnapshotRequestInfo request_info,
                                         const base::File::Info& file_info)
    : request_info_(std::move(request_info)),
      file_info_(file_info),
      bytes_written_(0),
      error_occurred_(false) {}

SnapshotFileDetails::~SnapshotFileDetails() = default;

void SnapshotFileDetails::set_error_occurred(bool error) {
  error_occurred_ = error;
}

bool SnapshotFileDetails::AddBytesWritten(uint32_t bytes_written) {
  if ((bytes_written == 0) ||
      (bytes_written_ > std::numeric_limits<uint32_t>::max() - bytes_written) ||
      (bytes_written_ + bytes_written > file_info_.size))
    return false;

  bytes_written_ += bytes_written;
  return true;
}

bool SnapshotFileDetails::IsSnapshotFileWriteComplete() const {
  return !error_occurred_ && (bytes_written_ == file_info_.size);
}

uint32_t SnapshotFileDetails::BytesToRead() const {
  // Read data in 1MB chunks.
  static const uint32_t kReadChunkSize = 1024 * 1024;
  return std::min(
      kReadChunkSize,
      base::checked_cast<uint32_t>(file_info_.size) - bytes_written_);
}
