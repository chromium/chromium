// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"

#include <sys/statvfs.h>
#include <sys/xattr.h>

#include <algorithm>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_stream_md5_digester.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/drive.pb.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_util.h"
#include "net/base/escape.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

using chromeos::disks::DiskMountManager;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;
using storage::FileSystemURL;

namespace extensions {
namespace {

const char kRootPath[] = "/";

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsAsync(const base::FilePath& mount_path,
                       uint64_t* total_size,
                       uint64_t* remaining_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0)
    *total_size = size;
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0)
    *remaining_size = size;
}

// Retrieves the maximum file name length of the file system of |path|.
// Returns 0 if it could not be queried.
size_t GetFileNameMaxLengthAsync(const std::string& path) {
  struct statvfs stat = {};
  if (HANDLE_EINTR(statvfs(path.c_str(), &stat)) != 0) {
    // The filesystem seems not supporting statvfs(). Assume it to be a commonly
    // used bound 255, and log the failure.
    LOG(ERROR) << "Cannot statvfs() the name length limit for: " << path;
    return 255;
  }
  return stat.f_namemax;
}

bool GetFileExtendedAttribute(const base::FilePath& path,
                              const char* name,
                              std::vector<char>* value) {
  ssize_t len = getxattr(path.value().c_str(), name, nullptr, 0);
  if (len < 0) {
    PLOG_IF(ERROR, errno != ENODATA) << "getxattr: " << path;
    return false;
  }
  value->resize(len);
  if (getxattr(path.value().c_str(), name, value->data(), len) != len) {
    PLOG(ERROR) << "getxattr: " << path;
    return false;
  }
  return true;
}

// Returns EventRouter for the |profile_id| if available.
file_manager::EventRouter* GetEventRouterByProfileId(void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return nullptr;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  return file_manager::EventRouterFactory::GetForProfile(profile);
}

// Notifies the copy progress to extensions via event router.
void NotifyCopyProgress(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID operation_id,
    storage::FileSystemOperation::CopyProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router) {
    event_router->OnCopyProgress(
        operation_id, type,
        source_url.ToGURL(), destination_url.ToGURL(), size);
  }
}

// Callback invoked periodically on progress update of Copy().
void OnCopyProgress(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID* operation_id,
    storage::FileSystemOperation::CopyProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyCopyProgress, profile_id, *operation_id, type,
                     source_url, destination_url, size));
}

// Notifies the copy completion to extensions via event router.
void NotifyCopyCompletion(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router)
    event_router->OnCopyCompleted(
        operation_id,
        source_url.ToGURL(), destination_url.ToGURL(), error);
}

// Callback invoked upon completion of Copy() (regardless of succeeded or
// failed).
void OnCopyCompleted(
    void* profile_id,
    storage::FileSystemOperationRunner::OperationID* operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyCopyCompletion, profile_id, *operation_id,
                     source_url, destination_url, error));
}

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    void* profile_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note: |operation_id| is owned by the callback for
  // FileSystemOperationRunner::Copy(). It is always called in the next message
  // loop or later, so at least during this invocation it should alive.
  //
  // TODO(yawano): change ERROR_BEHAVIOR_ABORT to ERROR_BEHAVIOR_SKIP after
  //     error messages of individual operations become appear in the Files app
  //     UI.
  storage::FileSystemOperationRunner::OperationID* operation_id =
      new storage::FileSystemOperationRunner::OperationID;
  *operation_id = file_system_context->operation_runner()->Copy(
      source_url, destination_url,
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      base::Bind(&OnCopyProgress, profile_id, base::Unretained(operation_id)),
      base::Bind(&OnCopyCompleted, profile_id, base::Owned(operation_id),
                 source_url, destination_url));
  return *operation_id;
}

void OnCopyCancelled(base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // We just ignore the status if the copy is actually cancelled or not,
  // because failing cancellation means the operation is not running now.
  DLOG_IF(WARNING, error != base::File::FILE_OK)
      << "Failed to cancel copy: " << error;
}

