// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_TASK_HELPER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_TASK_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "services/device/public/mojom/mtp_file_entry.mojom.h"
#include "storage/browser/file_system/async_file_util.h"

class MTPReadFileWorker;
struct SnapshotRequestInfo;

// MTPDeviceTaskHelper dispatches the media transfer protocol (MTP) device
// operation requests (such as GetFileInfo, ReadDirectory, CreateSnapshotFile,
// OpenStorage and CloseStorage) to the MediaTransferProtocolManager.
// MTPDeviceTaskHelper lives on the UI thread. MTPDeviceTaskHelperMapService
// owns the MTPDeviceTaskHelper objects. MTPDeviceTaskHelper is instantiated per
// MTP device storage.
class MTPDeviceTaskHelper {
 public:
  struct MTPEntry {
    MTPEntry();

    uint32_t file_id;
    std::string name;
    base::File::Info file_info;
  };

  typedef std::vector<MTPEntry> MTPEntries;

  typedef base::OnceCallback<void(bool succeeded)> OpenStorageCallback;

  using GetFileInfoSuccessCallback =
      MTPDeviceAsyncDelegate::GetFileInfoSuccessCallback;

  typedef base::OnceClosure CreateDirectorySuccessCallback;

  // When |has_more| is true, it means to expect another callback with more
  // entries.
  using ReadDirectorySuccessCallback =
      base::RepeatingCallback<void(const MTPEntries& entries, bool has_more)>;

  using CheckDirectoryEmptySuccessCallback =
      base::OnceCallback<void(bool is_empty)>;

  typedef base::OnceClosure RenameObjectSuccessCallback;

  typedef base::OnceClosure CopyFileFromLocalSuccessCallback;

  typedef base::OnceClosure DeleteObjectSuccessCallback;

  typedef MTPDeviceAsyncDelegate::ErrorCallback ErrorCallback;

  MTPDeviceTaskHelper();

  MTPDeviceTaskHelper(const MTPDeviceTaskHelper&) = delete;
  MTPDeviceTaskHelper& operator=(const MTPDeviceTaskHelper&) = delete;

  ~MTPDeviceTaskHelper();

  // Dispatches the request to the MediaTransferProtocolManager to open the MTP
  // storage for communication.
  //
  // |storage_name| specifies the name of the storage device.
  // |callback| is called when the OpenStorage request completes. |callback|
  // runs on the IO thread.
  void OpenStorage(const std::string& storage_name,
                   const bool read_only,
                   OpenStorageCallback callback);

  // Dispatches the GetFileInfo request to the MediaTransferProtocolManager.
  //
  // |file_id| specifies the id of the file whose details are requested.
  //
  // If the file details are fetched successfully, |success_callback| is invoked
  // on the IO thread to notify the caller about the file details.
  //
  // If there is an error, |error_callback| is invoked on the IO thread to
  // notify the caller about the file error.
  void GetFileInfo(uint32_t file_id,
                   GetFileInfoSuccessCallback success_callback,
                   ErrorCallback error_callback);

  // Forwards CreateDirectory request to the MediaTransferProtocolManager.
  void CreateDirectory(const uint32_t parent_id,
                       const std::string& directory_name,
                       CreateDirectorySuccessCallback success_callback,
                       ErrorCallback error_callback);

  // Dispatches the read directory request to the MediaTransferProtocolManager.
  //
  // |directory_id| specifies the directory to enumerate entries for.
  //
  // If the directory file entries are enumerated successfully,
  // |success_callback| is invoked on the IO thread to notify the caller about
  // the directory file entries. When there are many entries, instead of calling
  // |success_callback| once with the complete set of entries,
  // |success_callback| may be called repeatedly with subsets of the complete
  // set.
  //
  // If there is an error, |error_callback| is invoked on the IO thread to
  // notify the caller about the file error.
  void ReadDirectory(const uint32_t directory_id,
                     ReadDirectorySuccessCallback success_callback,
                     ErrorCallback error_callback);

  // Dispatches a read directory request to the MediaTransferProtocolManager to
  // check if |directory_id| is empty.
  //
  // On success, |success_callback| is invoked on the IO thread to notify the
  // caller with the results.
  //
  // If there is an error, |error_callback| is invoked on the IO thread to
  // notify the caller about the file error.
  void CheckDirectoryEmpty(uint32_t directory_id,
                           CheckDirectoryEmptySuccessCallback success_callback,
                           ErrorCallback error_callback);

  // Forwards the WriteDataIntoSnapshotFile request to the MTPReadFileWorker
  // object.
  //
  // |request_info| specifies the snapshot file request params.
  // |snapshot_file_info| specifies the metadata of the snapshot file.
  void WriteDataIntoSnapshotFile(SnapshotRequestInfo request_info,
                                 const base::File::Info& snapshot_file_info);

  // Dispatches the read bytes request to the MediaTransferProtocolManager.
  //
  // |request| contains details about the byte request including the file path,
  // byte range, and the callbacks. The callbacks specified within |request| are
  // called on the IO thread to notify the caller about success or failure.
  void ReadBytes(MTPDeviceAsyncDelegate::ReadBytesRequest request);

  // Forwards RenameObject request to the MediaTransferProtocolManager.
  void RenameObject(const uint32_t object_id,
                    const std::string& new_name,
                    RenameObjectSuccessCallback success_callback,
                    ErrorCallback error_callback);

  // Forwards CopyFileFromLocal request to the MediaTransferProtocolManager.
  void CopyFileFromLocal(const std::string& storage_name,
                         const int source_file_descriptor,
                         const uint32_t parent_id,
                         const std::string& file_name,
                         CopyFileFromLocalSuccessCallback success_callback,
                         ErrorCallback error_callback);

