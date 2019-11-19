// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace net {
class IOBuffer;
}

// Asynchronous delegate for media transfer protocol (MTP) device to perform
// media device file system operations. Class that implements this
// delegate does the actual communication with the MTP device.
// The lifetime of the delegate is managed by the MTPDeviceMapService class.
// Member functions and callbacks run on the IO thread.
class MTPDeviceAsyncDelegate {
 public:
  // A callback to be called when GetFileInfo method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info)> GetFileInfoSuccessCallback;

  // A callback to be called when CreateDirectory method call succeeds.
  typedef base::Closure CreateDirectorySuccessCallback;

  // A callback to be called when ReadDirectory method call succeeds.
  typedef base::RepeatingCallback<
      void(storage::AsyncFileUtil::EntryList file_list, bool has_more)>
      ReadDirectorySuccessCallback;

  // A callback to be called when GetFileInfo/ReadDirectory/CreateSnapshot
  // method call fails.
  typedef base::Callback<void(base::File::Error error)> ErrorCallback;

  // A callback to be called when CreateSnapshotFile method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info,
           const base::FilePath& local_path)> CreateSnapshotFileSuccessCallback;

  // A callback to be called when ReadBytes method call succeeds.
  typedef base::Callback<
      void(const base::File::Info& file_info,
           int bytes_read)> ReadBytesSuccessCallback;

  struct ReadBytesRequest {
    ReadBytesRequest(uint32_t file_id,
                     net::IOBuffer* buf,
                     int64_t offset,
                     int buf_len,
                     const ReadBytesSuccessCallback& success_callback,
                     const ErrorCallback& error_callback);
    ReadBytesRequest(const ReadBytesRequest& other);
    ~ReadBytesRequest();

    uint32_t file_id;
    scoped_refptr<net::IOBuffer> buf;
    int64_t offset;
    int buf_len;
    ReadBytesSuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  // A callback to be called to create a temporary file. Path to the temporary
  // file is returned as base::FilePath. The caller is responsible to manage
  // life time of the temporary file.
  typedef base::Callback<base::FilePath()> CreateTemporaryFileCallback;

  // A callback to be called when CopyFileLocal method call succeeds.
  typedef base::Closure CopyFileLocalSuccessCallback;

  // A callback to be called when MoveFileLocal method call succeeds.
  typedef base::Closure MoveFileLocalSuccessCallback;

  // A callback to be called when CopyFileFromLocal method call succeeds.
  typedef base::Closure CopyFileFromLocalSuccessCallback;

  // A callback to be called when DeleteFile method call succeeds.
  typedef base::Closure DeleteFileSuccessCallback;

  // A callback to be called when DeleteDirectory method call succeeds.
  typedef base::Closure DeleteDirectorySuccessCallback;

  typedef storage::AsyncFileUtil::CopyFileProgressCallback
      CopyFileProgressCallback;

  // Gets information about the given |file_path| and invokes the appropriate
  // callback asynchronously when complete.
  virtual void GetFileInfo(
      const base::FilePath& file_path,
      const GetFileInfoSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Creates a directory to |directory_path|. When |exclusive| is true, this
  // returns base::File::FILE_ERROR_EXISTS if a directory already exists for
  // |directory_path|. When |recursive| is true, the directory is created
  // recursively to |directory_path|.
  virtual void CreateDirectory(
      const base::FilePath& directory_path,
      const bool exclusive,
      const bool recursive,
      const CreateDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Enumerates the |root| directory contents and invokes the appropriate
  // callback asynchronously when complete.
  virtual void ReadDirectory(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Copy the contents of |device_file_path| to |local_path|. Invokes the
  // appropriate callback asynchronously when complete.
  virtual void CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Platform-specific implementations that are streaming don't create a local
  // snapshot file. Blobs are instead FileSystemURL backed and read in a stream.
  virtual bool IsStreaming() = 0;

  // Reads up to |buf_len| bytes from |device_file_path| into |buf|. Invokes the
  // appropriate callback asynchronously when complete. Only valid when
  // IsStreaming() is true.
  virtual void ReadBytes(const base::FilePath& device_file_path,
                         const scoped_refptr<net::IOBuffer>& buf,
                         int64_t offset,
                         int buf_len,
                         const ReadBytesSuccessCallback& success_callback,
                         const ErrorCallback& error_callback) = 0;

  // Returns true if storage is opened for read only.
  virtual bool IsReadOnly() const = 0;

  // Copies a file |source_file_path| to |device_file_path|.
  // |create_temporary_file_callback| can be used to create a temporary file.
  virtual void CopyFileLocal(
      const base::FilePath& source_file_path,
      const base::FilePath& device_file_path,
      const CreateTemporaryFileCallback& create_temporary_file_callback,
      const CopyFileProgressCallback& progress_callback,
      const CopyFileLocalSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Moves a file |source_file_path| to |device_file_path|.
  // |create_temporary_file_callback| can be used to create a temporary file.
  virtual void MoveFileLocal(
      const base::FilePath& source_file_path,
      const base::FilePath& device_file_path,
      const CreateTemporaryFileCallback& create_temporary_file_callback,
      const MoveFileLocalSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Copies a file from |source_file_path| to |device_file_path|.
  virtual void CopyFileFromLocal(
      const base::FilePath& source_file_path,
      const base::FilePath& device_file_path,
      const CopyFileFromLocalSuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Deletes a file at |file_path|.
  virtual void DeleteFile(const base::FilePath& file_path,
                          const DeleteFileSuccessCallback& success_callback,
                          const ErrorCallback& error_callback) = 0;

  // Deletes a directory at |file_path|. The directory must be empty.
  virtual void DeleteDirectory(
      const base::FilePath& file_path,
      const DeleteDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) = 0;

  // Adds watcher to |file_path| as |origin|.
  virtual void AddWatcher(
      const GURL& origin,
      const base::FilePath& file_path,
      const bool recursive,
      const storage::WatcherManager::StatusCallback& callback,
      const storage::WatcherManager::NotificationCallback&
          notification_callback) = 0;

  // Removes watcher from |file_path| of |origin|.
  virtual void RemoveWatcher(
      const GURL& origin,
      const base::FilePath& file_path,
      const bool recursive,
      const storage::WatcherManager::StatusCallback& callback) = 0;

  // Called when the
  // (1) Browser application is in shutdown mode (or)
  // (2) Last extension using this MTP device is destroyed (or)
  // (3) Attached MTP device is removed (or)
  // (4) User revoked the MTP device gallery permission.
  // Ownership of |MTPDeviceAsyncDelegate| is handed off to the delegate
  // implementation class by this call. This function should take care of
  // cancelling all the pending tasks before deleting itself.
  virtual void CancelPendingTasksAndDeleteDelegate() = 0;

 protected:
  // Always destruct this object via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceAsyncDelegate() {}
};

typedef base::Callback<void(MTPDeviceAsyncDelegate*)>
    CreateMTPDeviceAsyncDelegateCallback;

void CreateMTPDeviceAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const bool read_only,
    const CreateMTPDeviceAsyncDelegateCallback& callback);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_ASYNC_DELEGATE_H_
