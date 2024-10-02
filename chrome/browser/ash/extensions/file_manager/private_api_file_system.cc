// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_file_system.h"

#include <sys/statvfs.h>
#include <sys/xattr.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/barrier_callback.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service_factory.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"
#include "chrome/browser/ash/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/empty_trash_io_task.h"
#include "chrome/browser/ash/file_manager/extract_io_task.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/restore_io_task.h"
#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/zip_io_task.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fileapi/recent_disk_source.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "private_api_file_system.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/constants/cryptohome.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using ::ash::disks::DiskMountManager;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;
using storage::FileSystemURL;

namespace extensions {
namespace {

using file_manager::Volume;
using file_manager::VolumeManager;

std::string Redact(std::string_view s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

const char kRootPath[] = "/";

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsAsync(const base::FilePath& mount_path,
                       uint64_t* total_size,
                       uint64_t* remaining_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0) {
    *total_size = size;
  }
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0) {
    *remaining_size = size;
  }
}

// Retrieves the maximum file name length of the file system of |path|.
// Returns a default of 255 if it could not be queried.
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

// Converts a status code to a bool value and calls the |callback| with it.
void StatusCallbackToResponseCallback(base::OnceCallback<void(bool)> callback,
                                      base::File::Error result) {
  std::move(callback).Run(result == base::File::FILE_OK);
}

ash::disks::FormatFileSystemType ApiFormatFileSystemToChromeEnum(
    api::file_manager_private::FormatFileSystemType filesystem) {
  switch (filesystem) {
    case api::file_manager_private::FormatFileSystemType::kNone:
      return ash::disks::FormatFileSystemType::kUnknown;
    case api::file_manager_private::FormatFileSystemType::kVfat:
      return ash::disks::FormatFileSystemType::kVfat;
    case api::file_manager_private::FormatFileSystemType::kExfat:
      return ash::disks::FormatFileSystemType::kExfat;
    case api::file_manager_private::FormatFileSystemType::kNtfs:
      return ash::disks::FormatFileSystemType::kNtfs;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown format filesystem " << base::to_underlying(filesystem);
  return ash::disks::FormatFileSystemType::kUnknown;
}

std::optional<file_manager::io_task::OperationType> IoTaskTypeToChromeEnum(
    api::file_manager_private::IoTaskType type) {
  switch (type) {
    case api::file_manager_private::IoTaskType::kCopy:
      return file_manager::io_task::OperationType::kCopy;
    case api::file_manager_private::IoTaskType::kDelete:
      return file_manager::io_task::OperationType::kDelete;
    case api::file_manager_private::IoTaskType::kEmptyTrash:
      return file_manager::io_task::OperationType::kEmptyTrash;
    case api::file_manager_private::IoTaskType::kExtract:
      return file_manager::io_task::OperationType::kExtract;
    case api::file_manager_private::IoTaskType::kMove:
      return file_manager::io_task::OperationType::kMove;
    case api::file_manager_private::IoTaskType::kRestore:
      return file_manager::io_task::OperationType::kRestore;
    case api::file_manager_private::IoTaskType::kRestoreToDestination:
      return file_manager::io_task::OperationType::kRestoreToDestination;
    case api::file_manager_private::IoTaskType::kTrash:
      return file_manager::io_task::OperationType::kTrash;
    case api::file_manager_private::IoTaskType::kZip:
      return file_manager::io_task::OperationType::kZip;
    case api::file_manager_private::IoTaskType::kNone:
      return {};
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown I/O task type " << base::to_underlying(type);
  return {};
}

extensions::api::file_manager_private::DlpLevel DlpRulesManagerLevelToApiEnum(
    policy::DlpRulesManager::Level level) {
  using extensions::api::file_manager_private::DlpLevel;
  switch (level) {
    case policy::DlpRulesManager::Level::kAllow:
      return DlpLevel::kAllow;
    case policy::DlpRulesManager::Level::kBlock:
      return DlpLevel::kBlock;
    case policy::DlpRulesManager::Level::kWarn:
      return DlpLevel::kWarn;
    case policy::DlpRulesManager::Level::kReport:
      return DlpLevel::kReport;
    case policy::DlpRulesManager::Level::kNotSet:
      NOTREACHED_IN_MIGRATION() << "DLP level not set.";
      return DlpLevel::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown DLP level.";
  return {};
}

extensions::api::file_manager_private::VolumeType
DlpRulesManagerComponentToApiEnum(data_controls::Component component) {
  using ::extensions::api::file_manager_private::VolumeType;
  using Component = ::data_controls::Component;
  switch (component) {
    case Component::kArc:
      return VolumeType::kAndroidFiles;
    case Component::kCrostini:
      return VolumeType::kCrostini;
    case Component::kPluginVm:
      return VolumeType::kGuestOs;
    case Component::kUsb:
      return VolumeType::kRemovable;
    case Component::kDrive:
      return VolumeType::kDrive;
    case Component::kOneDrive:
      return VolumeType::kProvided;
    case Component::kUnknownComponent:
      NOTREACHED_IN_MIGRATION() << "DLP component not set.";
      return {};
  }
  NOTREACHED_IN_MIGRATION() << "Unknown component type.";
  return {};
}

policy::FilesDialogType ApiPolicyDialogTypeToChromeEnum(
    api::file_manager_private::PolicyDialogType type) {
  switch (type) {
    case api::file_manager_private::PolicyDialogType::kNone:
      return policy::FilesDialogType::kUnknown;
    case api::file_manager_private::PolicyDialogType::kWarning:
      return policy::FilesDialogType::kWarning;
    case api::file_manager_private::PolicyDialogType::kError:
      return policy::FilesDialogType::kError;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown policy dialog type " << base::to_underlying(type);
  return policy::FilesDialogType::kUnknown;
}

std::optional<policy::Policy> ApiPolicyErrorTypeToChromeEnum(
    api::file_manager_private::PolicyErrorType type) {
  switch (type) {
    case api::file_manager_private::PolicyErrorType::kDlp:
      return policy::Policy::kDlp;
    case api::file_manager_private::PolicyErrorType::kEnterpriseConnectors:
      return policy::Policy::kEnterpriseConnectors;
    case api::file_manager_private::PolicyErrorType::kNone:
      return std::nullopt;
    case api::file_manager_private::PolicyErrorType::kDlpWarningTimeout:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected policy type " << base::to_underlying(type);
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown policy error type " << base::to_underlying(type);
  return std::nullopt;
}

// Handles a callback from the LocalImageSearchService. The job of this function
// is to process the `matched` results and deliver them to the given callback.
void OnImageSearchDone(
    base::FilePath root_path,
    base::Time modified_time,
    FileManagerPrivateInternalSearchFilesFunction::OnResultsReadyCallback
        callback,
    const std::vector<app_list::FileSearchResult>& matched) {
  std::vector<std::pair<base::FilePath, bool>> results;
  for (const app_list::FileSearchResult& match : matched) {
    DVLOG(1) << "File image search inspecting " << match.file_path;
    if (!root_path.IsParent(match.file_path)) {
      continue;
    }
    if (match.last_modified < modified_time) {
      continue;
    }
    results.emplace_back(match.file_path, false);
  }
  std::move(callback).Run(results);
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateEnableExternalFileSchemeFunction::Run() {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      render_frame_host()->GetProcess()->GetID(), content::kExternalFileScheme);
  return RespondNow(NoArguments());
}

FileManagerPrivateGrantAccessFunction::FileManagerPrivateGrantAccessFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGrantAccessFunction::Run() {
  using extensions::api::file_manager_private::GrantAccess::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);

  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles) {
    if (profile->IsOffTheRecord()) {
      continue;
    }
    storage::FileSystemContext* const context =
        file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                             source_url());
    for (const auto& url : params->entry_urls) {
      const storage::FileSystemURL file_system_url =
          context->CrackURLInFirstPartyContext(GURL(url));
      // Grant permissions only to valid urls backed by the external file system
      // backend.
      if (!file_system_url.is_valid() ||
          file_system_url.mount_type() != storage::kFileSystemTypeExternal) {
        continue;
      }
      backend->GrantFileAccessToOrigin(url::Origin::Create(source_url()),
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
    FileWatchFunctionBase::ResponseCallback callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PostNotificationCallbackTaskToUIThread(
    storage::WatcherManager::NotificationCallback callback,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), type));
}

}  // namespace

void FileWatchFunctionBase::RespondWith(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (success) {
    Respond(WithArguments(success));
  } else {
    Respond(Error(""));
  }
}

ExtensionFunction::ResponseAction FileWatchFunctionBase::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!render_frame_host() || !render_frame_host()->GetProcess()) {
    return RespondNow(Error("Invalid state"));
  }

  // First param is url of a file to watch.
  if (args().empty() || !args()[0].is_string() ||
      args()[0].GetString().empty()) {
    return RespondNow(Error("Empty watch URL"));
  }
  const std::string& url = args()[0].GetString();

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  const FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(GURL(url));
  if (file_system_url.path().empty()) {
    return RespondNow(Error("Invalid URL"));
  }

  // For removeFileWatch() we can't validate the volume because it might have
  // been unmounted.
  if (IsAddWatch()) {
    VolumeManager* const volume_manager = VolumeManager::Get(profile);
    if (!volume_manager) {
      return RespondNow(Error("Cannot find VolumeManager"));
    }

    const base::WeakPtr<Volume> volume =
        volume_manager->FindVolumeFromPath(file_system_url.path());
    if (!volume) {
      return RespondNow(
          Error("Cannot find volume *", Redact(file_system_url.path())));
    }

    if (!volume->watchable()) {
      return RespondNow(Error("Volume is not watchable"));
    }
  }

  file_manager::EventRouter* const event_router =
      file_manager::EventRouterFactory::GetForProfile(profile);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileWatchFunctionBase::RunAsyncOnIOThread,
                                this, file_system_context, file_system_url,
                                event_router->GetWeakPtr()));
  return RespondLater();
}