  // Forwards DeleteObject request to the MediaTransferProtocolManager.
  void DeleteObject(const uint32_t object_id,
                    DeleteObjectSuccessCallback success_callback,
                    ErrorCallback error_callback);

  // Dispatches the CloseStorage request to the MediaTransferProtocolManager.
  void CloseStorage() const;

 private:
  // Query callback for OpenStorage() to run |callback| on the IO thread.
  //
  // If OpenStorage request succeeds, |error| is set to false and
  // |device_handle| contains the handle to communicate with the MTP device.
  //
  // If OpenStorage request fails, |error| is set to true and |device_handle| is
  // set to an empty string.
  void OnDidOpenStorage(OpenStorageCallback callback,
                        const std::string& device_handle,
                        bool error);

  // Query callback for GetFileInfo().
  //
  // If there is no error, |entries| will contain a single element with the
  // requested media device file details and |error| is set to false.
  // |success_callback| is invoked on the IO thread to notify the caller.
  //
  // When |entries| has a size other than 1, or if |error| is true, then an
  // error has occurred. In this case, |error_callback| is invoked on the IO
  // thread to notify the caller.
  void OnGetFileInfo(GetFileInfoSuccessCallback success_callback,
                     ErrorCallback error_callback,
                     std::vector<device::mojom::MtpFileEntryPtr> entries,
                     bool error) const;

  // Called when CreateDirectory completes.
  void OnCreateDirectory(CreateDirectorySuccessCallback success_callback,
                         ErrorCallback error_callback,
                         const bool error) const;

  // Query callback for ReadDirectoryEntryIds().
  //
  // If there is no error, |error| is set to false, and |file_ids| has the IDs
  // of the directory file entries. If |file_ids| is empty, then just run
  // |success_callback|. Otherwise, get the directories entries from |file_ids|
  // in chunks via OnGotDirectoryEntries().
  //
  // If there is an error, then |error| is set to true, and |error_callback| is
  // invoked on the IO thread to notify the caller.
  void OnReadDirectoryEntryIdsToReadDirectory(
      ReadDirectorySuccessCallback success_callback,
      ErrorCallback error_callback,
      const std::vector<uint32_t>& file_ids,
      bool error);

  // Query callback for GetFileInfo() when called by
  // OnReadDirectoryEntryIdsToReadDirectory().
  //
  // |success_callback| and |error_callback| are the same as the parameters to
  // OnReadDirectoryEntryIdsToReadDirectory().
  //
  // |expected_file_ids| contains the expected IDs for |file_entries|.
  // |file_ids_to_read| contains the IDs to read next.
  // |error| indicates if the GetFileInfo() call succeeded or failed.
  void OnGotDirectoryEntries(
      ReadDirectorySuccessCallback success_callback,
      ErrorCallback error_callback,
      const std::vector<uint32_t>& expected_file_ids,
      const std::vector<uint32_t>& file_ids_to_read,
      std::vector<device::mojom::MtpFileEntryPtr> file_entries,
      bool error);

  // Query callback for CheckDirectoryEmpty().
  //
  // If there is no error, |error| is set to false, |file_ids| has the directory
  // file ids and |success_callback| is invoked on the IO thread to notify the
  // caller whether |file_ids| is empty or not.
  //
  // If there is an error, |error| is set to true, |file_entries| is empty
  // and |error_callback| is invoked on the IO thread to notify the caller.
  void OnCheckedDirectoryEmpty(
      CheckDirectoryEmptySuccessCallback success_callback,
      ErrorCallback error_callback,
      const std::vector<uint32_t>& file_ids,
      bool error) const;

  // Intermediate step to finish a ReadBytes request.
  void OnGetFileInfoToReadBytes(
      MTPDeviceAsyncDelegate::ReadBytesRequest request,
      std::vector<device::mojom::MtpFileEntryPtr> entries,
      bool error);

  // Query callback for ReadBytes();
  //
  // If there is no error, |error| is set to false, the buffer within |request|
  // is written to, and the success callback within |request| is invoked on the
  // IO thread to notify the caller.
  //
  // If there is an error, |error| is set to true, the buffer within |request|
  // is untouched, and the error callback within |request| is invoked on the
  // IO thread to notify the caller.
  void OnDidReadBytes(MTPDeviceAsyncDelegate::ReadBytesRequest request,
                      const base::File::Info& file_info,
                      const std::string& data,
                      bool error) const;

  // Called when RenameObject completes.
  void OnRenameObject(RenameObjectSuccessCallback success_callback,
                      ErrorCallback error_callback,
                      const bool error) const;

  // Called when CopyFileFromLocal completes.
  void OnCopyFileFromLocal(CopyFileFromLocalSuccessCallback success_callback,
                           ErrorCallback error_callback,
                           const bool error) const;

  // Called when DeleteObject completes.
  void OnDeleteObject(DeleteObjectSuccessCallback success_callback,
                      ErrorCallback error_callback,
                      const bool error) const;

  // Called when the device is uninitialized.
  //
  // Runs |error_callback| on the IO thread to notify the caller about the
  // device |error|.
  void HandleDeviceError(ErrorCallback error_callback,
                         base::File::Error error) const;

  // Handle to communicate with the MTP device.
  std::string device_handle_;

  // Used to handle WriteDataInfoSnapshotFile request.
  std::unique_ptr<MTPReadFileWorker> read_file_worker_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<MTPDeviceTaskHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_CHROMEOS_MTP_DEVICE_TASK_HELPER_H_
