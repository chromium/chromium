// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/win/snapshot_file_details.h"

#include <stdint.h>

#include <limits>

///////////////////////////////////////////////////////////////////////////////
//                       SnapshotRequestInfo                                 //
///////////////////////////////////////////////////////////////////////////////

SnapshotRequestInfo::SnapshotRequestInfo(
    const base::FilePath& device_file_path,
    const base::FilePath& snapshot_file_path,
    MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback success_callback,
    MTPDeviceAsyncDelegate::ErrorCallback error_callback)
    : device_file_path(device_file_path),
      snapshot_file_path(snapshot_file_path),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error_callback)) {}

SnapshotRequestInfo::SnapshotRequestInfo(SnapshotRequestInfo&& other) = default;

SnapshotRequestInfo::~SnapshotRequestInfo() = default;

///////////////////////////////////////////////////////////////////////////////
//                       SnapshotFileDetails                                 //
///////////////////////////////////////////////////////////////////////////////

SnapshotFileDetails::SnapshotFileDetails(SnapshotRequestInfo request_info)
    : request_info_(std::move(request_info)),
      optimal_transfer_size_(0),
      bytes_written_(0) {}

SnapshotFileDetails::~SnapshotFileDetails() {
  file_stream_.Reset();
}

void SnapshotFileDetails::set_file_info(const base::File::Info& file_info) {
  file_info_ = file_info;
}

void SnapshotFileDetails::set_device_file_stream(
    IStream* file_stream) {
  file_stream_ = file_stream;
}

void SnapshotFileDetails::set_optimal_transfer_size(
    DWORD optimal_transfer_size) {
  optimal_transfer_size_ = optimal_transfer_size;
}

bool SnapshotFileDetails::IsSnapshotFileWriteComplete() const {
  return bytes_written_ == file_info_.size;
}

bool SnapshotFileDetails::AddBytesWritten(DWORD bytes_written) {
  if ((bytes_written == 0) ||
      (bytes_written_ > std::numeric_limits<uint64_t>::max() - bytes_written) ||
      (bytes_written_ + bytes_written > file_info_.size))
    return false;

  bytes_written_ += bytes_written;
  return true;
}
