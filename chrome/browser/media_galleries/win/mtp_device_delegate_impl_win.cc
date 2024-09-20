// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPDeviceDelegateImplWin implementation.

#include "chrome/browser/media_galleries/win/mtp_device_delegate_impl_win.h"

#include <portabledevice.h>
#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/win/mtp_device_operations_util.h"
#include "chrome/browser/media_galleries/win/portable_device_map_service.h"
#include "chrome/browser/media_galleries/win/snapshot_file_details.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/common/file_system/file_system_util.h"

namespace {

// Gets the details of the MTP partition storage specified by the
// |storage_path| on the UI thread. Returns true if the storage details are
// valid and returns false otherwise.
bool GetStorageInfoOnUIThread(const std::wstring& storage_path,
                              std::wstring* pnp_device_id,
                              std::wstring* storage_object_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!storage_path.empty());
  DCHECK(pnp_device_id);
  DCHECK(storage_object_id);
  std::wstring storage_device_id;
  base::RemoveChars(storage_path, L"\\\\", &storage_device_id);
  DCHECK(!storage_device_id.empty());
  // TODO(gbillock): Take the StorageMonitor as an argument.
  storage_monitor::StorageMonitor* monitor =
      storage_monitor::StorageMonitor::GetInstance();
  DCHECK(monitor);
  return monitor->GetMTPStorageInfoFromDeviceId(
      base::WideToUTF8(storage_device_id), pnp_device_id, storage_object_id);
}

// Returns the object id of the file object specified by the |file_path|,
// e.g. if the |file_path| is "\\MTP:StorageSerial:SID-{1001,,192}:125\DCIM"
// and |device_info.registered_device_path_| is
// "\\MTP:StorageSerial:SID-{1001,,192}:125", this function returns the
// identifier of the "DCIM" folder object.
//
// Returns an empty string if the device is detached while the request is in
// progress or when the |file_path| is invalid.
std::wstring GetFileObjectIdFromPathOnBlockingPoolThread(
    const MTPDeviceDelegateImplWin::StorageDeviceInfo& device_info,
    const base::FilePath& file_path) {
  DCHECK(!file_path.empty());
  IPortableDevice* device =
      PortableDeviceMapService::GetInstance()->GetPortableDevice(
          device_info.registered_device_path);
  if (!device)
    return std::wstring();

  if (device_info.registered_device_path == file_path.value())
    return device_info.storage_object_id;

  base::FilePath relative_path;
  if (!base::FilePath(device_info.registered_device_path).AppendRelativePath(
          file_path, &relative_path))
    return std::wstring();

  std::vector<std::wstring> path_components = relative_path.GetComponents();
  DCHECK(!path_components.empty());
  std::wstring parent_id(device_info.storage_object_id);
  std::wstring file_object_id;
  for (size_t i = 0; i < path_components.size(); ++i) {
    file_object_id = media_transfer_protocol::GetObjectIdFromName(
        device, parent_id, base::AsString16(path_components[i]));
    if (file_object_id.empty())
      break;
    parent_id = file_object_id;
  }
  return file_object_id;
}

// Returns a pointer to a new instance of AbstractFileEnumerator for the given
// |root| directory. Called on a blocking pool thread.
std::unique_ptr<MTPDeviceObjectEnumerator>
CreateFileEnumeratorOnBlockingPoolThread(
    const MTPDeviceDelegateImplWin::StorageDeviceInfo& device_info,
    const base::FilePath& root) {
  DCHECK(!device_info.registered_device_path.empty());
  DCHECK(!root.empty());
  IPortableDevice* device =
      PortableDeviceMapService::GetInstance()->GetPortableDevice(
          device_info.registered_device_path);
  if (!device)
    return nullptr;

  std::wstring object_id =
      GetFileObjectIdFromPathOnBlockingPoolThread(device_info, root);
  if (object_id.empty())
    return nullptr;

  MTPDeviceObjectEntries entries;
  if (!media_transfer_protocol::GetDirectoryEntries(device, object_id,
                                                    &entries) ||
      entries.empty()) {
    return nullptr;
  }

  return std::make_unique<MTPDeviceObjectEnumerator>(entries);
}