void FileWatchFunctionBase::RunAsyncOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_system_url,
    base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::WatcherManager* const watcher_manager =
      file_system_context->GetWatcherManager(file_system_url.type());

  if (!watcher_manager) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
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
      base::BindOnce(
          &StatusCallbackToResponseCallback,
          base::BindOnce(
              &PostResponseCallbackTaskToUIThread,
              base::BindOnce(&FileWatchFunctionBase::RespondWith, this))),
      base::BindRepeating(
          &PostNotificationCallbackTaskToUIThread,
          base::BindRepeating(
              &file_manager::EventRouter::OnWatcherManagerNotification,
              event_router, file_system_url,
              url::Origin::Create(source_url()))));
}

void FileManagerPrivateInternalAddFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->AddFileWatch(
      file_system_url.path(), file_system_url.virtual_path(),
      url::Origin::Create(source_url()),
      base::BindOnce(&FileWatchFunctionBase::RespondWith, this));
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
      base::BindOnce(
          &StatusCallbackToResponseCallback,
          base::BindOnce(
              &PostResponseCallbackTaskToUIThread,
              base::BindOnce(&FileWatchFunctionBase::RespondWith, this))));
}

void FileManagerPrivateInternalRemoveFileWatchFunction::
    PerformFallbackFileWatchOperationOnUIThread(
        const storage::FileSystemURL& file_system_url,
        base::WeakPtr<file_manager::EventRouter> event_router) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(event_router);

  // Obsolete. Fallback code if storage::WatcherManager is not implemented.
  event_router->RemoveFileWatch(file_system_url.path(),
                                url::Origin::Create(source_url()));
  RespondWith(true);
}

bool FileManagerPrivateInternalAddFileWatchFunction::IsAddWatch() {
  return true;
}

