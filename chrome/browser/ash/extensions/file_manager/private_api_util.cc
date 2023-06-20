// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/snapshot_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager_private = extensions::api::file_manager_private;

namespace file_manager::util {
namespace {

// The struct is used for GetSelectedFileInfo().
struct GetSelectedFileInfoParams {
  GetSelectedFileInfoLocalPathOption local_path_option;
  GetSelectedFileInfoCallback callback;
  std::vector<base::FilePath> file_paths;
  std::vector<ui::SelectedFileInfo> selected_files;
};

// The callback type for GetFileNativeLocalPathFor{Opening,Saving}. It receives
// the resolved local path when successful, and receives empty path for failure.
typedef base::OnceCallback<void(const base::FilePath&)> LocalPathCallback;

// Gets a resolved local file path of a non native |path| for file opening.
void GetFileNativeLocalPathForOpening(Profile* profile,
                                      const base::FilePath& path,
                                      LocalPathCallback callback) {
  VolumeManager::Get(profile)->snapshot_manager()->CreateManagedSnapshot(
      path, std::move(callback));
}

// Gets a resolved local file path of a non native |path| for file saving.
void GetFileNativeLocalPathForSaving(Profile* profile,
                                     const base::FilePath& path,
                                     LocalPathCallback callback) {
  // TODO(kinaba): For now, there are no writable non-local volumes.
  NOTREACHED();
  std::move(callback).Run(base::FilePath());
}

// Forward declarations of helper functions for GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(
    Profile* profile,
    std::unique_ptr<GetSelectedFileInfoParams> params,
    const base::FilePath& local_file_path);

void ContinueGetSelectedFileInfoWithDriveFsMetadata(
    Profile* profile,
    std::unique_ptr<GetSelectedFileInfoParams> params,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata);

// Part of GetSelectedFileInfo().
void GetSelectedFileInfoInternal(
    Profile* profile,
    std::unique_ptr<GetSelectedFileInfoParams> params) {
  DCHECK(profile);

  for (size_t i = params->selected_files.size(); i < params->file_paths.size();
       ++i) {
    const base::FilePath& file_path = params->file_paths[i];

    if (file_manager::util::IsUnderNonNativeLocalPath(profile, file_path)) {
      // When the caller of the select file dialog wants local file paths, and
      // the selected path does not point to a native local path (e.g., Drive,
      // MTP, or provided file system), we should resolve the path.
      switch (params->local_path_option) {
        case NO_LOCAL_PATH_RESOLUTION: {
          // Pass empty local path.
          params->selected_files.emplace_back(file_path, base::FilePath());

          GURL external_file_url =
              ash::CreateExternalFileURLFromPath(profile, file_path);
          if (!external_file_url.is_empty()) {
            params->selected_files.back().url.emplace(
                std::move(external_file_url));
          }
          break;
        }
        case NEED_LOCAL_PATH_FOR_OPENING: {
          GetFileNativeLocalPathForOpening(
              profile, file_path,
              base::BindOnce(&ContinueGetSelectedFileInfo, profile,
                             std::move(params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
        }
        case NEED_LOCAL_PATH_FOR_SAVING: {
          GetFileNativeLocalPathForSaving(
              profile, file_path,
              base::BindOnce(&ContinueGetSelectedFileInfo, profile,
                             std::move(params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
        }
      }
    } else {
      // Hosted docs can only accessed by navigating to their URLs. Get the
      // metadata for the file from DriveFS and populate the |url| field in the
      // SelectedFileInfo.
      if (drive::util::HasHostedDocumentExtension(file_path)) {
        auto* integration_service =
            drive::util::GetIntegrationServiceByProfile(profile);
        base::FilePath drive_mount_relative_path;
        if (integration_service && integration_service->GetDriveFsInterface() &&
            integration_service->GetRelativeDrivePath(
                file_path, &drive_mount_relative_path)) {
          integration_service->GetDriveFsInterface()->GetMetadata(
              drive_mount_relative_path,
              base::BindOnce(&ContinueGetSelectedFileInfoWithDriveFsMetadata,
                             profile, std::move(params)));
          return;
        }
      }
      params->selected_files.emplace_back(file_path, file_path);
    }
  }

  // Populate the virtual path for any files on a external mount point. This
  // lets consumers that are capable of using a virtual path use this rather
  // than file_path, which can make certain operations more efficient.
  for (auto& file_info : params->selected_files) {
    auto* external_mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    base::FilePath virtual_path;
    if (external_mount_points->GetVirtualPath(file_info.file_path,
                                              &virtual_path)) {
      file_info.virtual_path.emplace(std::move(virtual_path));
    } else {
      LOG(ERROR) << "Failed to get external virtual path: "
                 << file_info.file_path;
    }
  }

  std::move(params->callback).Run(params->selected_files);
}

// Part of GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(
    Profile* profile,
    std::unique_ptr<GetSelectedFileInfoParams> params,
    const base::FilePath& local_path) {
  if (local_path.empty()) {
    std::move(params->callback).Run(std::vector<ui::SelectedFileInfo>());
    return;
  }
  const int index = params->selected_files.size();
  const base::FilePath& file_path = params->file_paths[index];
  params->selected_files.emplace_back(file_path, local_path);
  GetSelectedFileInfoInternal(profile, std::move(params));
}

// Part of GetSelectedFileInfo().
void ContinueGetSelectedFileInfoWithDriveFsMetadata(
    Profile* profile,
    std::unique_ptr<GetSelectedFileInfoParams> params,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  const int index = params->selected_files.size();
  const auto& path = params->file_paths[index];
  params->selected_files.emplace_back(path, path);
  if (metadata && drivefs::IsHosted(metadata->type) &&
      !metadata->alternate_url.empty()) {
    params->selected_files.back().url.emplace(
        std::move(metadata->alternate_url));
  }
  GetSelectedFileInfoInternal(profile, std::move(params));
}

std::string GetShareUrlFromAlternateUrl(const GURL& alternate_url) {
  // Set |share_url| to a modified version of |alternate_url| that opens the
  // sharing dialog for files and folders (add ?userstoinvite="" to the URL).
  GURL::Replacements replacements;
  std::string new_query =
      (alternate_url.has_query() ? alternate_url.query() + "&" : "") +
      "userstoinvite=%22%22";
  replacements.SetQueryStr(new_query);

  return alternate_url.ReplaceComponents(replacements).spec();
}

extensions::api::file_manager_private::VmType VmTypeToJs(
    guest_os::VmType vm_type) {
  switch (vm_type) {
    case guest_os::VmType::TERMINA:
      return extensions::api::file_manager_private::VM_TYPE_TERMINA;
    case guest_os::VmType::PLUGIN_VM:
      return extensions::api::file_manager_private::VM_TYPE_PLUGIN_VM;
    case guest_os::VmType::BOREALIS:
      return extensions::api::file_manager_private::VM_TYPE_BOREALIS;
    case guest_os::VmType::BRUSCHETTA:
      return extensions::api::file_manager_private::VM_TYPE_BRUSCHETTA;
    case guest_os::VmType::ARCVM:
      return extensions::api::file_manager_private::VM_TYPE_ARCVM;
    case guest_os::VmType::UNKNOWN:
    case guest_os::VmType::VmType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case guest_os::VmType::VmType_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return extensions::api::file_manager_private::VM_TYPE_NONE;
  }
}

extensions::api::file_manager_private::BulkPinStage DrivefsPinStageToJs(
    drivefs::pinning::Stage stage) {
  switch (stage) {
    case drivefs::pinning::Stage::kStopped:
      return extensions::api::file_manager_private::BULK_PIN_STAGE_STOPPED;
    case drivefs::pinning::Stage::kPausedOffline:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_PAUSED_OFFLINE;
    case drivefs::pinning::Stage::kPausedBatterySaver:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_PAUSED_BATTERY_SAVER;
    case drivefs::pinning::Stage::kGettingFreeSpace:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_GETTING_FREE_SPACE;
    case drivefs::pinning::Stage::kListingFiles:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_LISTING_FILES;
    case drivefs::pinning::Stage::kSyncing:
      return extensions::api::file_manager_private::BULK_PIN_STAGE_SYNCING;
    case drivefs::pinning::Stage::kSuccess:
      return extensions::api::file_manager_private::BULK_PIN_STAGE_SUCCESS;
    case drivefs::pinning::Stage::kCannotGetFreeSpace:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_CANNOT_GET_FREE_SPACE;
    case drivefs::pinning::Stage::kCannotListFiles:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_CANNOT_LIST_FILES;
    case drivefs::pinning::Stage::kNotEnoughSpace:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_NOT_ENOUGH_SPACE;
    case drivefs::pinning::Stage::kCannotEnableDocsOffline:
      return extensions::api::file_manager_private::
          BULK_PIN_STAGE_CANNOT_ENABLE_DOCS_OFFLINE;
  }
  NOTREACHED();
  return extensions::api::file_manager_private::BULK_PIN_STAGE_NONE;
}

bool IsPinManagerSyncingForProfile(drivefs::pinning::PinManager* pin_manager) {
  if (!pin_manager) {
    return false;
  }
  if (pin_manager->GetProgress().stage !=
      drivefs::pin_manager_types::mojom::Stage::kSyncing) {
    return false;
  }
  return true;
}

drivefs::pinning::PinManager* GetPinManager(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->IsMounted()) {
    return nullptr;
  }

  return integration_service->GetPinManager();
}

bool IsPathUnderMyDrive(const base::FilePath& relative_path) {
  return base::FilePath("/")
      .Append(drive::util::kDriveMyDriveRootDirName)
      .IsParent(relative_path);
}

}  // namespace

// Creates an instance and starts the process.
void SingleEntryPropertiesGetterForDriveFs::Start(
    const storage::FileSystemURL& file_system_url,
    Profile* const profile,
    const std::set<extensions::api::file_manager_private::EntryPropertyName>
        requested_properties,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SingleEntryPropertiesGetterForDriveFs* instance =
      new SingleEntryPropertiesGetterForDriveFs(
          file_system_url, profile, requested_properties, std::move(callback));
  instance->StartProcess();

  // The instance will be destroyed by itself.
}

SingleEntryPropertiesGetterForDriveFs::SingleEntryPropertiesGetterForDriveFs(
    const storage::FileSystemURL& file_system_url,
    Profile* const profile,
    const std::set<extensions::api::file_manager_private::EntryPropertyName>
        requested_properties,
    ResultCallback callback)
    : callback_(std::move(callback)),
      file_system_url_(file_system_url),
      running_profile_(profile),
      requested_properties_(requested_properties),
      properties_(std::make_unique<
                  extensions::api::file_manager_private::EntryProperties>()) {
  DCHECK(callback_);
  DCHECK(profile);
}

SingleEntryPropertiesGetterForDriveFs::
    ~SingleEntryPropertiesGetterForDriveFs() = default;

void SingleEntryPropertiesGetterForDriveFs::StartProcess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(running_profile_);
  if (!integration_service || !integration_service->IsMounted()) {
    CompleteGetEntryProperties(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }
  if (!integration_service->GetRelativeDrivePath(file_system_url_.path(),
                                                 &relative_path_)) {
    CompleteGetEntryProperties(drive::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface) {
    CompleteGetEntryProperties(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }

  file_manager::EventRouter* event_router =
      file_manager::EventRouterFactory::GetForProfile(running_profile_);
  if (ash::features::IsInlineSyncStatusEnabled() && event_router) {
    drivefs::SyncState sync_state =
        ash::features::IsInlineSyncStatusProgressEventsEnabled()
            ? event_router->GetDriveSyncStateForPath(file_system_url_.path())
            : integration_service->GetSyncStateForPath(file_system_url_.path());
    properties_->progress = sync_state.progress;
    switch (sync_state.status) {
      case drivefs::SyncStatus::kQueued:
        properties_->sync_status = file_manager_private::SYNC_STATUS_QUEUED;
        break;
      case drivefs::SyncStatus::kInProgress:
        properties_->sync_status =
            file_manager_private::SYNC_STATUS_IN_PROGRESS;
        break;
      case drivefs::SyncStatus::kError:
        properties_->sync_status = file_manager_private::SYNC_STATUS_ERROR;
        break;
      default:
        properties_->sync_status = file_manager_private::SYNC_STATUS_NOT_FOUND;
        break;
    }

    std::set<extensions::api::file_manager_private::EntryPropertyName>
        remote_requests;
    base::ranges::set_difference(
        requested_properties_, locally_available_properties_,
        std::inserter(remote_requests, remote_requests.end()));

    // If only locally available metadata was requested (sync status and
    // progress) we don't need to request further metadata from DriveFS.
    // Note: for backwards compatibility, not requesting any properties is
    // currently considered the same as requesting all properties.
    if (!requested_properties_.empty() && remote_requests.empty()) {
      CompleteGetEntryProperties(drive::FILE_ERROR_OK);
      return;
    }
  }

  drivefs_interface->GetMetadata(
      relative_path_,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&SingleEntryPropertiesGetterForDriveFs::OnGetFileInfo,
                         weak_ptr_factory_.GetWeakPtr()),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
}

void SingleEntryPropertiesGetterForDriveFs::OnGetFileInfo(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!metadata) {
    CompleteGetEntryProperties(error);
    return;
  }

  properties_->size = metadata->size;
  properties_->present = metadata->available_offline;
  properties_->dirty = metadata->dirty;
  properties_->hosted = drivefs::IsHosted(metadata->type);

  properties_->available_offline =
      metadata->available_offline || *properties_->hosted;
  properties_->available_when_metered =
      metadata->available_offline || *properties_->hosted;
  properties_->pinned = metadata->pinned;

  if (drive::util::IsDriveFsBulkPinningEnabled(running_profile_)) {
    properties_->available_offline =
        (drivefs::IsHosted(metadata->type) &&
         !drive::util::IsPinnableGDocMimeType(metadata->content_mime_type))
            ? false
            : metadata->available_offline;
    properties_->available_when_metered = properties_->available_offline;
    properties_->pinned = metadata->pinned;

    if (drivefs::pinning::PinManager* const pin_manager =
            GetPinManager(running_profile_);
        IsPinManagerSyncingForProfile(pin_manager) &&
        IsPathUnderMyDrive(relative_path_)) {
      if (metadata->type == drivefs::mojom::FileMetadata::Type::kDirectory &&
          !pin_manager->IsUntrackedPath(file_system_url_.path())) {
        // Folders can't be pinned automatically to provide a way to intercept
        // items being added to these folders. However items in the folders will
        // be pinned, so to ensure the UI shows these folders as available
        // offline, return these items as pinned and available offline. This
        // should not include shortcuts and only cover directories that are
        // parented at "My drive" (e.g. no Shared drives).
        properties_->available_when_metered = true;
        properties_->available_offline = true;
        properties_->pinned = true;
      } else if (drive::util::IsPinnableGDocMimeType(
                     metadata->content_mime_type)) {
        // When bulk pinning is enabled, hosted files should reflect the pinned
        // state as their available offline state.
        properties_->pinned = properties_->available_offline;
      }
    }
  }

  properties_->shared = metadata->shared;
  properties_->starred = metadata->starred;

  if (metadata->modification_time != base::Time()) {
    properties_->modification_time = metadata->modification_time.ToJsTime();
  }
  if (metadata->last_viewed_by_me_time != base::Time()) {
    properties_->modification_by_me_time =
        metadata->last_viewed_by_me_time.ToJsTime();
  }
  if (!metadata->content_mime_type.empty()) {
    properties_->content_mime_type = metadata->content_mime_type;
  }
  if (!metadata->custom_icon_url.empty()) {
    properties_->custom_icon_url = std::move(metadata->custom_icon_url);
  }
  if (!metadata->alternate_url.empty()) {
    properties_->alternate_url = std::move(metadata->alternate_url);
    properties_->share_url =
        GetShareUrlFromAlternateUrl(GURL(*properties_->alternate_url));
  }
  if (metadata->image_metadata) {
    properties_->image_height = metadata->image_metadata->height;
    properties_->image_width = metadata->image_metadata->width;
    properties_->image_rotation = metadata->image_metadata->rotation;
  }

  properties_->can_delete = metadata->capabilities->can_delete;
  properties_->can_rename = metadata->capabilities->can_rename;
  properties_->can_add_children = metadata->capabilities->can_add_children;

  // Only set the |can_copy| capability for hosted documents; for other files,
  // we must have read access, so |can_copy| is implicitly true.
  properties_->can_copy =
      !*properties_->hosted || metadata->capabilities->can_copy;
  properties_->can_share = metadata->capabilities->can_share;

  properties_->can_pin =
      metadata->can_pin == drivefs::mojom::FileMetadata::CanPinStatus::kOk;

  if (drivefs::IsAFile(metadata->type)) {
    properties_->thumbnail_url =
        base::StrCat({"drivefs:", file_system_url_.ToGURL().spec()});
    properties_->cropped_thumbnail_url = *properties_->thumbnail_url;
  }

  if (metadata->folder_feature) {
    properties_->is_machine_root = metadata->folder_feature->is_machine_root;
    properties_->is_external_media =
        metadata->folder_feature->is_external_media;
    properties_->is_arbitrary_sync_folder =
        metadata->folder_feature->is_arbitrary_sync_folder;
  }

  if (metadata->shortcut_details) {
    properties_->shortcut =
        (metadata->shortcut_details->target_lookup_status !=
         drivefs::mojom::ShortcutDetails::LookupStatus::kUnknown);
  } else {
    properties_->shortcut = false;
  }

  CompleteGetEntryProperties(drive::FILE_ERROR_OK);
}

void SingleEntryPropertiesGetterForDriveFs::CompleteGetEntryProperties(
    drive::FileError error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);

  std::move(callback_).Run(std::move(properties_),
                           drive::FileErrorToBaseFileError(error));
  content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
}

void FillIconSet(file_manager_private::IconSet* output,
                 const ash::file_system_provider::IconSet& input) {
  DCHECK(output);
  using ash::file_system_provider::IconSet;
  if (input.HasIcon(IconSet::IconSize::SIZE_16x16)) {
    output->icon16x16_url = input.GetIcon(IconSet::IconSize::SIZE_16x16).spec();
  }
  if (input.HasIcon(IconSet::IconSize::SIZE_32x32)) {
    output->icon32x32_url = input.GetIcon(IconSet::IconSize::SIZE_32x32).spec();
  }
}

void VolumeToVolumeMetadata(
    Profile* profile,
    const Volume& volume,
    file_manager_private::VolumeMetadata* volume_metadata) {
  DCHECK(volume_metadata);

  volume_metadata->volume_id = volume.volume_id();

  // TODO(kinaba): fill appropriate information once multi-profile support is
  // implemented.
  volume_metadata->profile.display_name = profile->GetProfileUserName();
  volume_metadata->profile.is_current_profile = true;

  if (!volume.source_path().empty()) {
    volume_metadata->source_path = volume.source_path().AsUTF8Unsafe();
  }
  if (!volume.remote_mount_path().empty()) {
    volume_metadata->remote_mount_path = volume.remote_mount_path().value();
  }

  switch (volume.source()) {
    case SOURCE_FILE:
      volume_metadata->source = file_manager_private::SOURCE_FILE;
      break;
    case SOURCE_DEVICE:
      volume_metadata->source = file_manager_private::SOURCE_DEVICE;
      volume_metadata->is_read_only_removable_device =
          volume.is_read_only_removable_device();
      break;
    case SOURCE_NETWORK:
      volume_metadata->source =
          extensions::api::file_manager_private::SOURCE_NETWORK;
      break;
    case SOURCE_SYSTEM:
      volume_metadata->source =
          extensions::api::file_manager_private::SOURCE_SYSTEM;
      break;
  }

  volume_metadata->configurable = volume.configurable();
  volume_metadata->watchable = volume.watchable();

  if (volume.type() == VOLUME_TYPE_PROVIDED) {
    volume_metadata->provider_id = volume.provider_id().ToString();
    volume_metadata->file_system_id = volume.file_system_id();
  }

  FillIconSet(&volume_metadata->icon_set, volume.icon_set());

  volume_metadata->volume_label = volume.volume_label();
  volume_metadata->disk_file_system_type = volume.file_system_type();
  volume_metadata->drive_label = volume.drive_label();

  switch (volume.type()) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_DRIVE;
      break;
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_DOWNLOADS;
      break;
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_REMOVABLE;
      break;
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_ARCHIVE;
      break;
    case VOLUME_TYPE_PROVIDED:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_PROVIDED;
      break;
    case VOLUME_TYPE_MTP:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_MTP;
      break;
    case VOLUME_TYPE_MEDIA_VIEW:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_MEDIA_VIEW;
      break;
    case VOLUME_TYPE_CROSTINI:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_CROSTINI;
      break;
    case VOLUME_TYPE_ANDROID_FILES:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_ANDROID_FILES;
      break;
    case VOLUME_TYPE_DOCUMENTS_PROVIDER:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_DOCUMENTS_PROVIDER;
      break;
    case VOLUME_TYPE_TESTING:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_TESTING;
      break;
    case VOLUME_TYPE_SMB:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_SMB;
      break;
    case VOLUME_TYPE_SYSTEM_INTERNAL:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_SYSTEM_INTERNAL;
      break;
    case VOLUME_TYPE_GUEST_OS:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_GUEST_OS;
      break;
    case NUM_VOLUME_TYPE:
      NOTREACHED();
      break;
  }

  // Fill device_type iff the volume is removable partition.
  if (volume.type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    switch (volume.device_type()) {
      case ash::DeviceType::kUnknown:
        volume_metadata->device_type =
            file_manager_private::DEVICE_TYPE_UNKNOWN;
        break;
      case ash::DeviceType::kUSB:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_USB;
        break;
      case ash::DeviceType::kSD:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_SD;
        break;
      case ash::DeviceType::kOpticalDisc:
      case ash::DeviceType::kDVD:
        volume_metadata->device_type =
            file_manager_private::DEVICE_TYPE_OPTICAL;
        break;
      case ash::DeviceType::kMobile:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_MOBILE;
        break;
    }
    volume_metadata->device_path = volume.storage_device_path().AsUTF8Unsafe();
    volume_metadata->is_parent_device = volume.is_parent();
  } else {
    volume_metadata->device_type = file_manager_private::DEVICE_TYPE_NONE;
  }

  volume_metadata->is_read_only = volume.is_read_only();
  volume_metadata->has_media = volume.has_media();
  volume_metadata->hidden = volume.hidden();

  switch (volume.mount_condition()) {
    default:
      LOG(ERROR) << "Unexpected mount condition: " << volume.mount_condition();
      [[fallthrough]];
    case ash::MountError::kSuccess:
      volume_metadata->mount_condition = file_manager_private::MOUNT_ERROR_NONE;
      break;
    case ash::MountError::kUnknownFilesystem:
      volume_metadata->mount_condition =
          file_manager_private::MOUNT_ERROR_UNKNOWN_FILESYSTEM;
      break;
    case ash::MountError::kUnsupportedFilesystem:
      volume_metadata->mount_condition =
          file_manager_private::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
      break;
  }

  // If the context is known, then pass it.
  switch (volume.mount_context()) {
    case MOUNT_CONTEXT_USER:
      volume_metadata->mount_context = file_manager_private::MOUNT_CONTEXT_USER;
      break;
    case MOUNT_CONTEXT_AUTO:
      volume_metadata->mount_context = file_manager_private::MOUNT_CONTEXT_AUTO;
      break;
    case MOUNT_CONTEXT_UNKNOWN:
      break;
  }

  if (volume.vm_type()) {
    volume_metadata->vm_type = VmTypeToJs(*volume.vm_type());
  }
}

base::FilePath GetLocalPathFromURL(content::RenderFrameHost* render_frame_host,
                                   Profile* profile,
                                   const GURL& url) {
  DCHECK(render_frame_host);
  DCHECK(profile);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderFrameHost(profile, render_frame_host);

  const storage::FileSystemURL filesystem_url(
      file_system_context->CrackURLInFirstPartyContext(url));
  base::FilePath path;
  if (!ash::FileSystemBackend::CanHandleURL(filesystem_url)) {
    return base::FilePath();
  }
  return filesystem_url.path();
}

void GetSelectedFileInfo(content::RenderFrameHost* render_frame_host,
                         Profile* profile,
                         const std::vector<GURL>& file_urls,
                         GetSelectedFileInfoLocalPathOption local_path_option,
                         GetSelectedFileInfoCallback callback) {
  DCHECK(render_frame_host);
  DCHECK(profile);

  std::unique_ptr<GetSelectedFileInfoParams> params(
      new GetSelectedFileInfoParams);
  params->local_path_option = local_path_option;
  params->callback = std::move(callback);

  for (const GURL& url : file_urls) {
    base::FilePath path = GetLocalPathFromURL(render_frame_host, profile, url);
    if (!path.empty()) {
      DVLOG(1) << "Selected: file path: " << path;
      params->file_paths.push_back(std::move(path));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetSelectedFileInfoInternal, profile, std::move(params)));
}

drive::EventLogger* GetLogger(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return service ? service->event_logger() : nullptr;
}

std::vector<extensions::api::file_manager_private::MountableGuest>
CreateMountableGuestList(Profile* profile) {
  auto* registry =
      guest_os::GuestOsService::GetForProfile(profile)->MountProviderRegistry();
  std::vector<file_manager_private::MountableGuest> guests;
  for (const auto id : registry->List()) {
    file_manager_private::MountableGuest guest;
    auto* provider = registry->Get(id);
    guest.id = id;
    guest.display_name = provider->DisplayName();
    guest.vm_type = VmTypeToJs(provider->vm_type());
    guests.push_back(std::move(guest));
  }
  return guests;
}

bool ToRecentSourceFileType(
    extensions::api::file_manager_private::FileCategory input_category,
    ash::RecentSource::FileType* output_type) {
  switch (input_category) {
    case extensions::api::file_manager_private::FILE_CATEGORY_NONE:
      // The FileCategory is an optional parameter. Thus we convert NONE to All.
      // If the calling code does not specify the restrictions on the category
      // we do not enforce then.
    case extensions::api::file_manager_private::FILE_CATEGORY_ALL:
      *output_type = ash::RecentSource::FileType::kAll;
      return true;
    case extensions::api::file_manager_private::FILE_CATEGORY_AUDIO:
      *output_type = ash::RecentSource::FileType::kAudio;
      return true;
    case extensions::api::file_manager_private::FILE_CATEGORY_IMAGE:
      *output_type = ash::RecentSource::FileType::kImage;
      return true;
    case extensions::api::file_manager_private::FILE_CATEGORY_VIDEO:
      *output_type = ash::RecentSource::FileType::kVideo;
      return true;
    case extensions::api::file_manager_private::FILE_CATEGORY_DOCUMENT:
      *output_type = ash::RecentSource::FileType::kDocument;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

extensions::api::file_manager_private::BulkPinProgress BulkPinProgressToJs(
    const drivefs::pinning::Progress& progress) {
  extensions::api::file_manager_private::BulkPinProgress result;
  result.stage = DrivefsPinStageToJs(progress.stage);
  result.free_space_bytes = progress.free_space;
  result.required_space_bytes = progress.required_space;
  result.bytes_to_pin = progress.bytes_to_pin;
  result.pinned_bytes = progress.pinned_bytes;
  result.files_to_pin = progress.files_to_pin;
  result.remaining_seconds = progress.remaining_seconds;
  return result;
}

}  // namespace file_manager::util