// Opens the device for communication on a blocking pool thread.
// |pnp_device_id| specifies the PnP device id.
// |registered_device_path| specifies the registered file system root path for
// the given device.
bool OpenDeviceOnBlockingPoolThread(
    const std::wstring& pnp_device_id,
    const std::wstring& registered_device_path) {
  DCHECK(!pnp_device_id.empty());
  DCHECK(!registered_device_path.empty());
  Microsoft::WRL::ComPtr<IPortableDevice> device =
      media_transfer_protocol::OpenDevice(pnp_device_id);
  bool init_succeeded = device.Get() != NULL;
  if (init_succeeded) {
    PortableDeviceMapService::GetInstance()->AddPortableDevice(
        registered_device_path, device.Get());
  }
  return init_succeeded;
}

// Gets the |file_path| details from the MTP device specified by the
// |device_info.registered_device_path|. On success, |error| is set to
// base::File::FILE_OK and fills in |file_info|. On failure, |error| is set
// to corresponding platform file error and |file_info| is not set.
base::File::Error GetFileInfoOnBlockingPoolThread(
    const MTPDeviceDelegateImplWin::StorageDeviceInfo& device_info,
    const base::FilePath& file_path,
    base::File::Info* file_info) {
  DCHECK(!device_info.registered_device_path.empty());
  DCHECK(!file_path.empty());
  DCHECK(file_info);
  IPortableDevice* device =
      PortableDeviceMapService::GetInstance()->GetPortableDevice(
          device_info.registered_device_path);
  if (!device)
    return base::File::FILE_ERROR_FAILED;

  std::wstring object_id =
      GetFileObjectIdFromPathOnBlockingPoolThread(device_info, file_path);
  if (object_id.empty())
    return base::File::FILE_ERROR_FAILED;
  return media_transfer_protocol::GetFileEntryInfo(device, object_id,
                                                   file_info);
}

// Reads the |root| directory file entries on a blocking pool thread. On
// success, |error| is set to base::File::FILE_OK and |entries| contains the
// directory file entries. On failure, |error| is set to platform file error
// and |entries| is not set.
base::File::Error ReadDirectoryOnBlockingPoolThread(
    const MTPDeviceDelegateImplWin::StorageDeviceInfo& device_info,
    const base::FilePath& root,
    storage::AsyncFileUtil::EntryList* entries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!root.empty());
  DCHECK(entries);
  base::File::Info file_info;
  base::File::Error error = GetFileInfoOnBlockingPoolThread(device_info, root,
                                                            &file_info);
  if (error != base::File::FILE_OK)
    return error;

  if (!file_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;

  base::FilePath current;
  std::unique_ptr<MTPDeviceObjectEnumerator> file_enum =
      CreateFileEnumeratorOnBlockingPoolThread(device_info, root);
  if (!file_enum)
    return error;

  while (!(current = file_enum->Next()).empty()) {
    entries->emplace_back(
        storage::VirtualPath::BaseName(current), base::FilePath(),
        file_enum->IsDirectory() ? filesystem::mojom::FsFileType::DIRECTORY
                                 : filesystem::mojom::FsFileType::REGULAR_FILE);
  }
  return error;
}

// Gets the device file stream object on a blocking pool thread.
// |device_info| contains the device storage partition details.
// On success, returns base::File::FILE_OK and file stream details are set in
// |file_details|. On failure, returns a platform file error and file stream
// details are not set in |file_details|.
base::File::Error GetFileStreamOnBlockingPoolThread(
    const MTPDeviceDelegateImplWin::StorageDeviceInfo& device_info,
    SnapshotFileDetails* file_details) {
  DCHECK(file_details);
  DCHECK(!file_details->request_info().device_file_path.empty());
  DCHECK(!file_details->request_info().snapshot_file_path.empty());
  IPortableDevice* device =
      PortableDeviceMapService::GetInstance()->GetPortableDevice(
          device_info.registered_device_path);
  if (!device)
    return base::File::FILE_ERROR_FAILED;

  std::wstring file_object_id = GetFileObjectIdFromPathOnBlockingPoolThread(
      device_info, file_details->request_info().device_file_path);
  if (file_object_id.empty())
    return base::File::FILE_ERROR_FAILED;

  base::File::Info file_info;
  base::File::Error error =
      GetFileInfoOnBlockingPoolThread(
          device_info,
          file_details->request_info().device_file_path,
          &file_info);
  if (error != base::File::FILE_OK)
    return error;

  DWORD optimal_transfer_size = 0;
  Microsoft::WRL::ComPtr<IStream> file_stream;
  if (file_info.size > 0) {
    HRESULT hr = media_transfer_protocol::GetFileStreamForObject(
        device, file_object_id, &file_stream, &optimal_transfer_size);
    if (hr != S_OK)
      return base::File::FILE_ERROR_FAILED;
  }

  // LocalFileStreamReader is used to read the contents of the snapshot file.
  // Snapshot file modification time does not match the last modified time
  // of the original media file. Therefore, set the last modified time to null
  // in order to avoid the verification in LocalFileStreamReader.
  //
  // Users will use HTML5 FileSystem Entry getMetadata() interface to get the
  // actual last modified time of the media file.
  file_info.last_modified = base::Time();

  DCHECK(file_info.size == 0 || optimal_transfer_size > 0U);
  file_details->set_file_info(file_info);
  file_details->set_device_file_stream(file_stream.Get());
  file_details->set_optimal_transfer_size(optimal_transfer_size);
  return error;
}