bool FileManagerPrivateInternalRemoveFileWatchFunction::IsAddWatch() {
  return false;
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetSizeStatsFunction::Run() {
  using extensions::api::file_manager_private::GetSizeStats::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get()) {
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));
  }

  // For fusebox volumes, get the underlying (aka regular) volume.
  const auto fusebox = std::string_view(file_manager::util::kFuseBox);
  if (base::StartsWith(volume->file_system_type(), fusebox)) {
    std::string volume_id = params->volume_id;

    if ((volume->type() == file_manager::VOLUME_TYPE_MTP) ||
        (volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER)) {
      volume_id = volume_id.substr(fusebox.length());
    } else if (volume->type() == file_manager::VOLUME_TYPE_PROVIDED) {
      // NB: FileManagerPrivate.GetSizeStats is not called by files app JS
      // because regular PROVIDED volumes do not support size stats.
      volume_manager->ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(&volume_id);
    }

    volume = volume_manager->FindVolumeById(volume_id);
    if (!volume.get()) {
      return RespondNow(Error("Cannot find volume with ID *", volume_id));
    }
  }

  if (volume->type() == file_manager::VOLUME_TYPE_MTP) {
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
        base::BindOnce(
            &FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace,
            this));
  } else if (volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER) {
    std::string authority;
    std::string root_id;
    if (!arc::ParseDocumentsProviderPath(volume->mount_path(), &authority,
                                         &root_id)) {
      return RespondNow(Error("File path was invalid"));
    }

    // Get DocumentsProvider's root available and capacity sizes in bytes.
    auto* root_map = arc::ArcDocumentsProviderRootMap::GetForBrowserContext(
        browser_context());
    if (!root_map) {
      return RespondNow(Error("File not found"));
    }
    auto* root = root_map->Lookup(authority, root_id);
    if (!root) {
      return RespondNow(Error("File not found"));
    }
    root->GetRootSize(base::BindOnce(&FileManagerPrivateGetSizeStatsFunction::
                                         OnGetDocumentsProviderAvailableSpace,
                                     this));
  } else if (volume->type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE) {
    Profile* const profile = Profile::FromBrowserContext(browser_context());
    drive::DriveIntegrationService* integration_service =
        drive::util::GetIntegrationServiceByProfile(profile);
    if (!integration_service) {
      return RespondNow(Error("Drive not available"));
    }
    integration_service->GetQuotaUsage(base::BindOnce(
        &FileManagerPrivateGetSizeStatsFunction::OnGetDriveQuotaUsage, this));
  } else {
    uint64_t* total_size = new uint64_t(0);
    uint64_t* remaining_size = new uint64_t(0);
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetSizeStatsAsync, volume->mount_path(), total_size,
                       remaining_size),
        base::BindOnce(&FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats,
                       this, base::Owned(total_size),
                       base::Owned(remaining_size)));
  }
  return RespondLater();
}

