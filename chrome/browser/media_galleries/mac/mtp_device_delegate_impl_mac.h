// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MAC_MTP_DEVICE_DELEGATE_IMPL_MAC_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MAC_MTP_DEVICE_DELEGATE_IMPL_MAC_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"

// Delegate for presenting an Image Capture device through the filesystem
// API. The synthetic filesystem will be rooted at the constructed location,
// and names of all files notified through the ItemAdded call will be
// all appear as children of that directory. (ItemAdded calls with directories
// will be ignored.)
// Note on thread management: This class is thread-compatible: it can be created
// on any thread, but then mutates all state on the UI thread. The async
// delegate interface can be invoked on any thread, as it simply forwards calls
// to the UI thread.
class MTPDeviceDelegateImplMac : public MTPDeviceAsyncDelegate {
 public:
  MTPDeviceDelegateImplMac(const std::string& device_id,
                           const base::FilePath::StringType& synthetic_path);

  MTPDeviceDelegateImplMac(const MTPDeviceDelegateImplMac&) = delete;
  MTPDeviceDelegateImplMac& operator=(const MTPDeviceDelegateImplMac&) = delete;

  // MTPDeviceAsyncDelegate implementation. These functions are called on the
  // IO thread by the async filesystem file util. They forward to
  // similarly-named methods on the UI thread.
  void GetFileInfo(const base::FilePath& file_path,
                   GetFileInfoSuccessCallback success_callback,
                   ErrorCallback error_callback) override;

  void CreateDirectory(const base::FilePath& directory_path,
                       const bool exclusive,
                       const bool recursive,
                       CreateDirectorySuccessCallback success_callback,
                       ErrorCallback error_callback) override;

  // Note: passed absolute paths, but expects relative paths in reply.
  void ReadDirectory(const base::FilePath& root,
                     ReadDirectorySuccessCallback success_callback,
                     ErrorCallback error_callback) override;

  // Note: passed absolute paths.
  void CreateSnapshotFile(const base::FilePath& device_file_path,
                          const base::FilePath& local_path,
                          CreateSnapshotFileSuccessCallback success_callback,
                          ErrorCallback error_callback) override;
  bool IsStreaming() override;
  void ReadBytes(const base::FilePath& device_file_path,
                 const scoped_refptr<net::IOBuffer>& buf,
                 int64_t offset,
                 int buf_len,
                 ReadBytesSuccessCallback success_callback,
                 ErrorCallback error_callback) override;
  bool IsReadOnly() const override;
  void CopyFileLocal(const base::FilePath& source_file_path,
                     const base::FilePath& device_file_path,
                     CreateTemporaryFileCallback create_temporary_file_callback,
                     CopyFileProgressCallback progress_callback,
                     CopyFileLocalSuccessCallback success_callback,
                     ErrorCallback error_callback) override;
  void MoveFileLocal(const base::FilePath& source_file_path,
                     const base::FilePath& device_file_path,
                     CreateTemporaryFileCallback create_temporary_file_callback,
                     MoveFileLocalSuccessCallback success_callback,
                     ErrorCallback error_callback) override;
  void CopyFileFromLocal(const base::FilePath& source_file_path,
                         const base::FilePath& device_file_path,
                         CopyFileFromLocalSuccessCallback success_callback,
                         ErrorCallback error_callback) override;
  void DeleteFile(const base::FilePath& file_path,
                  DeleteFileSuccessCallback success_callback,
                  ErrorCallback error_callback) override;
  void DeleteDirectory(const base::FilePath& file_path,
                       DeleteDirectorySuccessCallback success_callback,
                       ErrorCallback error_callback) override;
  void AddWatcher(const GURL& origin,
                  const base::FilePath& file_path,
                  const bool recursive,
                  storage::WatcherManager::StatusCallback callback,
                  storage::WatcherManager::NotificationCallback
                      notification_callback) override;
  void RemoveWatcher(const GURL& origin,
                     const base::FilePath& file_path,
                     const bool recursive,
                     storage::WatcherManager::StatusCallback callback) override;
  void CancelPendingTasksAndDeleteDelegate() override;

  // Forward delegates for ImageCaptureDeviceListener. These are
  // invoked in callbacks of the ImageCapture library on the UI thread.
  virtual void ItemAdded(const std::string& name,
                         const base::File::Info& info);
  virtual void NoMoreItems();
  virtual void DownloadedFile(const std::string& name,
                              base::File::Error error);

  // Scheduled when early directory reads are requested. The
  // timeout will signal an ABORT error to the caller if the
  // device metadata cannot be read.
  void ReadDirectoryTimeout(const base::FilePath& root);

 private:
  class DeviceListener;

  ~MTPDeviceDelegateImplMac() override;

  // Delegate for GetFileInfo, called on the UI thread.
  void GetFileInfoImpl(const base::FilePath& file_path,
                       base::File::Info* file_info,
                       base::File::Error* error);

  // Delegate for ReadDirectory, called on the UI thread.
  void ReadDirectoryImpl(const base::FilePath& root,
                         ReadDirectorySuccessCallback success_callback,
                         ErrorCallback error_callback);

  // Delegate for CreateSnapshotFile, called on the UI thread.
  void DownloadFile(const base::FilePath& device_file_path,
                    const base::FilePath& local_path,
                    CreateSnapshotFileSuccessCallback success_callback,
                    ErrorCallback error_callback);

  // Public for closures; should not be called except by
  // CancelTasksAndDeleteDelegate.
  void CancelAndDelete();

  // Cancels any outstanding downloads.
  void CancelDownloads();

  // If necessary, notifies the ReadDirectory callback that all data
  // has been read.
  void NotifyReadDir();

  std::string device_id_;
  base::FilePath root_path_;

  // Interface object for the camera underlying this MTP session.
  std::unique_ptr<DeviceListener> camera_interface_;

  // Stores a map from filename to file metadata received from the camera.
  std::map<base::FilePath::StringType, base::File::Info> file_info_;

  // List of filenames received from the camera.
  std::vector<base::FilePath> file_paths_;

  // Set to true when all file metadata has been received from the camera.
  bool received_all_files_;

  struct ReadFileRequest {
    ReadFileRequest();
    ReadFileRequest(const std::string& request_file,
                    const base::FilePath& snapshot_filename,
                    CreateSnapshotFileSuccessCallback success_cb,
                    ErrorCallback error_cb);
    ~ReadFileRequest();

    std::string request_file;
    base::FilePath snapshot_file;
    CreateSnapshotFileSuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  typedef std::list<ReadFileRequest> ReadFileTransactionList;

  struct ReadDirectoryRequest {
    ReadDirectoryRequest(const base::FilePath& dir,
                         const ReadDirectorySuccessCallback& success_cb,
                         ErrorCallback error_cb);
    ~ReadDirectoryRequest();

    base::FilePath directory;
    ReadDirectorySuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  typedef std::list<ReadDirectoryRequest> ReadDirTransactionList;

  ReadFileTransactionList read_file_transactions_;
  ReadDirTransactionList read_dir_transactions_;

  base::WeakPtrFactory<MTPDeviceDelegateImplMac> weak_factory_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MAC_MTP_DEVICE_DELEGATE_IMPL_MAC_H_