// Copies the data chunk from device file to the snapshot file based on the
// parameters specified by |file_details|.
// Returns the total number of bytes written to the snapshot file for non-empty
// files, or 0 on failure. For empty files, just return 0.
DWORD WriteDataChunkIntoSnapshotFileOnBlockingPoolThread(
    const base::File::Info& file_info,
    Microsoft::WRL::ComPtr<IStream> device_file_stream,
    const base::FilePath& snapshot_file_path,
    DWORD optimal_transfer_size) {
  if (file_info.size == 0)
    return 0;
  return media_transfer_protocol::CopyDataChunkToLocalFile(
      device_file_stream.Get(), snapshot_file_path, optimal_transfer_size);
}

void DeletePortableDeviceOnBlockingPoolThread(
    const std::wstring& registered_device_path) {
  PortableDeviceMapService::GetInstance()->RemovePortableDevice(
      registered_device_path);
}

}  // namespace

// Used by CreateMTPDeviceAsyncDelegate() to create the MTP device
// delegate on the IO thread.
void OnGetStorageInfoCreateDelegate(
    const std::wstring& device_location,
    CreateMTPDeviceAsyncDelegateCallback callback,
    std::wstring* pnp_device_id,
    std::wstring* storage_object_id,
    bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(pnp_device_id);
  DCHECK(storage_object_id);
  if (!succeeded)
    return;
  std::move(callback).Run(new MTPDeviceDelegateImplWin(
      device_location, *pnp_device_id, *storage_object_id));
}

void CreateMTPDeviceAsyncDelegate(
    const std::wstring& device_location,
    const bool read_only,
    CreateMTPDeviceAsyncDelegateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Write operation is not supported on Windows.
  DCHECK(read_only);

  DCHECK(!device_location.empty());
  std::wstring* pnp_device_id = new std::wstring;
  std::wstring* storage_object_id = new std::wstring;
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetStorageInfoOnUIThread, device_location,
                     base::Unretained(pnp_device_id),
                     base::Unretained(storage_object_id)),
      base::BindOnce(&OnGetStorageInfoCreateDelegate, device_location,
                     std::move(callback), base::Owned(pnp_device_id),
                     base::Owned(storage_object_id)));
}

// MTPDeviceDelegateImplWin ---------------------------------------------------

MTPDeviceDelegateImplWin::StorageDeviceInfo::StorageDeviceInfo(
    const std::wstring& pnp_device_id,
    const std::wstring& registered_device_path,
    const std::wstring& storage_object_id)
    : pnp_device_id(pnp_device_id),
      registered_device_path(registered_device_path),
      storage_object_id(storage_object_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

MTPDeviceDelegateImplWin::PendingTaskInfo::PendingTaskInfo(
    const base::Location& location,
    base::OnceCallback<base::File::Error(void)> task,
    base::OnceCallback<void(base::File::Error)> reply)
    : location(location), task(std::move(task)), reply(std::move(reply)) {}

MTPDeviceDelegateImplWin::PendingTaskInfo::PendingTaskInfo(
    PendingTaskInfo&& other) = default;

MTPDeviceDelegateImplWin::PendingTaskInfo::~PendingTaskInfo() = default;

MTPDeviceDelegateImplWin::MTPDeviceDelegateImplWin(
    const std::wstring& registered_device_path,
    const std::wstring& pnp_device_id,
    const std::wstring& storage_object_id)
    : init_state_(UNINITIALIZED),
      media_task_runner_(MediaFileSystemBackend::MediaTaskRunner()),
      storage_device_info_(pnp_device_id,
                           registered_device_path,
                           storage_object_id),
      task_in_progress_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!registered_device_path.empty());
  DCHECK(!pnp_device_id.empty());
  DCHECK(!storage_object_id.empty());
  DCHECK(media_task_runner_.get());
}

