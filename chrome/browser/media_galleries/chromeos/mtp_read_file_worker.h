// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_READ_FILE_WORKER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_READ_FILE_WORKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

class SnapshotFileDetails;
struct SnapshotRequestInfo;

// Worker class to copy the contents of the media transfer protocol(MTP) device
// file to the given snapshot file.
class MTPReadFileWorker {
 public:
  explicit MTPReadFileWorker(const std::string& device_handle);

  MTPReadFileWorker(const MTPReadFileWorker&) = delete;
  MTPReadFileWorker& operator=(const MTPReadFileWorker&) = delete;

  ~MTPReadFileWorker();

  // Dispatches the request to MediaTransferProtocolManager to get the media
  // file contents.
  //
  // |request_info| specifies the snapshot file request params.
  // |snapshot_file_info| specifies the metadata of the snapshot file.
  void WriteDataIntoSnapshotFile(SnapshotRequestInfo request_info,
                                 const base::File::Info& snapshot_file_info);

 private:
  // Called when WriteDataIntoSnapshotFile() completes.
  //
  // |snapshot_file_details| contains the current state of the snapshot file
  // (such as how many bytes written to the snapshot file, media device file
  // path, snapshot file path, bytes remaining, etc).
  //
  // If there is an error, |snapshot_file_details.error_callback| is invoked on
  // the IO thread to notify the caller about the failure.
  //
  // If there is no error, |snapshot_file_details.success_callback| is invoked
  // on the IO thread to notify the caller about the success.
  void OnDidWriteIntoSnapshotFile(
      std::unique_ptr<SnapshotFileDetails> snapshot_file_details);

  // Dispatches the request to MediaTransferProtocolManager to get the device
  // media file data chunk based on the parameters in |snapshot_file_details|.
  void ReadDataChunkFromDeviceFile(
      std::unique_ptr<SnapshotFileDetails> snapshot_file_details);

  // Called when ReadDataChunkFromDeviceFile() completes.
  //
  // If there is no error, |data| will contain the data chunk and |error| is
  // set to false.
  //
  // If there is an error, |data| is empty and |error| is set to true.
  void OnDidReadDataChunkFromDeviceFile(
      std::unique_ptr<SnapshotFileDetails> snapshot_file_details,
      const std::string& data,
      bool error);

  // Called when the data chunk is written to the
  // |snapshot_file_details_.snapshot_file_path|.
  //
  // If the write operation succeeds, |bytes_written| is set to a non-zero
  // value.
  //
  // If the write operation fails, |bytes_written| is set to zero.
  void OnDidWriteDataChunkIntoSnapshotFile(
      std::unique_ptr<SnapshotFileDetails> snapshot_file_details,
      uint32_t bytes_written);

  // The device unique identifier to query the device.
  const std::string device_handle_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPReadFileWorker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_READ_FILE_WORKER_H_