// Cancels the running copy operation identified by |operation_id|.
void CancelCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemOperationRunner::OperationID operation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->Cancel(
      operation_id, base::Bind(&OnCopyCancelled));
}

// Converts a status code to a bool value and calls the |callback| with it.
void StatusCallbackToResponseCallback(
    const base::Callback<void(bool)>& callback,
    base::File::Error result) {
  callback.Run(result == base::File::FILE_OK);
}

// Calls a response callback (on the UI thread) with a file content hash
// computed on the IO thread.
void ComputeChecksumRespondOnUIThread(
    const base::Callback<void(const std::string&)>& callback,
    const std::string& hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, hash));
}

// Calls a response callback on the UI thread.
void GetFileMetadataRespondOnUIThread(
    storage::FileSystemOperation::GetMetadataCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), result, file_info));
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateEnableExternalFileSchemeFunction::Run() {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      render_frame_host()->GetProcess()->GetID(), content::kExternalFileScheme);
  return RespondNow(NoArguments());
}

FileManagerPrivateGrantAccessFunction::FileManagerPrivateGrantAccessFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction FileManagerPrivateGrantAccessFunction::Run() {
  using extensions::api::file_manager_private::GrantAccess::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    if (profile->IsOffTheRecord())
      continue;
    const GURL site = util::GetSiteForExtensionId(extension_id(), profile);
    storage::FileSystemContext* const context =
        content::BrowserContext::GetStoragePartitionForSite(profile, site)
            ->GetFileSystemContext();
    for (const auto& url : params->entry_urls) {
      const storage::FileSystemURL file_system_url =
          context->CrackURL(GURL(url));
      // Grant permissions only to valid urls backed by the external file system
      // backend.
      if (!file_system_url.is_valid() ||
          file_system_url.mount_type() != storage::kFileSystemTypeExternal) {
        continue;
      }
      backend->GrantFileAccessToExtension(extension_->id(),
                                          file_system_url.virtual_path());
      content::ChildProcessSecurityPolicy::GetInstance()
          ->GrantCreateReadWriteFile(render_frame_host()->GetProcess()->GetID(),
                                     file_system_url.path());
    }
  }
  return RespondNow(NoArguments());
}

namespace {

void PostResponseCallbackTaskToUIThread(
    const FileWatchFunctionBase::ResponseCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, success));
}

void PostNotificationCallbackTaskToUIThread(
    const storage::WatcherManager::NotificationCallback& callback,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, type));
}

}  // namespace

void FileWatchFunctionBase::Respond(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SetResult(std::make_unique<base::Value>(success));
  SendResponse(success);
}

bool FileWatchFunctionBase::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return false;

  // First param is url of a file to watch.
  std::string url;
  if (!args_->GetString(0, &url) || url.empty())
    return false;

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  const FileSystemURL file_system_url =
      file_system_context->CrackURL(GURL(url));
  if (file_system_url.path().empty()) {
    Respond(false);
    return true;
  }

  file_manager::EventRouter* const event_router =
      file_manager::EventRouterFactory::GetForProfile(GetProfile());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileWatchFunctionBase::RunAsyncOnIOThread, this,
                     file_system_context, file_system_url,
                     event_router->GetWeakPtr()));
  return true;
}

void FileWatchFunctionBase::RunAsyncOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_system_url,
    base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::WatcherManager* const watcher_manager =
      file_system_context->GetWatcherManager(file_system_url.type());

  if (!watcher_manager) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &FileWatchFunctionBase::PerformFallbackFileWatchOperationOnUIThread,
            this, file_system_url, event_router));
    return;
  }

  PerformFileWatchOperationOnIOThread(file_system_context, watcher_manager,
                                      file_system_url, event_router);
}