MTPDeviceDelegateImplWin::~MTPDeviceDelegateImplWin() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void MTPDeviceDelegateImplWin::GetFileInfo(
    const base::FilePath& file_path,
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());
  base::File::Info* file_info = new base::File::Info;
  EnsureInitAndRunTask(PendingTaskInfo(
      FROM_HERE,
      base::BindOnce(&GetFileInfoOnBlockingPoolThread, storage_device_info_,
                     file_path, base::Unretained(file_info)),
      base::BindOnce(&MTPDeviceDelegateImplWin::OnGetFileInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback),
                     base::Owned(file_info))));
}

void MTPDeviceDelegateImplWin::CreateDirectory(
    const base::FilePath& directory_path,
    const bool exclusive,
    const bool recursive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::ReadDirectory(
    const base::FilePath& root,
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!root.empty());
  storage::AsyncFileUtil::EntryList* entries =
      new storage::AsyncFileUtil::EntryList;
  EnsureInitAndRunTask(PendingTaskInfo(
      FROM_HERE,
      base::BindOnce(&ReadDirectoryOnBlockingPoolThread, storage_device_info_,
                     root, base::Unretained(entries)),
      base::BindOnce(&MTPDeviceDelegateImplWin::OnDidReadDirectory,
                     weak_ptr_factory_.GetWeakPtr(), success_callback,
                     std::move(error_callback), base::Owned(entries))));
}

void MTPDeviceDelegateImplWin::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& snapshot_file_path,
    CreateSnapshotFileSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  DCHECK(!snapshot_file_path.empty());
  std::unique_ptr<SnapshotFileDetails> file_details(
      new SnapshotFileDetails(SnapshotRequestInfo(
          device_file_path, snapshot_file_path, std::move(success_callback),
          std::move(error_callback))));
  // Passing a raw SnapshotFileDetails* to the blocking pool is safe, because
  // it is owned by |file_details| in the reply callback.
  EnsureInitAndRunTask(PendingTaskInfo(
      FROM_HERE,
      base::BindOnce(&GetFileStreamOnBlockingPoolThread, storage_device_info_,
                     file_details.get()),
      base::BindOnce(&MTPDeviceDelegateImplWin::OnGetFileStream,
                     weak_ptr_factory_.GetWeakPtr(), std::move(file_details))));
}

bool MTPDeviceDelegateImplWin::IsStreaming() {
  return false;
}

void MTPDeviceDelegateImplWin::ReadBytes(
    const base::FilePath& device_file_path,
    const scoped_refptr<net::IOBuffer>& buf,
    int64_t offset,
    int buf_len,
    ReadBytesSuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

bool MTPDeviceDelegateImplWin::IsReadOnly() const {
  return true;
}

void MTPDeviceDelegateImplWin::CopyFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CreateTemporaryFileCallback create_temporary_file_callback,
    CopyFileProgressCallback progress_callback,
    CopyFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::MoveFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CreateTemporaryFileCallback create_temporary_file_callback,
    MoveFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::CopyFileFromLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::DeleteFile(
    const base::FilePath& file_path,
    DeleteFileSuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::DeleteDirectory(
    const base::FilePath& file_path,
    DeleteDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MTPDeviceDelegateImplWin::AddWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    storage::WatcherManager::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void MTPDeviceDelegateImplWin::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    storage::WatcherManager::StatusCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void MTPDeviceDelegateImplWin::CancelPendingTasksAndDeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  PortableDeviceMapService::GetInstance()->MarkPortableDeviceForDeletion(
      storage_device_info_.registered_device_path);
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeletePortableDeviceOnBlockingPoolThread,
                                storage_device_info_.registered_device_path));
  while (!pending_tasks_.empty())
    pending_tasks_.pop();
  delete this;
}

void MTPDeviceDelegateImplWin::EnsureInitAndRunTask(PendingTaskInfo task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if ((init_state_ == INITIALIZED) && !task_in_progress_) {
    DCHECK(pending_tasks_.empty());
    DCHECK(!current_snapshot_details_.get());
    media_task_runner_->PostTaskAndReplyWithResult(task_info.location,
                                                   std::move(task_info.task),
                                                   std::move(task_info.reply));
    task_in_progress_ = true;
    return;
  }

  pending_tasks_.push(std::move(task_info));
  if (init_state_ == UNINITIALIZED) {
    init_state_ = PENDING_INIT;
    media_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&OpenDeviceOnBlockingPoolThread,
                       storage_device_info_.pnp_device_id,
                       storage_device_info_.registered_device_path),
        base::BindOnce(&MTPDeviceDelegateImplWin::OnInitCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    task_in_progress_ = true;
  }
}

