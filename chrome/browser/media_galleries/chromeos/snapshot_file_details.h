// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_SNAPSHOT_FILE_DETAILS_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_SNAPSHOT_FILE_DETAILS_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

// Used to represent snapshot file request params.
struct SnapshotRequestInfo {
  SnapshotRequestInfo(
      uint32_t file_id,
      const base::FilePath& snapshot_file_path,
      const MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback&
          success_callback,
      const MTPDeviceAsyncDelegate::ErrorCallback& error_callback);
  SnapshotRequestInfo(const SnapshotRequestInfo& other);
  ~SnapshotRequestInfo();

  // MTP device file id.
  const uint32_t file_id;

  // Local platform path of the snapshot file.
  const base::FilePath snapshot_file_path;

  // A callback to be called when CreateSnapshotFile() succeeds.
  const MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback
      success_callback;

  // A callback to be called when CreateSnapshotFile() fails.
  const MTPDeviceAsyncDelegate::ErrorCallback error_callback;
};

// SnapshotFileDetails tracks the current state of the snapshot file (e.g how
// many bytes written to the snapshot file, source file details, snapshot file
// metadata information, etc).
class SnapshotFileDetails {
 public:
  SnapshotFileDetails(const SnapshotRequestInfo& request_info,
                      const base::File::Info& file_info);

  ~SnapshotFileDetails();

  uint32_t file_id() const { return request_info_.file_id; }

  base::FilePath snapshot_file_path() const {
    return request_info_.snapshot_file_path;
  }

  uint32_t bytes_written() const { return bytes_written_; }

  const base::File::Info& file_info() const {
    return file_info_;
  }

  const MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback
      success_callback() const {
    return request_info_.success_callback;
  }

  const MTPDeviceAsyncDelegate::ErrorCallback error_callback() const {
    return request_info_.error_callback;
  }

  bool error_occurred() const {
    return error_occurred_;
  }

  void set_error_occurred(bool error);

  // Adds |bytes_written| to |bytes_written_|.
  // |bytes_written| specifies the total number of bytes transferred during the
  // last write operation.
  // If |bytes_written| is valid, returns true and adds |bytes_written| to
  // |bytes_written_|.
  // If |bytes_written| is invalid, returns false and does not add
  // |bytes_written| to |bytes_written_|.
  bool AddBytesWritten(uint32_t bytes_written);

  // Returns true if the snapshot file is created successfully (no more write
  // operation is required to complete the snapshot file).
  bool IsSnapshotFileWriteComplete() const;

  uint32_t BytesToRead() const;

 private:
  // Snapshot file request params.
  const SnapshotRequestInfo request_info_;

  // Metadata of the snapshot file (such as name, size, type, etc).
  const base::File::Info file_info_;

  // Number of bytes written into the snapshot file.
  uint32_t bytes_written_;

  // Whether an error occurred during file transfer.
  bool error_occurred_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotFileDetails);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_SNAPSHOT_FILE_DETAILS_H_