void FileManagerPrivateInternalAddFileWatchFunction::
    PerformFileWatchOperationOnIOThread(
        scoped_refptr<storage::FileSystemContext> file_system_context,
        storage::WatcherManager* watcher_manager,
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  watcher_manager->AddWatcher(
      file_system_url, false /* recursive */,
      base::Bind(&StatusCallbackToResponseCallback,
                 base::Bind(&PostResponseCallbackTaskToUIThread,
                            base::Bind(&FileWatchFunctionBase::Respond, this))),
      base::Bind(
          &PostNotificationCallbackTaskToUIThread,
          base::Bind(&file_manager::EventRouter::OnWatcherManagerNotification,
                     event_router, file_system_url, extension_id())));
}

void FileManagerPrivateInternalAddFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->AddFileWatch(file_system_url.path(),
                             file_system_url.virtual_path(), extension_id(),
                             base::Bind(&FileWatchFunctionBase::Respond, this));
}

void FileManagerPrivateInternalRemoveFileWatchFunction::
    PerformFileWatchOperationOnIOThread(
        scoped_refptr<storage::FileSystemContext> file_system_context,
        storage::WatcherManager* watcher_manager,
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  watcher_manager->RemoveWatcher(
      file_system_url, false /* recursive */,
      base::Bind(
          &StatusCallbackToResponseCallback,
          base::Bind(&PostResponseCallbackTaskToUIThread,
                     base::Bind(&FileWatchFunctionBase::Respond, this))));
}

void FileManagerPrivateInternalRemoveFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->RemoveFileWatch(file_system_url.path(), extension_id());
  Respond(true);
}

bool FileManagerPrivateGetSizeStatsFunction::RunAsync() {
  using extensions::api::file_manager_private::GetSizeStats::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return false;

  if (volume->type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE &&
      !base::FeatureList::IsEnabled(chromeos::features::kDriveFs)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(GetProfile());
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled.
      // If stats couldn't be gotten for drive, result should be left
      // undefined. See comments in GetDriveAvailableSpaceCallback().
      SendResponse(true);
      return true;
    }

    file_system->GetAvailableSpace(base::BindOnce(
        &FileManagerPrivateGetSizeStatsFunction::OnGetDriveAvailableSpace,
        this));
  } else if (volume->type() == file_manager::VOLUME_TYPE_MTP) {
    // Resolve storage_name.
    storage_monitor::StorageMonitor* storage_monitor =
        storage_monitor::StorageMonitor::GetInstance();
    storage_monitor::StorageInfo info;
    storage_monitor->GetStorageInfoForPath(volume->mount_path(), &info);
    std::string storage_name;
    base::RemoveChars(info.location(), kRootPath, &storage_name);
    DCHECK(!storage_name.empty());

    // Get MTP StorageInfo.
    auto* manager = storage_monitor->media_transfer_protocol_manager();
    manager->GetStorageInfoFromDevice(
        storage_name,
        base::Bind(
            &FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace,
            this));
  } else {
    uint64_t* total_size = new uint64_t(0);
    uint64_t* remaining_size = new uint64_t(0);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetSizeStatsAsync, volume->mount_path(), total_size,
                       remaining_size),
        base::BindOnce(&FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats,
                       this, base::Owned(total_size),
                       base::Owned(remaining_size)));
  }
  return true;
}

void FileManagerPrivateGetSizeStatsFunction::OnGetDriveAvailableSpace(
    drive::FileError error,
    int64_t bytes_total,
    int64_t bytes_used) {
  if (error == drive::FILE_ERROR_OK) {
    const uint64_t bytes_total_unsigned = bytes_total;
    // bytes_used can be larger than bytes_total (over quota).
    const uint64_t bytes_remaining_unsigned =
        std::max(bytes_total - bytes_used, int64_t(0));
    OnGetSizeStats(&bytes_total_unsigned, &bytes_remaining_unsigned);
  } else {
    // If stats couldn't be gotten for drive, result should be left undefined.
    SendResponse(true);
  }
}

void FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace(
    device::mojom::MtpStorageInfoPtr mtp_storage_info,
    const bool error) {
  if (error) {
    // If stats couldn't be gotten from MTP volume, result should be left
    // undefined same as we do for Drive.
    SendResponse(true);
    return;
  }

  const uint64_t max_capacity = mtp_storage_info->max_capacity;
  const uint64_t free_space_in_bytes = mtp_storage_info->free_space_in_bytes;
  OnGetSizeStats(&max_capacity, &free_space_in_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats(
    const uint64_t* total_size,
    const uint64_t* remaining_size) {
  std::unique_ptr<base::DictionaryValue> sizes(new base::DictionaryValue());

  sizes->SetDouble("totalSize", static_cast<double>(*total_size));
  sizes->SetDouble("remainingSize", static_cast<double>(*remaining_size));

  SetResult(std::move(sizes));
  SendResponse(true);
}

bool FileManagerPrivateInternalValidatePathNameLengthFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::ValidatePathNameLength::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->parent_url)));
  if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
    return false;

  // No explicit limit on the length of Drive file names.
  if (file_system_url.type() == storage::kFileSystemTypeDrive) {
    SetResult(std::make_unique<base::Value>(true));
    SendResponse(true);
    return true;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::Bind(&GetFileNameMaxLengthAsync,
                 file_system_url.path().AsUTF8Unsafe()),
      base::Bind(&FileManagerPrivateInternalValidatePathNameLengthFunction::
                     OnFilePathLimitRetrieved,
                 this, params->name.size()));
  return true;
}

void FileManagerPrivateInternalValidatePathNameLengthFunction::
    OnFilePathLimitRetrieved(size_t current_length, size_t max_length) {
  SetResult(std::make_unique<base::Value>(current_length <= max_length));
  SendResponse(true);
}

bool FileManagerPrivateFormatVolumeFunction::RunAsync() {
  using extensions::api::file_manager_private::FormatVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume)
    return false;

  DiskMountManager::GetInstance()->FormatMountedDevice(
      volume->mount_path().AsUTF8Unsafe());
  SendResponse(true);
  return true;
}

bool FileManagerPrivateRenameVolumeFunction::RunAsync() {
  using extensions::api::file_manager_private::RenameVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume)
    return false;

  DiskMountManager::GetInstance()->RenameMountedDevice(
      volume->mount_path().AsUTF8Unsafe(), params->new_name);
  SendResponse(true);
  return true;
}

// Obtains file size of URL.
void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(
      url, fields,
      base::BindOnce(&GetFileMetadataRespondOnUIThread, std::move(callback)));
}

// Checks if the available space of the |path| is enough for required |bytes|.
bool CheckLocalDiskSpace(const base::FilePath& path, int64_t bytes) {
  return bytes <= base::SysInfo::AmountOfFreeDiskSpace(path) -
                      cryptohome::kMinFreeSpaceInBytes;
}

bool FileManagerPrivateInternalStartCopyFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::StartCopy::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty() || params->parent_url.empty() ||
      params->new_name.empty()) {
    // Error code in format of DOMError.name.
    SetError("EncodingError");
    return false;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  // |parent| may have a trailing slash if it is a root directory.
  std::string destination_url_string = params->parent_url;
  if (destination_url_string.back() != '/')
    destination_url_string += '/';
  destination_url_string += net::EscapePath(params->new_name);

  source_url_ = file_system_context->CrackURL(GURL(params->url));
  destination_url_ =
      file_system_context->CrackURL(GURL(destination_url_string));

  if (!source_url_.is_valid() || !destination_url_.is_valid()) {
    // Error code in format of DOMError.name.
    SetError("EncodingError");
    return false;
  }

  // Check if the destination directory is downloads. If so, secure available
  // spece by freeing drive caches.
  if (destination_url_.filesystem_id() ==
      file_manager::util::GetDownloadsMountPointName(GetProfile())) {
    return base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &GetFileMetadataOnIOThread, file_system_context, source_url_,
            storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
            base::BindOnce(&FileManagerPrivateInternalStartCopyFunction::
                               RunAfterGetFileMetadata,
                           this)));
  }

  return base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &FileManagerPrivateInternalStartCopyFunction::RunAfterFreeDiskSpace,
          this, true));
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterGetFileMetadata(
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result != base::File::FILE_OK) {
    SetError("NotFoundError");
    SendResponse(false);
    return;
  }

  drive::FileSystemInterface* const drive_file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (drive_file_system) {
    drive_file_system->FreeDiskSpaceIfNeededFor(
        file_info.size,
        base::Bind(
            &FileManagerPrivateInternalStartCopyFunction::RunAfterFreeDiskSpace,
            this));
  } else {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &CheckLocalDiskSpace,
            file_manager::util::GetDownloadsFolderForProfile(GetProfile()),
            file_info.size),
        base::BindOnce(
            &FileManagerPrivateInternalStartCopyFunction::RunAfterFreeDiskSpace,
            this));
  }
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterFreeDiskSpace(
    bool available) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!available) {
    SetError("QuotaExceededError");
    SendResponse(false);
    return;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  const bool result = base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&StartCopyOnIOThread, GetProfile(), file_system_context,
                 source_url_, destination_url_),
      base::Bind(
          &FileManagerPrivateInternalStartCopyFunction::RunAfterStartCopy,
          this));
  if (!result)
    SendResponse(false);
}

void FileManagerPrivateInternalStartCopyFunction::RunAfterStartCopy(
    int operation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SetResult(std::make_unique<base::Value>(operation_id));
  SendResponse(true);
}

bool FileManagerPrivateCancelCopyFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::CancelCopy::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  // We don't much take care about the result of cancellation.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CancelCopyOnIOThread, file_system_context,
                     params->copy_id));
  SendResponse(true);
  return true;
}

bool FileManagerPrivateInternalResolveIsolatedEntriesFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::ResolveIsolatedEntries::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  DCHECK(file_system_context.get());

  const storage::ExternalFileSystemBackend* external_backend =
      file_system_context->external_backend();
  DCHECK(external_backend);

  file_manager::util::FileDefinitionList file_definition_list;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    const FileSystemURL file_system_url =
        file_system_context->CrackURL(GURL(params->urls[i]));
    DCHECK(external_backend->CanHandleType(file_system_url.type()))
        << "GURL: " << file_system_url.ToGURL()
        << "type: " << file_system_url.type();
    FileDefinition file_definition;
    const bool result =
        file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            GetProfile(), extension_->id(), file_system_url.path(),
            &file_definition.virtual_path);
    if (!result)
      continue;
    // The API only supports isolated files. It still works for directories,
    // as the value is ignored for existing entries.
    file_definition.is_directory = false;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      GetProfile(), extension_->id(),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(
          &FileManagerPrivateInternalResolveIsolatedEntriesFunction::
              RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList,
          this));
  return true;
}

void FileManagerPrivateInternalResolveIsolatedEntriesFunction::
    RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  using extensions::api::file_manager_private_internal::EntryDescription;
  std::vector<EntryDescription> entries;

  for (const auto& definition : *entry_definition_list) {
    if (definition.error != base::File::FILE_OK)
      continue;
    EntryDescription entry;
    entry.file_system_name = definition.file_system_name;
    entry.file_system_root = definition.file_system_root_url;
    entry.file_full_path = "/" + definition.full_path.AsUTF8Unsafe();
    entry.file_is_directory = definition.is_directory;
    entries.push_back(std::move(entry));
  }

  results_ = extensions::api::file_manager_private_internal::
      ResolveIsolatedEntries::Results::Create(entries);
  SendResponse(true);
}