void MTPDeviceDelegateImplWin::WriteDataChunkIntoSnapshotFile() {
  DCHECK(current_snapshot_details_.get());
  media_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &WriteDataChunkIntoSnapshotFileOnBlockingPoolThread,
          current_snapshot_details_->file_info(),
          Microsoft::WRL::ComPtr<IStream>(
              current_snapshot_details_->device_file_stream()),
          current_snapshot_details_->request_info().snapshot_file_path,
          current_snapshot_details_->optimal_transfer_size()),
      base::BindOnce(
          &MTPDeviceDelegateImplWin::OnWroteDataChunkIntoSnapshotFile,
          weak_ptr_factory_.GetWeakPtr(),
          current_snapshot_details_->request_info().snapshot_file_path));
}

void MTPDeviceDelegateImplWin::ProcessNextPendingRequest() {
  DCHECK(!task_in_progress_);
  if (pending_tasks_.empty())
    return;
  PendingTaskInfo& task_info = pending_tasks_.front();
  task_in_progress_ = true;
  media_task_runner_->PostTaskAndReplyWithResult(task_info.location,
                                                 std::move(task_info.task),
                                                 std::move(task_info.reply));
  pending_tasks_.pop();
}

void MTPDeviceDelegateImplWin::OnInitCompleted(bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  init_state_ = succeeded ? INITIALIZED : UNINITIALIZED;
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImplWin::OnGetFileInfo(
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback,
    base::File::Info* file_info,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_info);
  if (error == base::File::FILE_OK)
    std::move(success_callback).Run(*file_info);
  else
    std::move(error_callback).Run(error);
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImplWin::OnDidReadDirectory(
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    storage::AsyncFileUtil::EntryList* file_list,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_list);
  if (error == base::File::FILE_OK)
    success_callback.Run(*file_list, false /*no more entries*/);
  else
    std::move(error_callback).Run(error);
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImplWin::OnGetFileStream(
    std::unique_ptr<SnapshotFileDetails> file_details,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_details);
  DCHECK(!file_details->request_info().device_file_path.empty());
  DCHECK(!file_details->request_info().snapshot_file_path.empty());
  DCHECK(!current_snapshot_details_.get());
  if (error != base::File::FILE_OK) {
    std::move(file_details->error_callback()).Run(error);
    task_in_progress_ = false;
    ProcessNextPendingRequest();
    return;
  }
  DCHECK(file_details->file_info().size == 0 ||
         file_details->device_file_stream());
  current_snapshot_details_ = std::move(file_details);
  WriteDataChunkIntoSnapshotFile();
}

void MTPDeviceDelegateImplWin::OnWroteDataChunkIntoSnapshotFile(
    const base::FilePath& snapshot_file_path,
    DWORD bytes_written) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!snapshot_file_path.empty());
  if (!current_snapshot_details_.get())
    return;
  DCHECK_EQ(
      current_snapshot_details_->request_info().snapshot_file_path.value(),
      snapshot_file_path.value());

  bool succeeded = false;
  bool should_continue = false;
  if (current_snapshot_details_->file_info().size > 0) {
    if (current_snapshot_details_->AddBytesWritten(bytes_written)) {
      if (current_snapshot_details_->IsSnapshotFileWriteComplete()) {
        succeeded = true;
      } else {
        should_continue = true;
      }
    }
  } else {
    // Handle empty files.
    DCHECK_EQ(0U, bytes_written);
    succeeded = true;
  }

  if (should_continue) {
    WriteDataChunkIntoSnapshotFile();
    return;
  }
  if (succeeded) {
    std::move(current_snapshot_details_->success_callback())
        .Run(current_snapshot_details_->file_info(),
             current_snapshot_details_->request_info().snapshot_file_path);
  } else {
    std::move(current_snapshot_details_->error_callback())
        .Run(base::File::FILE_ERROR_FAILED);
  }
  task_in_progress_ = false;
  current_snapshot_details_.reset();
  ProcessNextPendingRequest();
}
