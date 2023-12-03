// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/fileapi/mtp_file_stream_reader.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/fileapi/readahead_file_stream_reader.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/native_file_util.h"

using storage::AsyncFileUtil;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::ShareableFileReference;

namespace {

const char kDeviceMediaAsyncFileUtilTempDir[] = "DeviceMediaFileSystem";

// Called when GetFileInfo method call failed to get the details of file
// specified by the requested url. |callback| is invoked to notify the
// caller about the file |error|.
void OnGetFileInfoError(AsyncFileUtil::GetFileInfoCallback callback,
                        base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error, base::File::Info());
}

// Called after OnDidGetFileInfo finishes media check.
// |callback| is invoked to complete the GetFileInfo request.
void OnDidCheckMediaForGetFileInfo(AsyncFileUtil::GetFileInfoCallback callback,
                                   const base::File::Info& file_info,
                                   bool is_valid_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!is_valid_file) {
    OnGetFileInfoError(std::move(callback), base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  std::move(callback).Run(base::File::FILE_OK, file_info);
}

// Called after OnDidReadDirectory finishes media check.
// |callback| is invoked to complete the ReadDirectory request.
void OnDidCheckMediaForReadDirectory(
    AsyncFileUtil::ReadDirectoryCallback callback,
    bool has_more,
    AsyncFileUtil::EntryList file_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(base::File::FILE_OK, std::move(file_list), has_more);
}

// Called when CreateDirectory method call failed.
void OnCreateDirectoryError(AsyncFileUtil::StatusCallback callback,
                            base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called when ReadDirectory method call failed to enumerate the directory
// objects. |callback| is invoked to notify the caller about the |error|
// that occured while reading the directory objects.
void OnReadDirectoryError(AsyncFileUtil::ReadDirectoryCallback callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(error, AsyncFileUtil::EntryList(), false /*no more*/);
}

// Called when CopyFileLocal method call failed.
void OnCopyFileLocalError(AsyncFileUtil::StatusCallback callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called when MoveFileLocal method call failed.
void OnMoveFileLocalError(AsyncFileUtil::StatusCallback callback,
                          base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called when CopyInForeignFile method call failed.
void OnCopyInForeignFileError(AsyncFileUtil::StatusCallback callback,
                              base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called when DeleteFile method call failed.
void OnDeleteFileError(AsyncFileUtil::StatusCallback callback,
                       base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called when DeleteDirectory method call failed.
void OnDeleteDirectoryError(AsyncFileUtil::StatusCallback callback,
                            base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error);
}

// Called on a blocking pool thread to create a snapshot file to hold the
// contents of |device_file_path|. The snapshot file is created in the
// "profile_path/kDeviceMediaAsyncFileUtilTempDir" directory. Return the
// snapshot file path or an empty path on failure.
base::FilePath CreateSnapshotFileOnBlockingPool(
    const base::FilePath& profile_path) {
  base::FilePath snapshot_file_path;
  base::FilePath media_file_system_dir_path =
      profile_path.AppendASCII(kDeviceMediaAsyncFileUtilTempDir);
  if (!base::CreateDirectory(media_file_system_dir_path) ||
      !base::CreateTemporaryFileInDir(media_file_system_dir_path,
                                      &snapshot_file_path)) {
    LOG(WARNING) << "Could not create media snapshot file "
                 << media_file_system_dir_path.value();
    snapshot_file_path = base::FilePath();
  }
  return snapshot_file_path;
}

// Called after OnDidCreateSnapshotFile finishes media check.
// |callback| is invoked to complete the CreateSnapshotFile request.
void OnDidCheckMediaForCreateSnapshotFile(
    AsyncFileUtil::CreateSnapshotFileCallback callback,
    const base::File::Info& file_info,
    scoped_refptr<storage::ShareableFileReference> platform_file,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::FilePath platform_path(platform_file.get()->path());
  if (error != base::File::FILE_OK)
    platform_file.reset();
  std::move(callback).Run(error, file_info, platform_path, platform_file);
}

// Called when the snapshot file specified by the |platform_path| is
// successfully created. |file_info| contains the device media file details
// for which the snapshot file is created.
void OnDidCreateSnapshotFile(AsyncFileUtil::CreateSnapshotFileCallback callback,
                             base::SequencedTaskRunner* media_task_runner,
                             bool validate_media_files,
                             const base::File::Info& file_info,
                             const base::FilePath& platform_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_refptr<storage::ShareableFileReference> file =
      ShareableFileReference::GetOrCreate(
          platform_path,
          ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          media_task_runner);

  if (validate_media_files) {
    media_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&NativeMediaFileUtil::IsMediaFile, platform_path),
        base::BindOnce(&OnDidCheckMediaForCreateSnapshotFile,
                       std::move(callback), file_info, file));
  } else {
    OnDidCheckMediaForCreateSnapshotFile(std::move(callback), file_info, file,
                                         base::File::FILE_OK);
  }
}

// Called when CreateSnapshotFile method call fails. |callback| is invoked to
// notify the caller about the |error|.
void OnCreateSnapshotFileError(
    AsyncFileUtil::CreateSnapshotFileCallback callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(error, base::File::Info(), base::FilePath(),
                          scoped_refptr<ShareableFileReference>());
}

// Called when the snapshot file specified by the |snapshot_file_path| is
// created to hold the contents of the url.path(). If the snapshot
// file is successfully created, |snapshot_file_path| will be an non-empty
// file path. In case of failure, |snapshot_file_path| will be an empty file
// path. Forwards the CreateSnapshot request to the delegate to copy the
// contents of url.path() to |snapshot_file_path|.
void OnSnapshotFileCreatedRunTask(
    std::unique_ptr<FileSystemOperationContext> context,
    AsyncFileUtil::CreateSnapshotFileCallback callback,
    const FileSystemURL& url,
    bool validate_media_files,
    const base::FilePath& snapshot_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (snapshot_file_path.empty()) {
    OnCreateSnapshotFileError(std::move(callback),
                              base::File::FILE_ERROR_FAILED);
    return;
  }
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(std::move(callback),
                              base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->CreateSnapshotFile(
      url.path(),  // device file path
      snapshot_file_path,
      base::BindOnce(&OnDidCreateSnapshotFile, std::move(split_callback.first),
                     base::RetainedRef(context->task_runner()),
                     validate_media_files),
      base::BindOnce(&OnCreateSnapshotFileError,
                     std::move(split_callback.second)));
}

}  // namespace

class DeviceMediaAsyncFileUtil::MediaPathFilterWrapper
    : public base::RefCountedThreadSafe<MediaPathFilterWrapper> {
 public:
  MediaPathFilterWrapper();

  MediaPathFilterWrapper(const MediaPathFilterWrapper&) = delete;
  MediaPathFilterWrapper& operator=(const MediaPathFilterWrapper&) = delete;

  // Check if entries in |file_list| look like media files.
  // Append the ones that look like media files to |results|.
  // Should run on a media task runner.
  AsyncFileUtil::EntryList FilterMediaEntries(
      const AsyncFileUtil::EntryList& file_list);

  // Check if |path| looks like a media file.
  bool CheckFilePath(const base::FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<MediaPathFilterWrapper>;

  virtual ~MediaPathFilterWrapper();

  std::unique_ptr<MediaPathFilter> media_path_filter_;
};

DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::MediaPathFilterWrapper()
    : media_path_filter_(new MediaPathFilter) {
}

DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::~MediaPathFilterWrapper() {
}

AsyncFileUtil::EntryList
DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::FilterMediaEntries(
    const AsyncFileUtil::EntryList& file_list) {
  AsyncFileUtil::EntryList results;
  for (size_t i = 0; i < file_list.size(); ++i) {
    const filesystem::mojom::DirectoryEntry& entry = file_list[i];
    if (entry.type == filesystem::mojom::FsFileType::DIRECTORY ||
        CheckFilePath(entry.name)) {
      results.push_back(entry);
    }
  }
  return results;
}

bool DeviceMediaAsyncFileUtil::MediaPathFilterWrapper::CheckFilePath(
    const base::FilePath& path) {
  return media_path_filter_->Match(path);
}

DeviceMediaAsyncFileUtil::~DeviceMediaAsyncFileUtil() {
}

// static
std::unique_ptr<DeviceMediaAsyncFileUtil> DeviceMediaAsyncFileUtil::Create(
    const base::FilePath& profile_path,
    MediaFileValidationType validation_type) {
  DCHECK(!profile_path.empty());
  return base::WrapUnique(
      new DeviceMediaAsyncFileUtil(profile_path, validation_type));
}

bool DeviceMediaAsyncFileUtil::SupportsStreaming(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate)
    return false;
  return delegate->IsStreaming();
}

void DeviceMediaAsyncFileUtil::CreateOrOpen(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    uint32_t file_flags,
    CreateOrOpenCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Returns an error if any unsupported flag is found.
  if (file_flags &
      ~(base::File::FLAG_OPEN | base::File::FLAG_READ |
        base::File::FLAG_WRITE_ATTRIBUTES | base::File::FLAG_WIN_NO_EXECUTE)) {
    std::move(callback).Run(base::File(base::File::FILE_ERROR_SECURITY),
                            base::OnceClosure());
    return;
  }
  auto* task_runner = context->task_runner();
  CreateSnapshotFile(
      std::move(context), url,
      base::BindOnce(&NativeMediaFileUtil::CreatedSnapshotFileForCreateOrOpen,
                     base::RetainedRef(task_runner), file_flags,
                     std::move(callback)));
}

void DeviceMediaAsyncFileUtil::EnsureFileExists(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    EnsureFileExistsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY, false);
}

void DeviceMediaAsyncFileUtil::CreateDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnCreateDirectoryError(std::move(callback),
                           base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCreateDirectoryError(std::move(callback),
                           base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto [on_success, on_error] = base::SplitOnceCallback(std::move(callback));
  delegate->CreateDirectory(
      url.path(), exclusive, recursive,
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidCreateDirectory,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_success)),
      base::BindOnce(&OnCreateDirectoryError, std::move(on_error)));
}

void DeviceMediaAsyncFileUtil::GetFileInfo(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnGetFileInfoError(std::move(callback), base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->GetFileInfo(
      url.path(),
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidGetFileInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::RetainedRef(context->task_runner()), url.path(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnGetFileInfoError, std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::ReadDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    ReadDirectoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnReadDirectoryError(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  delegate->ReadDirectory(
      url.path(),
      base::BindRepeating(&DeviceMediaAsyncFileUtil::OnDidReadDirectory,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::RetainedRef(context->task_runner()), callback),
      base::BindOnce(&OnReadDirectoryError, callback));
}

void DeviceMediaAsyncFileUtil::Touch(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::Truncate(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  NOTIMPLEMENTED();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void DeviceMediaAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(dest_url);
  if (!delegate) {
    OnCopyFileLocalError(std::move(callback), base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCopyFileLocalError(std::move(callback), base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->CopyFileLocal(
      src_url.path(), dest_url.path(),
      base::BindOnce(&CreateSnapshotFileOnBlockingPool, profile_path_),
      progress_callback,
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidCopyFileLocal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnCopyFileLocalError, std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::MoveFileLocal(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(dest_url);
  if (!delegate) {
    OnMoveFileLocalError(std::move(callback), base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnMoveFileLocalError(std::move(callback), base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->MoveFileLocal(
      src_url.path(), dest_url.path(),
      base::BindOnce(&CreateSnapshotFileOnBlockingPool, profile_path_),
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidMoveFileLocal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnMoveFileLocalError, std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::CopyInForeignFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(dest_url);
  if (!delegate) {
    OnCopyInForeignFileError(std::move(callback),
                             base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnCopyInForeignFileError(std::move(callback),
                             base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->CopyFileFromLocal(
      src_file_path, dest_url.path(),
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidCopyInForeignFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnCopyInForeignFileError,
                     std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::DeleteFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* const delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnDeleteFileError(std::move(callback), base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnDeleteFileError(std::move(callback), base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->DeleteFile(
      url.path(),
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidDeleteFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnDeleteFileError, std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::DeleteDirectory(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceAsyncDelegate* const delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnDeleteDirectoryError(std::move(callback),
                           base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (delegate->IsReadOnly()) {
    OnDeleteDirectoryError(std::move(callback),
                           base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Only one of the success or error callbacks will be called here.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  delegate->DeleteDirectory(
      url.path(),
      base::BindOnce(&DeviceMediaAsyncFileUtil::OnDidDeleteDirectory,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&OnDeleteDirectoryError,
                     std::move(split_callback.second)));
}

void DeviceMediaAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void DeviceMediaAsyncFileUtil::CreateSnapshotFile(
    std::unique_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    OnCreateSnapshotFileError(std::move(callback),
                              base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner(context->task_runner());
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateSnapshotFileOnBlockingPool, profile_path_),
      base::BindOnce(&OnSnapshotFileCreatedRunTask, std::move(context),
                     std::move(callback), url, validate_media_files()));
}

std::unique_ptr<storage::FileStreamReader>
DeviceMediaAsyncFileUtil::GetFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  MTPDeviceAsyncDelegate* delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate)
    return nullptr;

  DCHECK(delegate->IsStreaming());
  return std::unique_ptr<storage::FileStreamReader>(
      new ReadaheadFileStreamReader(new MTPFileStreamReader(
          context, url, offset, expected_modification_time,
          validate_media_files())));
}

void DeviceMediaAsyncFileUtil::AddWatcher(
    const storage::FileSystemURL& url,
    bool recursive,
    storage::WatcherManager::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  MTPDeviceAsyncDelegate* const delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  delegate->AddWatcher(url.origin().GetURL(), url.path(), recursive,
                       std::move(callback), std::move(notification_callback));
}

void DeviceMediaAsyncFileUtil::RemoveWatcher(
    const storage::FileSystemURL& url,
    const bool recursive,
    storage::WatcherManager::StatusCallback callback) {
  MTPDeviceAsyncDelegate* const delegate =
      MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(url);
  if (!delegate) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  delegate->RemoveWatcher(url.origin().GetURL(), url.path(), recursive,
                          std::move(callback));
}

DeviceMediaAsyncFileUtil::DeviceMediaAsyncFileUtil(
    const base::FilePath& profile_path,
    MediaFileValidationType validation_type)
    : profile_path_(profile_path) {
  if (validation_type == APPLY_MEDIA_FILE_VALIDATION) {
    media_path_filter_wrapper_ = new MediaPathFilterWrapper;
  }
}

void DeviceMediaAsyncFileUtil::OnDidCreateDirectory(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidGetFileInfo(
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& path,
    AsyncFileUtil::GetFileInfoCallback callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (file_info.is_directory || !validate_media_files()) {
    OnDidCheckMediaForGetFileInfo(std::move(callback), file_info,
                                  true /* valid */);
    return;
  }

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaPathFilterWrapper::CheckFilePath,
                     media_path_filter_wrapper_, path),
      base::BindOnce(&OnDidCheckMediaForGetFileInfo, std::move(callback),
                     file_info));
}

void DeviceMediaAsyncFileUtil::OnDidReadDirectory(
    base::SequencedTaskRunner* task_runner,
    AsyncFileUtil::ReadDirectoryCallback callback,
    AsyncFileUtil::EntryList file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!validate_media_files()) {
    OnDidCheckMediaForReadDirectory(callback, has_more, std::move(file_list));
    return;
  }

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaPathFilterWrapper::FilterMediaEntries,
                     media_path_filter_wrapper_, std::move(file_list)),
      base::BindOnce(&OnDidCheckMediaForReadDirectory, callback, has_more));
}

void DeviceMediaAsyncFileUtil::OnDidCopyFileLocal(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidMoveFileLocal(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidCopyInForeignFile(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidDeleteFile(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

void DeviceMediaAsyncFileUtil::OnDidDeleteDirectory(StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_OK);
}

bool DeviceMediaAsyncFileUtil::validate_media_files() const {
  return media_path_filter_wrapper_.get() != nullptr;
}