FileManagerPrivateInternalComputeChecksumFunction::
    FileManagerPrivateInternalComputeChecksumFunction()
    : digester_(new drive::util::FileStreamMd5Digester()) {
}

FileManagerPrivateInternalComputeChecksumFunction::
    ~FileManagerPrivateInternalComputeChecksumFunction() = default;

bool FileManagerPrivateInternalComputeChecksumFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::ComputeChecksum::Params;
  using drive::util::FileStreamMd5Digester;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty()) {
    SetError("File URL must be provided.");
    return false;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->url)));
  if (!file_system_url.is_valid()) {
    SetError("File URL was invalid");
    return false;
  }

  std::unique_ptr<storage::FileStreamReader> reader =
      file_system_context->CreateFileStreamReader(
          file_system_url, 0, storage::kMaximumLength, base::Time());

  FileStreamMd5Digester::ResultCallback result_callback = base::Bind(
      &ComputeChecksumRespondOnUIThread,
      base::Bind(&FileManagerPrivateInternalComputeChecksumFunction::Respond,
                 this));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileStreamMd5Digester::GetMd5Digest,
                     base::Unretained(digester_.get()), base::Passed(&reader),
                     result_callback));

  return true;
}

void FileManagerPrivateInternalComputeChecksumFunction::Respond(
    const std::string& hash) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetResult(std::make_unique<base::Value>(hash));
  SendResponse(true);
}

bool FileManagerPrivateSearchFilesByHashesFunction::RunAsync() {
  using api::file_manager_private::SearchFilesByHashes::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(hirono): Check the volume ID and fail the function for volumes other
  // than Drive.

  drive::EventLogger* const logger =
      file_manager::util::GetLogger(GetProfile());
  if (logger) {
    logger->Log(logging::LOG_INFO,
                "%s[%d] called. (volume id: %s, number of hashes: %zd)", name(),
                request_id(), params->volume_id.c_str(),
                params->hash_list.size());
  }
  set_log_on_completion(true);

  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(GetProfile());
  if (!integration_service) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return false;
  }

  std::set<std::string> hashes(params->hash_list.begin(),
                               params->hash_list.end());

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (file_system) {
    file_system->SearchByHashes(
        hashes,
        base::BindOnce(
            &FileManagerPrivateSearchFilesByHashesFunction::OnSearchByHashes,
            this, hashes));
  } else {
    // |file_system| is NULL if the backend is DriveFs. It doesn't provide
    // dedicated backup solution yet, so for now just walk the files and check
    // MD5 extended attribute.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &FileManagerPrivateSearchFilesByHashesFunction::SearchByAttribute,
            this, hashes,
            integration_service->GetMountPointPath().Append(
                drive::util::kDriveMyDriveRootDirName),
            drive::util::GetDriveMountPointPath(GetProfile())),
        base::BindOnce(
            &FileManagerPrivateSearchFilesByHashesFunction::OnSearchByAttribute,
            this, hashes));
  }

  return true;
}

std::vector<drive::HashAndFilePath>
FileManagerPrivateSearchFilesByHashesFunction::SearchByAttribute(
    const std::set<std::string>& hashes,
    const base::FilePath& dir,
    const base::FilePath& prefix) {
  std::vector<drive::HashAndFilePath> results;

  if (hashes.empty())
    return results;

  std::set<std::string> remaining = hashes;
  std::vector<char> attribute;
  base::FileEnumerator enumerator(dir, true, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (GetFileExtendedAttribute(path, "user.drive.md5", &attribute)) {
      std::string md5(attribute.begin(), attribute.end());

      if (remaining.erase(md5)) {
        base::FilePath drive_path = prefix;
        bool success = dir.AppendRelativePath(path, &drive_path);
        DCHECK(success);
        results.push_back({md5, drive_path});
        if (remaining.empty())
          break;
      }
    }
  }

  return results;
}

