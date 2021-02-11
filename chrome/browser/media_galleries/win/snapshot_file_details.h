// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_SNAPSHOT_FILE_DETAILS_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_SNAPSHOT_FILE_DETAILS_H_

#include <wrl/client.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

// Structure used to represent snapshot file request params.
struct SnapshotRequestInfo {
  SnapshotRequestInfo(
      const base::FilePath& device_file_path,
      const base::FilePath& snapshot_file_path,
      const MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback&
          success_callback,
      const MTPDeviceAsyncDelegate::ErrorCallback& error_callback);
  SnapshotRequestInfo(const SnapshotRequestInfo& other);
  ~SnapshotRequestInfo();

  // Device file path.
  base::FilePath device_file_path;

  // Local platform path of the snapshot file.
  base::FilePath snapshot_file_path;

  // A callback to be called when CreateSnapshotFile() succeeds.
  MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback
      success_callback;

  // A callback to be called when CreateSnapshotFile() fails.
  MTPDeviceAsyncDelegate::ErrorCallback error_callback;
};

// Provides the details for the the creation of snapshot file.
class SnapshotFileDetails {
 public:
  explicit SnapshotFileDetails(const SnapshotRequestInfo& request_info);
  SnapshotFileDetails(const SnapshotFileDetails& other);
  ~SnapshotFileDetails();

  void set_file_info(const base::File::Info& file_info);
  void set_device_file_stream(IStream* file_stream);
  void set_optimal_transfer_size(DWORD optimal_transfer_size);

  SnapshotRequestInfo request_info() const {
    return request_info_;
  }

  base::File::Info file_info() const {
    return file_info_;
  }

  IStream* device_file_stream() const {
    return file_stream_.Get();
  }

  DWORD optimal_transfer_size() const {
    return optimal_transfer_size_;
  }

  // Returns true if the data contents of the device file is written to the
  // snapshot file.
  bool IsSnapshotFileWriteComplete() const;

  // Adds |bytes_written| to |bytes_written_|.
  // |bytes_written| specifies the total number of bytes transferred during
  // the last write operation.
  // If |bytes_written| is valid, returns true and adds |bytes_written| to
  // |bytes_written_|.
  // If |bytes_written| is invalid, returns false and does not add
  // |bytes_written| to |bytes_written_|.
  bool AddBytesWritten(DWORD bytes_written);

 private:
  // Snapshot file request params.
  SnapshotRequestInfo request_info_;

  // Metadata of the created snapshot file.
  base::File::Info file_info_;

  // Used to read the device file contents.
  Microsoft::WRL::ComPtr<IStream> file_stream_;

  // The number of bytes of data to read from the |file_stream| object
  // during each IStream::Read() operation.
  DWORD optimal_transfer_size_;

  // Total number of bytes written into the snapshot file.
  DWORD bytes_written_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_SNAPSHOT_FILE_DETAILS_H_