void FileManagerPrivateGetSizeStatsFunction::OnGetMtpAvailableSpace(
    device::mojom::MtpStorageInfoPtr mtp_storage_info,
    const bool error) {
  if (error) {
    // If stats couldn't be gotten from MTP volume, result should be left
    // undefined same as we do for Drive.
    Respond(NoArguments());
    return;
  }

  const uint64_t max_capacity = mtp_storage_info->max_capacity;
  const uint64_t free_space_in_bytes = mtp_storage_info->free_space_in_bytes;
  OnGetSizeStats(&max_capacity, &free_space_in_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::
    OnGetDocumentsProviderAvailableSpace(const bool error,
                                         const uint64_t available_bytes,
                                         const uint64_t capacity_bytes) {
  if (error) {
    // If stats was not successfully retrieved from DocumentsProvider volume,
    // result should be left undefined same as we do for Drive.
    Respond(NoArguments());
    return;
  }
  OnGetSizeStats(&capacity_bytes, &available_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::OnGetDriveQuotaUsage(
    drive::FileError error,
    drivefs::mojom::QuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    Respond(NoArguments());
    return;
  }
  OnGetSizeStats(&usage->total_cloud_bytes, &usage->free_cloud_bytes);
}

void FileManagerPrivateGetSizeStatsFunction::OnGetSizeStats(
    const uint64_t* total_size,
    const uint64_t* remaining_size) {
  base::Value::Dict sizes;
  sizes.Set("totalSize", static_cast<double>(*total_size));
  sizes.Set("remainingSize", static_cast<double>(*remaining_size));
  Respond(WithArguments(std::move(sizes)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDriveQuotaMetadataFunction::Run() {
  using extensions::api::file_manager_private_internal::GetDriveQuotaMetadata::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  const GURL url = GURL(params->url);
  file_system_url_ = file_system_context->CrackURLInFirstPartyContext(url);

  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    return RespondNow(Error("Drive not available"));
  }
  integration_service->GetPooledQuotaUsage(
      base::BindOnce(&FileManagerPrivateInternalGetDriveQuotaMetadataFunction::
                         OnGetPooledQuotaUsage,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalGetDriveQuotaMetadataFunction::
    OnGetPooledQuotaUsage(drive::FileError error,
                          drivefs::mojom::PooledQuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    Respond(NoArguments());
    return;
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    return Respond(Error("Drive not available"));
  }

  quotaMetadata_.user_type =
      usage->user_type == drivefs::mojom::UserType::kUnmanaged
          ? api::file_manager_private::UserType::kUnmanaged
          : api::file_manager_private::UserType::kOrganization;
  quotaMetadata_.used_bytes = static_cast<double>(usage->used_user_bytes);
  quotaMetadata_.total_bytes = static_cast<double>(usage->total_user_bytes);
  quotaMetadata_.organization_limit_exceeded =
      usage->organization_limit_exceeded;
  quotaMetadata_.organization_name = usage->organization_name;

  if (integration_service->IsSharedDrive(file_system_url_.path())) {
    // Init quota to unlimited if no quota set.
    quotaMetadata_.total_bytes = -1;
    quotaMetadata_.used_bytes = 0;
    integration_service->GetMetadata(
        file_system_url_.path(),
        base::BindOnce(
            &FileManagerPrivateInternalGetDriveQuotaMetadataFunction::
                OnGetMetadata,
            this));
    return;
  }

  Respond(
      ArgumentList(api::file_manager_private_internal::GetDriveQuotaMetadata::
                       Results::Create(quotaMetadata_)));
}

void FileManagerPrivateInternalGetDriveQuotaMetadataFunction::OnGetMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error == drive::FileError::FILE_ERROR_OK &&
      metadata->shared_drive_quota) {
    quotaMetadata_.used_bytes =
        metadata->shared_drive_quota->quota_bytes_used_in_drive;
    quotaMetadata_.total_bytes =
        metadata->shared_drive_quota->individual_quota_bytes_total;
  }

  Respond(
      ArgumentList(api::file_manager_private_internal::GetDriveQuotaMetadata::
                       Results::Create(quotaMetadata_)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalValidatePathNameLengthFunction::Run() {
  using extensions::api::file_manager_private_internal::ValidatePathNameLength::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(
          GURL(params->parent_url)));
  if (!ash::FileSystemBackend::CanHandleURL(file_system_url)) {
    return RespondNow(Error("Invalid URL"));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFileNameMaxLengthAsync,
                     file_system_url.path().AsUTF8Unsafe()),
      base::BindOnce(&FileManagerPrivateInternalValidatePathNameLengthFunction::
                         OnFilePathLimitRetrieved,
                     this, params->name.size()));
  return RespondLater();
}

void FileManagerPrivateInternalValidatePathNameLengthFunction::
    OnFilePathLimitRetrieved(size_t current_length, size_t max_length) {
  Respond(WithArguments(current_length <= max_length));
}

ExtensionFunction::ResponseAction
FileManagerPrivateFormatVolumeFunction::Run() {
  using extensions::api::file_manager_private::FormatVolume::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume) {
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));
  }

  DiskMountManager::GetInstance()->FormatMountedDevice(
      volume->mount_path().AsUTF8Unsafe(),
      ApiFormatFileSystemToChromeEnum(params->filesystem),
      params->volume_label);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateSinglePartitionFormatFunction::Run() {
  using extensions::api::file_manager_private::SinglePartitionFormat::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const DiskMountManager::Disks& disks =
      DiskMountManager::GetInstance()->disks();

  DiskMountManager::Disks::const_iterator it = disks.begin();
  for (; it != disks.end(); ++it) {
    if (it->get()->storage_device_path() == params->device_storage_path &&
        it->get()->is_parent()) {
      break;
    }
  }

  if (it == disks.end()) {
    return RespondNow(Error("Device not found"));
  }

  const ash::disks::Disk* const device_disk = it->get();
  DCHECK(device_disk);

  if (device_disk->is_read_only()) {
    return RespondNow(Error("Invalid device"));
  }

  DiskMountManager::GetInstance()->SinglePartitionFormatDevice(
      device_disk->device_path(),
      ApiFormatFileSystemToChromeEnum(params->filesystem),
      params->volume_label);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateRenameVolumeFunction::Run() {
  using extensions::api::file_manager_private::RenameVolume::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume) {
    return RespondNow(Error("Cannot find volume with ID *", params->volume_id));
  }

  DiskMountManager::GetInstance()->RenameMountedDevice(
      volume->mount_path().AsUTF8Unsafe(), params->new_name);
  return RespondNow(NoArguments());
}

FileManagerPrivateInternalGetDisallowedTransfersFunction::
    FileManagerPrivateInternalGetDisallowedTransfersFunction() = default;

FileManagerPrivateInternalGetDisallowedTransfersFunction::
    ~FileManagerPrivateInternalGetDisallowedTransfersFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDisallowedTransfersFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private_internal::GetDisallowedTransfers::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  profile_ = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());

  for (const std::string& url : params->entries) {
    FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));
    if (!file_system_url.is_valid()) {
      return RespondNow(Error("File URL was invalid"));
    }
    source_urls_.push_back(file_system_url);
  }

  destination_url_ = file_system_context->CrackURLInFirstPartyContext(
      GURL(params->destination_entry));
  if (!destination_url_.is_valid()) {
    return RespondNow(Error("File URL was invalid"));
  }

  // If the new UX flow is enabled, return an empty list so the copy/move
  // operation can start.
  if (base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpFilesControllerAsh* files_controller =
      static_cast<policy::DlpFilesControllerAsh*>(
          rules_manager->GetDlpFilesController());
  files_controller->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, source_urls_, destination_url_, params->is_move,
      base::BindOnce(&FileManagerPrivateInternalGetDisallowedTransfersFunction::
                         OnGetDisallowedFiles,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalGetDisallowedTransfersFunction::
    OnGetDisallowedFiles(std::vector<storage::FileSystemURL> disallowed_files) {
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : disallowed_files) {
    file_manager::util::FileDefinition file_definition;
    // Disallowed transfers lists regular files not directories.
    file_definition.is_directory = false;
    file_definition.virtual_path = file.virtual_path();
    file_definition.absolute_path = file.path();
    file_definition_list.emplace_back(std::move(file_definition));
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile_,
                                                           source_url()),
      url::Origin::Create(source_url().DeprecatedGetOriginAsURL()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(&FileManagerPrivateInternalGetDisallowedTransfersFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this));
}

void FileManagerPrivateInternalGetDisallowedTransfersFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK(entry_definition_list);

  Respond(
      WithArguments(file_manager::util::ConvertEntryDefinitionListToListValue(
          *entry_definition_list)));
}

FileManagerPrivateInternalGetDlpMetadataFunction::
    FileManagerPrivateInternalGetDlpMetadataFunction() = default;

FileManagerPrivateInternalGetDlpMetadataFunction::
    ~FileManagerPrivateInternalGetDlpMetadataFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDlpMetadataFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private_internal::GetDlpMetadata::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  for (const std::string& url : params->entries) {
    FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));
    if (!file_system_url.is_valid()) {
      return RespondNow(Error("File URL was invalid"));
    }
    source_urls_.push_back(file_system_url);
  }

  policy::DlpFilesControllerAsh* files_controller =
      static_cast<policy::DlpFilesControllerAsh*>(
          rules_manager->GetDlpFilesController());

  std::optional<policy::DlpFileDestination> destination;
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    LOG(WARNING) << "Failed to locate WebContents";
    return RespondNow(Error("Failed to locate WebContents"));
  }
  ui::SelectFileDialog::Type type =
      SelectFileDialogExtensionUserData::GetDialogTypeForWebContents(
          web_contents);
  if (type != ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE) {
    destination =
        SelectFileDialogExtensionUserData::GetDialogCallerForWebContents(
            web_contents);
  }

  files_controller->GetDlpMetadata(
      source_urls_, destination,
      base::BindOnce(
          &FileManagerPrivateInternalGetDlpMetadataFunction::OnGetDlpMetadata,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalGetDlpMetadataFunction::OnGetDlpMetadata(
    std::vector<policy::DlpFilesControllerAsh::DlpFileMetadata>
        dlp_metadata_list) {
  using extensions::api::file_manager_private::DlpMetadata;
  std::vector<DlpMetadata> converted_list;
  for (const auto& md : dlp_metadata_list) {
    DlpMetadata metadata;
    metadata.is_dlp_restricted = md.is_dlp_restricted;
    metadata.source_url = md.source_url;
    metadata.is_restricted_for_destination = md.is_restricted_for_destination;
    converted_list.emplace_back(std::move(metadata));
  }
  Respond(ArgumentList(
      api::file_manager_private_internal::GetDlpMetadata::Results::Create(
          converted_list)));
}

FileManagerPrivateGetDlpRestrictionDetailsFunction::
    FileManagerPrivateGetDlpRestrictionDetailsFunction() = default;

FileManagerPrivateGetDlpRestrictionDetailsFunction::
    ~FileManagerPrivateGetDlpRestrictionDetailsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetDlpRestrictionDetailsFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !rules_manager->GetDlpFilesController()) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private::GetDlpRestrictionDetails::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  policy::DlpFilesControllerAsh* files_controller =
      static_cast<policy::DlpFilesControllerAsh*>(
          rules_manager->GetDlpFilesController());
  const std::vector<policy::DlpFilesControllerAsh::DlpFileRestrictionDetails>
      dlp_restriction_details =
          files_controller->GetDlpRestrictionDetails(params->source_url);

  using extensions::api::file_manager_private::DlpRestrictionDetails;
  std::vector<DlpRestrictionDetails> converted_list;

  for (const auto& [level, urls, components] : dlp_restriction_details) {
    DlpRestrictionDetails details;
    details.level = DlpRulesManagerLevelToApiEnum(level);
    base::ranges::move(urls.begin(), urls.end(),
                       std::back_inserter(details.urls));
    for (const auto& component : components) {
      details.components.push_back(
          DlpRulesManagerComponentToApiEnum(component));
    }
    converted_list.emplace_back(std::move(details));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDlpRestrictionDetails::Results::Create(
          converted_list)));
}

