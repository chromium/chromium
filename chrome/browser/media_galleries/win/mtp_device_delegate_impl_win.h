// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_

#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

class SnapshotFileDetails;

// MTPDeviceDelegateImplWin is used to communicate with the media transfer
// protocol (MTP) device to complete file system operations. These operations
// are performed asynchronously on a blocking pool thread since the device
// access may be slow and may take a long time to complete. MTP
// device can have multiple data storage partitions. MTPDeviceDelegateImplWin
// is instantiated per MTP device storage partition using
// CreateMTPDeviceAsyncDelegate(). MTPDeviceDelegateImplWin lives on the IO
// thread.
class MTPDeviceDelegateImplWin : public MTPDeviceAsyncDelegate {
 public:
  // Structure used to represent MTP device storage partition details.
  struct StorageDeviceInfo {
    StorageDeviceInfo(const std::wstring& pnp_device_id,
                      const std::wstring& registered_device_path,
                      const std::wstring& storage_object_id);

    // The PnP Device Id, used to open the device for communication,
    // e.g. "\\?\usb#vid_04a9&pid_3073#12#{6ac27878-a6fa-4155-ba85-f1d4f33}".
    const std::wstring pnp_device_id;

    // The media file system root path, which is obtained during the
    // registration of MTP device storage partition as a file system,
    // e.g. "\\MTP:StorageSerial:SID-{10001,E,9823}:237483".
    const std::wstring registered_device_path;

    // The MTP device storage partition object identifier, used to enumerate the
    // storage contents, e.g. "s10001".
    const std::wstring storage_object_id;
  };

  MTPDeviceDelegateImplWin(const MTPDeviceDelegateImplWin&) = delete;
  MTPDeviceDelegateImplWin& operator=(const MTPDeviceDelegateImplWin&) = delete;

 private:
  friend void OnGetStorageInfoCreateDelegate(
      const std::wstring& device_location,
      CreateMTPDeviceAsyncDelegateCallback callback,
      std::wstring* pnp_device_id,
      std::wstring* storage_object_id,
      bool succeeded);

  enum InitializationState {
    UNINITIALIZED = 0,
    PENDING_INIT,
    INITIALIZED
  };

  // Used to represent pending task details.
  struct PendingTaskInfo {
    PendingTaskInfo(const base::Location& location,
                    base::OnceCallback<base::File::Error(void)> task,
                    base::OnceCallback<void(base::File::Error)> reply);
    PendingTaskInfo(PendingTaskInfo&& other);
    ~PendingTaskInfo();

    base::Location location;
    base::OnceCallback<base::File::Error(void)> task;
    base::OnceCallback<void(base::File::Error)> reply;
  };

  // Defers the device initializations until the first file operation request.
  // Do all the initializations in EnsureInitAndRunTask() function.
  MTPDeviceDelegateImplWin(const std::wstring& registered_device_path,
                           const std::wstring& pnp_device_id,
                           const std::wstring& storage_object_id);

  // Destructed via CancelPendingTasksAndDeleteDelegate().
  ~MTPDeviceDelegateImplWin() override;

  // MTPDeviceAsyncDelegate:
  void GetFileInfo(const base::FilePath& file_path,
                   GetFileInfoSuccessCallback success_callback,
                   ErrorCallback error_callback) override;
  void CreateDirectory(const base::FilePath& directory_path,
                       const bool exclusive,
                       const bool recursive,
                       CreateDirectorySuccessCallback success_callback,
                       ErrorCallback error_callback) override;
  void ReadDirectory(const base::FilePath& root,
                     ReadDirectorySuccessCallback success_callback,
                     ErrorCallback error_callback) override;
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