void FileManagerPrivateSearchFilesByHashesFunction::OnSearchByAttribute(
    const std::set<std::string>& hashes,
    const std::vector<drive::HashAndFilePath>& results) {
  OnSearchByHashes(hashes, drive::FileError::FILE_ERROR_OK, results);
}

void FileManagerPrivateSearchFilesByHashesFunction::OnSearchByHashes(
    const std::set<std::string>& hashes,
    drive::FileError error,
    const std::vector<drive::HashAndFilePath>& search_results) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    SendResponse(false);
    return;
  }

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  for (const auto& hash : hashes) {
    result->SetWithoutPathExpansion(hash, std::make_unique<base::ListValue>());
  }

  for (const auto& hashAndPath : search_results) {
    DCHECK(result->HasKey(hashAndPath.hash));
    base::ListValue* list;
    result->GetListWithoutPathExpansion(hashAndPath.hash, &list);
    list->AppendString(
        file_manager::util::ConvertDrivePathToFileSystemUrl(
            GetProfile(), hashAndPath.path, extension_id()).spec());
  }
  SetResult(std::move(result));
  SendResponse(true);
}

ExtensionFunction::ResponseAction
FileManagerPrivateIsUMAEnabledFunction::Run() {
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())));
}

FileManagerPrivateInternalSetEntryTagFunction::
    FileManagerPrivateInternalSetEntryTagFunction()
    : chrome_details_(this) {}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSetEntryTagFunction::Run() {
  using extensions::api::file_manager_private_internal::SetEntryTag::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());
  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->url)));
  if (file_system_url.type() == storage::kFileSystemTypeDriveFs) {
    return RespondNow(NoArguments());
  }

  const base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_system_url.path());
  if (drive_path.empty())
    return RespondNow(Error("Only Drive files and directories are supported."));

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(chrome_details_.GetProfile());
  // |file_system| is NULL if Drive is disabled.
  if (!file_system)
    return RespondNow(Error("Drive is disabled."));

  google_apis::drive::Property::Visibility visibility;
  switch (params->visibility) {
    case extensions::api::file_manager_private::ENTRY_TAG_VISIBILITY_PRIVATE:
      visibility = google_apis::drive::Property::VISIBILITY_PRIVATE;
      break;
    case extensions::api::file_manager_private::ENTRY_TAG_VISIBILITY_PUBLIC:
      visibility = google_apis::drive::Property::VISIBILITY_PUBLIC;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error("Invalid visibility."));
      break;
  }

  file_system->SetProperty(
      drive_path, visibility, params->key, params->value,
      base::Bind(&FileManagerPrivateInternalSetEntryTagFunction::
                     OnSetEntryPropertyCompleted,
                 this));
  return RespondLater();
}

void FileManagerPrivateInternalSetEntryTagFunction::OnSetEntryPropertyCompleted(
    drive::FileError result) {
  Respond(result == drive::FILE_ERROR_OK ? NoArguments()
                                         : Error("Failed to set a tag."));
}

bool FileManagerPrivateInternalGetDirectorySizeFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::GetDirectorySize::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty()) {
    SetError("File URL must be provided.");
    return false;
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->url)));
  if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url)) {
    SetError("FileSystemBackend failed to handle the entry's url.");
    return false;
  }
  if (file_system_url.type() != storage::kFileSystemTypeNativeLocal) {
    SetError("Only local directories are supported.");
    return false;
  }

  const base::FilePath root_path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), GetProfile(), GURL(params->url));
  if (root_path.empty()) {
    SetError("Failed to get a local path from the entry's url.");
    return false;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&base::ComputeDirectorySize, root_path),
      base::Bind(&FileManagerPrivateInternalGetDirectorySizeFunction::
                     OnDirectorySizeRetrieved,
                 this));
  return true;
}

void FileManagerPrivateInternalGetDirectorySizeFunction::
    OnDirectorySizeRetrieved(int64_t size) {
  SetResult(std::make_unique<base::Value>(static_cast<double>(size)));
  SendResponse(true);
}

}  // namespace extensions