FileManagerPrivateGetDlpBlockedComponentsFunction::
    FileManagerPrivateGetDlpBlockedComponentsFunction() = default;

FileManagerPrivateGetDlpBlockedComponentsFunction::
    ~FileManagerPrivateGetDlpBlockedComponentsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetDlpBlockedComponentsFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  policy::DlpFilesControllerAsh* files_controller;
  if (!rules_manager || !rules_manager->IsFilesPolicyEnabled() ||
      !(files_controller = static_cast<policy::DlpFilesControllerAsh*>(
            rules_manager->GetDlpFilesController()))) {
    return RespondNow(WithArguments(base::Value::List()));
  }

  using extensions::api::file_manager_private::GetDlpBlockedComponents::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::vector<data_controls::Component> components =
      files_controller->GetBlockedComponents(params->source_url);

  using extensions::api::file_manager_private::VolumeType;
  std::vector<VolumeType> converted_list;

  for (const auto& component : components) {
    converted_list.emplace_back(DlpRulesManagerComponentToApiEnum(component));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDlpBlockedComponents::Results::Create(
          converted_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDialogCallerFunction::Run() {
  std::optional<policy::DlpFileDestination> caller =
      SelectFileDialogExtensionUserData::GetDialogCallerForWebContents(
          GetSenderWebContents());
  base::Value::Dict info;
  if (caller.has_value()) {
    if (caller->url().has_value()) {
      info.Set("url", caller->url()->spec());
    }
    if (caller->component().has_value()) {
      info.Set("component",
               base::to_underlying(DlpRulesManagerComponentToApiEnum(
                   caller->component().value())));
    }
  }

  return RespondNow(WithArguments(std::move(info)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalResolveIsolatedEntriesFunction::Run() {
  using extensions::api::file_manager_private_internal::ResolveIsolatedEntries::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  DCHECK(file_system_context.get());

  const auto* external_backend =
      ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(external_backend);

  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& url : params->urls) {
    const FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(GURL(url));
    DCHECK(external_backend->CanHandleType(file_system_url.type()))
        << "GURL: " << file_system_url.ToGURL()
        << "type: " << file_system_url.type();
    FileDefinition file_definition;
    const bool result =
        file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, source_url(), file_system_url.path(),
            &file_definition.virtual_path);
    if (!result) {
      LOG(WARNING) << "Failed to convert file_system_url to relative file "
                      "system path, type: "
                   << file_system_url.type();
      continue;
    }
    // The API only supports isolated files. It still works for directories,
    // as the value is ignored for existing entries.
    file_definition.is_directory = false;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      url::Origin::Create(source_url()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(
          &FileManagerPrivateInternalResolveIsolatedEntriesFunction::
              RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalResolveIsolatedEntriesFunction::
    RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  using extensions::api::file_manager_private_internal::EntryDescription;
  std::vector<EntryDescription> entries;

  for (const auto& definition : *entry_definition_list) {
    if (definition.error != base::File::FILE_OK) {
      LOG(WARNING) << "Error resolving file system URL: " << definition.error;
      continue;
    }
    EntryDescription entry;
    entry.file_system_name = definition.file_system_name;
    entry.file_system_root = definition.file_system_root_url;
    entry.file_full_path = "/" + definition.full_path.AsUTF8Unsafe();
    entry.file_is_directory = definition.is_directory;
    entries.push_back(std::move(entry));
  }

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           ResolveIsolatedEntries::Results::Create(entries)));
}

FileManagerPrivateInternalSearchFilesFunction::
    FileManagerPrivateInternalSearchFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSearchFilesFunction::Run() {
  using api::file_manager_private_internal::SearchFiles::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const auto& search_params = params->search_params;

  if (search_params.max_results < 0) {
    return RespondNow(Error("maxResults must be non-negative"));
  }

  ash::RecentSource::FileType file_type;
  if (!file_manager::util::ToRecentSourceFileType(search_params.category,
                                                  &file_type)) {
    return RespondNow(
        Error("Cannot convert category * to file type",
              api::file_manager_private::ToString(search_params.category)));
  }

  base::FilePath root_path;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string root_url = search_params.root_url.value_or("");
  if (root_url.empty()) {
    root_path = file_manager::util::GetMyFilesFolderForProfile(profile);
  } else {
    const scoped_refptr<storage::FileSystemContext> file_system_context =
        file_manager::util::GetFileSystemContextForRenderFrameHost(
            profile, render_frame_host());
    const storage::FileSystemURL url =
        file_system_context->CrackURLInFirstPartyContext(GURL(root_url));
    root_path = url.path();
  }

  size_t max_results =
      base::internal::checked_cast<size_t>(search_params.max_results);
  base::Time modified_time = base::Time::FromMillisecondsSinceUnixEpoch(
      search_params.modified_timestamp);

  // Barrier that collects results from the file search by name and image
  // search by (query) terms. Explicitly waits for 2 tasks to complete.
  auto barrier_callback = base::BarrierCallback<FileSearchResults>(
      2,
      base::BindOnce(
          &FileManagerPrivateInternalSearchFilesFunction::OnSearchByPatternDone,
          this));

  RunFileSearchByName(profile, root_path, search_params.query, modified_time,
                      file_type, max_results, barrier_callback);
  RunImageSearchByQuery(root_path, search_params.query, modified_time,
                        max_results, barrier_callback);

  return RespondLater();
}

void FileManagerPrivateInternalSearchFilesFunction::RunFileSearchByName(
    Profile* profile,
    base::FilePath root_path,
    const std::string& query,
    base::Time modified_time,
    ash::RecentSource::FileType file_type,
    size_t max_results,
    OnResultsReadyCallback callback) {
  // If trash is enabled for the given profile and by local user files policy,
  // generate all trash paths that are to be excluded when searching for
  // matching files.
  std::vector<base::FilePath> excluded_paths;
  if (file_manager::trash::IsTrashEnabledForProfile(profile)) {
    auto enabled_trash_locations =
        file_manager::trash::GenerateEnabledTrashLocationsForProfile(
            profile, /*base_path=*/base::FilePath());
    for (const auto& it : enabled_trash_locations) {
      excluded_paths.emplace_back(
          it.first.Append(it.second.relative_folder_path));
    }
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SearchByPattern, root_path, excluded_paths, query,
                     modified_time, file_type, max_results),
      std::move(callback));
}

void FileManagerPrivateInternalSearchFilesFunction::RunImageSearchByQuery(
    base::FilePath root_path,
    const std::string& query,
    base::Time modified_time,
    size_t max_results,
    OnResultsReadyCallback callback) {
  // If the feature is not enabled or the query is too short return empty match.
  std::u16string q16 = base::UTF8ToUTF16(query);
  if (!ash::features::IsFilesLocalImageSearchEnabled() ||
      !search_features::IsLauncherImageSearchEnabled() ||
      app_list::IsQueryTooShort(q16)) {
    std::move(callback).Run({});
    return;
  }

  app_list::LocalImageSearchServiceFactory::GetForBrowserContext(
      browser_context())
      ->Search(q16, max_results,
               base::BindOnce(&OnImageSearchDone, root_path, modified_time,
                              std::move(callback)));
}

void FileManagerPrivateInternalSearchFilesFunction::OnSearchByPatternDone(
    std::vector<FileSearchResults> all_results) {
  // Remove duplicates as image search and name search do not interact with each
  // other.
  FileSearchResults unique_results;
  std::set<base::FilePath> found;
  for (const auto& results : all_results) {
    for (const auto& [file_path, is_directory] : results) {
      if (base::Contains(found, file_path)) {
        continue;
      }
      found.insert(file_path);
      unique_results.emplace_back(file_path, is_directory);
    }
  }

  base::Value::List entries;
  for (const auto& result : unique_results) {
    std::string mount_name;
    std::string file_system_name;
    std::string full_path;
    if (!file_manager::util::ExtractMountNameFileSystemNameFullPath(
            result.first, &mount_name, &file_system_name, &full_path)) {
      DLOG(WARNING) << "Unable to extract details from "
                    << result.first.value();
      continue;
    }
    std::string fs_root =
        storage::GetExternalFileSystemRootURIString(source_url(), mount_name);

    base::Value::Dict entry;
    entry.Set("fileSystemName", file_system_name);
    entry.Set("fileSystemRoot", fs_root);
    entry.Set("fileFullPath", full_path);
    entry.Set("fileIsDirectory", result.second);
    entries.Append(std::move(entry));
  }

  Respond(WithArguments(std::move(entries)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDirectorySizeFunction::Run() {
  using extensions::api::file_manager_private_internal::GetDirectorySize::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->url.empty()) {
    return RespondNow(Error("File URL must be provided."));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  const storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)));
  if (!ash::FileSystemBackend::CanHandleURL(file_system_url)) {
    return RespondNow(
        Error("FileSystemBackend failed to handle the entry's url."));
  }
  if (file_system_url.type() != storage::kFileSystemTypeLocal &&
      file_system_url.type() != storage::kFileSystemTypeDriveFs) {
    return RespondNow(Error("Only local directories are supported."));
  }

  const base::FilePath root_path = file_manager::util::GetLocalPathFromURL(
      file_system_context, GURL(params->url));
  if (root_path.empty()) {
    return RespondNow(
        Error("Failed to get a local path from the entry's url."));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::ComputeDirectorySize, root_path),
      base::BindOnce(&FileManagerPrivateInternalGetDirectorySizeFunction::
                         OnDirectorySizeRetrieved,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalGetDirectorySizeFunction::
    OnDirectorySizeRetrieved(int64_t size) {
  Respond(WithArguments(static_cast<double>(size)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalStartIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::StartIOTask::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller()) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  std::vector<storage::FileSystemURL> source_urls;
  for (const std::string& url : params->urls) {
    GURL gurl(url);
    storage::FileSystemURL cracked_url =
        file_system_context->CrackURLInFirstPartyContext(gurl);
    if (!cracked_url.is_valid()) {
      return RespondNow(Error("Invalid source URL *", Redact(url)));
    }
    base::FilePath virtual_path;
    const bool result =
        file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, gurl, cracked_url.path(), &virtual_path);
    if (!result) {
      LOG(WARNING) << "Failed to convert file_system_url to relative file "
                      "system path, type: "
                   << cracked_url.type();
      continue;
    }
    cracked_url = mount_points->CreateCrackedFileSystemURL(
        cracked_url.storage_key(), storage::kFileSystemTypeExternal,
        virtual_path);
    source_urls.push_back(std::move(cracked_url));
  }

  auto type = IoTaskTypeToChromeEnum(params->type);
  if (!type) {
    return RespondNow(Error("Invalid I/O task type given."));
  }

  storage::FileSystemURL destination_folder_url;
  if (params->params.destination_folder_url) {
    destination_folder_url = file_system_context->CrackURLInFirstPartyContext(
        GURL(*(params->params.destination_folder_url)));
    if (!destination_folder_url.is_valid()) {
      return RespondNow(Error("Invalid destination folder url."));
    }
  }

  // Whether to display this IOTask notification, if undefined this should
  // default to true.
  bool show_notification = params->params.show_notification.value_or(true);

  // Check if Trash is enabled, this pref is mainly used by enterprise policy to
  // disable trash on a per profile basis.
  bool is_trash_enabled = false;
  if (profile && profile->GetPrefs()) {
    is_trash_enabled =
        profile->GetPrefs()->GetBoolean(ash::prefs::kFilesAppTrashEnabled);
  }

  std::unique_ptr<file_manager::io_task::IOTask> task;
  switch (type.value()) {
    case file_manager::io_task::OperationType::kCopy:
    case file_manager::io_task::OperationType::kMove:
      task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          type.value(), std::move(source_urls),
          std::move(destination_folder_url), profile, file_system_context,
          show_notification);
      break;
    case file_manager::io_task::OperationType::kZip:
      task = std::make_unique<file_manager::io_task::ZipIOTask>(
          std::move(source_urls), std::move(destination_folder_url), profile,
          file_system_context, show_notification);
      break;
    case file_manager::io_task::OperationType::kDelete:
      task = std::make_unique<file_manager::io_task::DeleteIOTask>(
          std::move(source_urls), file_system_context, show_notification);
      break;
    case file_manager::io_task::OperationType::kEmptyTrash:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::EmptyTrashIOTask>(
            blink::StorageKey::CreateFirstParty(
                render_frame_host()->GetLastCommittedOrigin()),
            profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kRestore:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::RestoreIOTask>(
            std::move(source_urls), profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kRestoreToDestination:
      if (is_trash_enabled) {
        task =
            std::make_unique<file_manager::io_task::RestoreToDestinationIOTask>(
                std::move(source_urls), std::move(destination_folder_url),
                profile, file_system_context,
                /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kTrash:
      if (is_trash_enabled) {
        task = std::make_unique<file_manager::io_task::TrashIOTask>(
            std::move(source_urls), profile, file_system_context,
            /*base_path=*/base::FilePath(), show_notification);
      }
      break;
    case file_manager::io_task::OperationType::kExtract:
      std::string password;
      if (params->params.password) {
        password = *params->params.password;
      }
      task = std::make_unique<file_manager::io_task::ExtractIOTask>(
          std::move(source_urls), std::move(password),
          std::move(destination_folder_url), profile, file_system_context,
          show_notification);
      break;
  }
  if (!task) {
    return RespondNow(Error("Invalid operation type: *",
                            api::file_manager_private::ToString(params->type)));
  }
  const auto taskId =
      volume_manager->io_task_controller()->Add(std::move(task));
  return RespondNow(WithArguments(static_cast<int>(taskId)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateCancelIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::CancelIOTask::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller()) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  if (params->task_id <= 0) {
    return RespondNow(Error("Invalid task id"));
  }

  volume_manager->io_task_controller()->Cancel(params->task_id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateResumeIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::ResumeIOTask::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller()) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  if (params->task_id <= 0) {
    return RespondNow(Error("Invalid task id"));
  }

  file_manager::io_task::ResumeParams io_task_resume_params;
  if (params->params.conflict_params) {
    io_task_resume_params.conflict_params.emplace();
    io_task_resume_params.conflict_params->conflict_resolve =
        params->params.conflict_params->conflict_resolve.value_or("");
    io_task_resume_params.conflict_params->conflict_apply_to_all =
        params->params.conflict_params->conflict_apply_to_all.value_or(false);
  }
  if (params->params.policy_params) {
    std::optional<policy::Policy> policy =
        ApiPolicyErrorTypeToChromeEnum(params->params.policy_params->type);
    if (policy.has_value()) {
      io_task_resume_params.policy_params.emplace();
      io_task_resume_params.policy_params->type = policy.value();
    }
  }

  volume_manager->io_task_controller()->Resume(
      params->task_id, std::move(io_task_resume_params));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateDismissIOTaskFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::DismissIOTask::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->task_id <= 0) {
    return RespondNow(Error("Invalid task id"));
  }

  policy::FilesPolicyNotificationManager* manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          browser_context());
  if (!manager) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't notify about task_id "
               << params->task_id;
    return RespondNow(NoArguments());
  }
  manager->OnErrorItemDismissed(params->task_id);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateShowPolicyDialogFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private::ShowPolicyDialog::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->task_id <= 0) {
    return RespondNow(Error("Invalid task id"));
  }

  policy::FilesDialogType type = ApiPolicyDialogTypeToChromeEnum(params->type);
  if (type == policy::FilesDialogType::kUnknown) {
    return RespondNow(Error("No dialog type passed for task_id *",
                            base::NumberToString(params->task_id)));
  }

  policy::FilesPolicyNotificationManager* manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          browser_context());
  if (!manager) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy dialog for task_id "
               << params->task_id;
    return RespondNow(NoArguments());
  }
  manager->ShowDialog(params->task_id, type);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateProgressPausedTasksFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  if (!volume_manager || !volume_manager->io_task_controller()) {
    return RespondNow(Error("Cannot find VolumeManager"));
  }

  volume_manager->io_task_controller()->ProgressPausedTasks();

  return RespondNow(NoArguments());
}

FileManagerPrivateInternalParseTrashInfoFilesFunction::
    FileManagerPrivateInternalParseTrashInfoFilesFunction() = default;

FileManagerPrivateInternalParseTrashInfoFilesFunction::
    ~FileManagerPrivateInternalParseTrashInfoFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalParseTrashInfoFilesFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::ParseTrashInfoFiles::
      Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* const profile = Profile::FromBrowserContext(browser_context());
  file_system_context_ =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  std::vector<base::FilePath> trash_info_paths;
  for (const std::string& url : params->urls) {
    storage::FileSystemURL cracked_url =
        file_system_context_->CrackURLInFirstPartyContext(GURL(url));
    if (!cracked_url.is_valid()) {
      return RespondNow(Error("Invalid source url."));
    }
    trash_info_paths.push_back(cracked_url.path());
  }

  validator_ = std::make_unique<file_manager::trash::TrashInfoValidator>(
      profile, /*base_path=*/base::FilePath());

  auto barrier_callback =
      base::BarrierCallback<file_manager::trash::ParsedTrashInfoDataOrError>(
          trash_info_paths.size(),
          base::BindOnce(
              &FileManagerPrivateInternalParseTrashInfoFilesFunction::
                  OnTrashInfoFilesParsed,
              this));

  for (const base::FilePath& path : trash_info_paths) {
    validator_->ValidateAndParseTrashInfo(std::move(path), barrier_callback);
  }

  return RespondLater();
}

void FileManagerPrivateInternalParseTrashInfoFilesFunction::
    OnTrashInfoFilesParsed(
        std::vector<file_manager::trash::ParsedTrashInfoDataOrError>
            parsed_data_or_error) {
  // The underlying trash service could potentially live longer than the Files
  // window that invoked this function, ensure the frame host and browser
  // context are alive before continuing.
  if (!render_frame_host() || !browser_context()) {
    LOG(WARNING) << "Parsing trashinfo files finished but no window available "
                    "to respond to";
    Respond(NoArguments());
    return;
  }

  file_manager::util::FileDefinitionList file_definition_list;
  std::vector<file_manager::trash::ParsedTrashInfoData> valid_data;
  url::Origin origin = render_frame_host()->GetLastCommittedOrigin();

  for (auto& trash_info_data_or_error : parsed_data_or_error) {
    if (!trash_info_data_or_error.has_value()) {
      LOG(ERROR) << "Failed parsing trashinfo file: "
                 << trash_info_data_or_error.error();
      continue;
    }

    file_manager::util::FileDefinition file_definition;
    if (!file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            Profile::FromBrowserContext(browser_context()), origin.GetURL(),
            trash_info_data_or_error.value().absolute_restore_path,
            &file_definition.virtual_path)) {
      LOG(ERROR) << "Failed to convert absolute path to relative path";
      continue;
    }

    file_definition_list.push_back(std::move(file_definition));
    valid_data.push_back(std::move(trash_info_data_or_error.value()));
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_system_context_, origin, std::move(file_definition_list),
      base::BindOnce(&FileManagerPrivateInternalParseTrashInfoFilesFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this, std::move(valid_data)));
}

void FileManagerPrivateInternalParseTrashInfoFilesFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::vector<file_manager::trash::ParsedTrashInfoData> parsed_data,
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK_EQ(parsed_data.size(), entry_definition_list->size());
  std::vector<api::file_manager_private_internal::ParsedTrashInfoFile> results;

  for (size_t i = 0; i < parsed_data.size(); ++i) {
    const auto& [trash_info_path, trashed_file_path, absolute_restore_path,
                 deletion_date] = parsed_data[i];
    api::file_manager_private_internal::ParsedTrashInfoFile info;

    info.restore_entry.file_system_name =
        entry_definition_list->at(i).file_system_name;
    info.restore_entry.file_system_root =
        entry_definition_list->at(i).file_system_root_url;
    info.restore_entry.file_full_path =
        base::FilePath("/")
            .Append(entry_definition_list->at(i).full_path)
            .value();
    info.restore_entry.file_is_directory =
        entry_definition_list->at(i).is_directory;
    info.trash_info_file_name = trash_info_path.BaseName().value();
    info.deletion_date =
        deletion_date.InMillisecondsFSinceUnixEpochIgnoringNull();

    results.push_back(std::move(info));
  }

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           ParseTrashInfoFiles::Results::Create(results)));
}

}  // namespace extensions