  // Ensures the device is initialized for communication by doing a
  // call-and-reply to a blocking pool thread. |task_info.task| runs on a
  // blocking pool thread and |task_info.reply| runs on the IO thread.
  //
  // If the device is already initialized, post the |task_info.task|
  // immediately on a blocking pool thread.
  //
  // If the device is uninitialized, store the |task_info| in a pending task
  // list and then runs all the pending tasks once the device is successfully
  // initialized.
  void EnsureInitAndRunTask(PendingTaskInfo task_info);

  // Writes data chunk from the device to the snapshot file path based on the
  // parameters in |current_snapshot_details_| by doing a call-and-reply to a
  // blocking pool thread.
  void WriteDataChunkIntoSnapshotFile();

  // Processes the next pending request.
  void ProcessNextPendingRequest();

  // Handles the event that the device is initialized. |succeeded| indicates
  // whether device initialization succeeded or not. If the device is
  // successfully initialized, runs the next pending task.
  void OnInitCompleted(bool succeeded);

  // Called when GetFileInfo() completes. |file_info| specifies the requested
  // file details. |error| specifies the platform file error code.
  //
  // If the GetFileInfo() succeeds, |success_callback| is invoked to notify the
  // caller about the |file_info| details.
  //
  // If the GetFileInfo() fails, |file_info| is not set and |error_callback| is
  // invoked to notify the caller about the platform file |error|.
  void OnGetFileInfo(GetFileInfoSuccessCallback success_callback,
                     ErrorCallback error_callback,
                     base::File::Info* file_info,
                     base::File::Error error);

  // Called when ReadDirectory() completes. |file_list| contains the directory
  // file entries information. |error| specifies the platform file error code.
  //
  // If the ReadDirectory() succeeds, |success_callback| is invoked to notify
  // the caller about the directory file entries.
  //
  // If the ReadDirectory() fails, |file_list| is not set and |error_callback|
  // is invoked to notify the caller about the platform file |error|.
  void OnDidReadDirectory(ReadDirectorySuccessCallback success_callback,
                          ErrorCallback error_callback,
                          storage::AsyncFileUtil::EntryList* file_list,
                          base::File::Error error);

  // Called when the get file stream request completes.
  // |file_details.request_info| contains the CreateSnapshot request param
  // details. |error| specifies the platform file error code.
  //
  // If the file stream of the device file is successfully
  // fetched, |file_details| will contain the required details for the creation
  // of the snapshot file.
  //
  // If the get file stream request fails, |error| is set accordingly.
  void OnGetFileStream(std::unique_ptr<SnapshotFileDetails> file_details,
                       base::File::Error error);

  // Called when WriteDataChunkIntoSnapshotFile() completes.
  // |bytes_written| specifies the number of bytes written into the
  // |snapshot_file_path| during the last write operation.
  //
  // If the write operation succeeds, |bytes_written| is set to a non-zero
  // value.
  //
  // If the write operation fails, |bytes_written| is set to zero.
  void OnWroteDataChunkIntoSnapshotFile(
      const base::FilePath& snapshot_file_path,
      DWORD bytes_written);

  // Portable device initialization state.
  InitializationState init_state_;

  // The task runner where the device operation tasks runs.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Device storage partition details
  // (e.g. device path, PnP device id and storage object id).
  StorageDeviceInfo storage_device_info_;

  // Used to track the current state of the snapshot file (e.g how many bytes
  // written to the snapshot file, optimal data transfer size, source file
  // stream, etc).
  //
  // A snapshot file is created incrementally. CreateSnapshotFile request reads
  // and writes the snapshot file data in chunks. In order to retain the order
  // of the snapshot file requests, make sure there is only one active snapshot
  // file request at any time.
  std::unique_ptr<SnapshotFileDetails> current_snapshot_details_;

  // A list of pending tasks that needs to be run when the device is
  // initialized or when the current task in progress is complete.
  base::queue<PendingTaskInfo> pending_tasks_;

  // Used to make sure only one task is in progress at any time.
  bool task_in_progress_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPDeviceDelegateImplWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_DELEGATE_IMPL_WIN_H_
