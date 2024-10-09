// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
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
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_info.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace file_manager::util {
namespace {

namespace fmp = extensions::api::file_manager_private;

// The struct is used for GetSelectedFileInfo().
struct GetSelectedFileInfoParams {
  GetSelectedFileInfoLocalPathOption local_path_option;
  GetSelectedFileInfoCallback callback;
  std::vector<base::FilePath> file_paths;
  std::vector<ui::SelectedFileInfo> selected_files;
};

// The callback type for GetFileNativeLocalPathFor{Opening,Saving}. It receives
// the resolved local path when successful, and receives empty path for failure.
using LocalPathCallback = base::OnceCallback<void(const base::FilePath&)>;

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
  NOTREACHED_IN_MIGRATION();
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
      // Hosted docs and encrypted files can only accessed by navigating
      // to their URLs. Get the metadata for the file from DriveFS and
      // if needed populate the |url| field in the SelectedFileInfo.
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
  if (metadata &&
      (drivefs::IsHosted(metadata->type) ||
       drive::util::IsEncryptedMimeType(metadata->content_mime_type)) &&
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

fmp::VmType VmTypeToJs(guest_os::VmType vm_type) {
  switch (vm_type) {
    case guest_os::VmType::TERMINA:
      return fmp::VmType::kTermina;
    case guest_os::VmType::PLUGIN_VM:
      return fmp::VmType::kPluginVm;
    case guest_os::VmType::BOREALIS:
      return fmp::VmType::kBorealis;
    case guest_os::VmType::BRUSCHETTA:
      return fmp::VmType::kBruschetta;
    case guest_os::VmType::ARCVM:
      return fmp::VmType::kArcvm;
    case guest_os::VmType::BAGUETTE:
      // Baguette currently isn't hooked up to file manager
      return fmp::VmType::kNone;
    case guest_os::VmType::UNKNOWN:
    case guest_os::VmType::VmType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case guest_os::VmType::VmType_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return fmp::VmType::kNone;
  }
}

fmp::BulkPinStage DrivefsPinStageToJs(drivefs::pinning::Stage stage) {
  switch (stage) {
    using enum drivefs::pinning::Stage;
    case kStopped:
      return fmp::BulkPinStage::kStopped;
    case kPausedOffline:
      return fmp::BulkPinStage::kPausedOffline;
    case kPausedBatterySaver:
      return fmp::BulkPinStage::kPausedBatterySaver;
    case kGettingFreeSpace:
      return fmp::BulkPinStage::kGettingFreeSpace;
    case kListingFiles:
      return fmp::BulkPinStage::kListingFiles;
    case kSyncing:
      return fmp::BulkPinStage::kSyncing;
    case kSuccess:
      return fmp::BulkPinStage::kSuccess;
    case kNotEnoughSpace:
      return fmp::BulkPinStage::kNotEnoughSpace;
    case kCannotGetFreeSpace:
      return fmp::BulkPinStage::kCannotGetFreeSpace;
    case kCannotListFiles:
      return fmp::BulkPinStage::kCannotListFiles;
    case kCannotEnableDocsOffline:
      return fmp::BulkPinStage::kCannotEnableDocsOffline;
  }

  NOTREACHED_IN_MIGRATION();
  return fmp::BulkPinStage::kNone;
}

bool IsBulkPinningEnabledForProfile(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(
      drive::prefs::kDriveFsBulkPinningEnabled);
}

drivefs::pinning::PinningManager* GetPinningManager(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->IsMounted()) {
    return nullptr;
  }

  return integration_service->GetPinningManager();
}

bool IsPathUnderMyDrive(const base::FilePath& relative_path) {
  return base::FilePath("/")
      .Append(drive::util::kDriveMyDriveRootDirName)
      .IsParent(relative_path);
}

// Part of GURLToEntryData().
void GURLToEntryDataOnResolve(
    std::string entry_name,
    const GURL& url,
    base::OnceCallback<void(base::FileErrorOr<fmp::EntryData>)> callback,
    base::File::Error result,
    const storage::FileSystemInfo& file_system_info,
    const base::FilePath& file_path,
    storage::FileSystemContext::ResolvedEntryType type) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
    return;
  }
  fmp::EntryData entry_data;
  switch (type) {
    case storage::FileSystemContext::RESOLVED_ENTRY_FILE:
      entry_data.is_directory = false;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_DIRECTORY:
      entry_data.is_directory = true;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_NOT_FOUND:
      std::move(callback).Run(
          base::unexpected(base::File::FILE_ERROR_NOT_FOUND));
      return;
  }
  entry_data.name = std::move(entry_name);
  entry_data.entry_url = url.spec();
  entry_data.filesystem.name = file_system_info.name;
  entry_data.filesystem.root_url = file_system_info.root_url.spec();
  std::move(callback).Run(std::move(entry_data));
}

}  // namespace

// Creates an instance and starts the process.
void SingleEntryPropertiesGetterForDriveFs::Start(
    const storage::FileSystemURL& file_system_url,
    Profile* const profile,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SingleEntryPropertiesGetterForDriveFs* instance =
      new SingleEntryPropertiesGetterForDriveFs(file_system_url, profile,
                                                std::move(callback));
  instance->StartProcess();

  // The instance will be destroyed by itself.
}

SingleEntryPropertiesGetterForDriveFs::SingleEntryPropertiesGetterForDriveFs(
    const storage::FileSystemURL& file_system_url,
    Profile* const profile,
    ResultCallback callback)
    : callback_(std::move(callback)),
      file_system_url_(file_system_url),
      running_profile_(profile),
      properties_(std::make_unique<fmp::EntryProperties>()) {
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
  if (event_router) {
    drivefs::SyncState sync_state =
        event_router->GetDriveSyncStateForPath(file_system_url_.path());
    properties_->progress = sync_state.progress;
    switch (sync_state.status) {
      using enum drivefs::SyncStatus;
      case kQueued:
        properties_->sync_status = fmp::SyncStatus::kQueued;
        break;
      case kInProgress:
        properties_->sync_status = fmp::SyncStatus::kInProgress;
        break;
      case kError:
        properties_->sync_status = fmp::SyncStatus::kError;
        break;
      default:
        properties_->sync_status = fmp::SyncStatus::kNotFound;
        break;
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
  // Dirty files have unsynced changes hence will eventually get queued for
  // syncing. Let's make sure we report them as queued as soon as possible.
  if (metadata->dirty) {
    properties_->sync_status = file_manager_private::SyncStatus::kQueued;
  }
  properties_->hosted = drivefs::IsHosted(metadata->type);

  properties_->available_offline =
      metadata->available_offline ||
      drive::util::IsEncryptedMimeType(metadata->content_mime_type) ||
      *properties_->hosted;
  properties_->available_when_metered =
      metadata->available_offline || *properties_->hosted;
  properties_->pinned = metadata->pinned;

  if (drive::util::IsDriveFsBulkPinningAvailable(running_profile_)) {
    properties_->available_offline =
        (drivefs::IsHosted(metadata->type) &&
         !drive::util::IsPinnableGDocMimeType(metadata->content_mime_type))
            ? false
            : metadata->available_offline;
    properties_->available_when_metered = properties_->available_offline;
    properties_->pinned = metadata->pinned;

    if (IsBulkPinningEnabledForProfile(running_profile_) &&
        IsPathUnderMyDrive(relative_path_)) {
      drivefs::pinning::PinningManager* const pinning_manager =
          GetPinningManager(running_profile_);

      const auto stable_id =
          drivefs::pinning::PinningManager::Id(metadata->stable_id);
      if (properties_->sync_status ==
              file_manager_private::SyncStatus::kNotFound &&
          pinning_manager->IsTrackedAndUnpinned(stable_id)) {
        // The `PinningManager` maintains a list of 200 items that it pins, if
        // the item is not within these 200 items it will eventually be pinned,
        // but does not enter into a queued state just yet. This ensures the
        // queued state is reflected for items that will be pinned but haven't
        // called `SetPinned` yet.
        properties_->sync_status = file_manager_private::SyncStatus::kQueued;
      }

      if (drive::util::IsPinnableGDocMimeType(metadata->content_mime_type)) {
        // When bulk pinning is enabled, hosted files should reflect the pinned
        // state as their available offline state.
        properties_->pinned = properties_->available_offline;
      }
    }
  }

  properties_->shared = metadata->shared;
  properties_->starred = metadata->starred;

  if (metadata->modification_time != base::Time()) {
    properties_->modification_time =
        metadata->modification_time.InMillisecondsFSinceUnixEpoch();
  }
  if (metadata->last_viewed_by_me_time != base::Time()) {
    properties_->modification_by_me_time =
        metadata->last_viewed_by_me_time.InMillisecondsFSinceUnixEpoch();
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

void FillIconSet(fmp::IconSet* output,
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

void VolumeToVolumeMetadata(Profile* profile,
                            const Volume& volume,
                            fmp::VolumeMetadata* volume_metadata) {
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
      volume_metadata->source = fmp::Source::kFile;
      break;
    case SOURCE_DEVICE:
      volume_metadata->source = fmp::Source::kDevice;
      volume_metadata->is_read_only_removable_device =
          volume.is_read_only_removable_device();
      break;
    case SOURCE_NETWORK:
      volume_metadata->source = fmp::Source::kNetwork;
      break;
    case SOURCE_SYSTEM:
      volume_metadata->source = fmp::Source::kSystem;
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
      volume_metadata->volume_type = fmp::VolumeType::kDrive;
      break;
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      volume_metadata->volume_type = fmp::VolumeType::kDownloads;
      break;
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      volume_metadata->volume_type = fmp::VolumeType::kRemovable;
      break;
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      volume_metadata->volume_type = fmp::VolumeType::kArchive;
      break;
    case VOLUME_TYPE_PROVIDED:
      volume_metadata->volume_type = fmp::VolumeType::kProvided;
      break;
    case VOLUME_TYPE_MTP:
      volume_metadata->volume_type = fmp::VolumeType::kMtp;
      break;
    case VOLUME_TYPE_MEDIA_VIEW:
      volume_metadata->volume_type = fmp::VolumeType::kMediaView;
      break;
    case VOLUME_TYPE_CROSTINI:
      volume_metadata->volume_type = fmp::VolumeType::kCrostini;
      break;
    case VOLUME_TYPE_ANDROID_FILES:
      volume_metadata->volume_type = fmp::VolumeType::kAndroidFiles;
      break;
    case VOLUME_TYPE_DOCUMENTS_PROVIDER:
      volume_metadata->volume_type = fmp::VolumeType::kDocumentsProvider;
      break;
    case VOLUME_TYPE_TESTING:
      volume_metadata->volume_type = fmp::VolumeType::kTesting;
      break;
    case VOLUME_TYPE_SMB:
      volume_metadata->volume_type = fmp::VolumeType::kSmb;
      break;
    case VOLUME_TYPE_SYSTEM_INTERNAL:
      volume_metadata->volume_type = fmp::VolumeType::kSystemInternal;
      break;
    case VOLUME_TYPE_GUEST_OS:
      volume_metadata->volume_type = fmp::VolumeType::kGuestOs;
      break;
    case NUM_VOLUME_TYPE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Fill device_type iff the volume is removable partition.
  if (volume.type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    switch (volume.device_type()) {
      case ash::DeviceType::kUnknown:
        volume_metadata->device_type = fmp::DeviceType::kUnknown;
        break;
      case ash::DeviceType::kUSB:
        volume_metadata->device_type = fmp::DeviceType::kUsb;
        break;
      case ash::DeviceType::kSD:
        volume_metadata->device_type = fmp::DeviceType::kSd;
        break;
      case ash::DeviceType::kOpticalDisc:
      case ash::DeviceType::kDVD:
        volume_metadata->device_type = fmp::DeviceType::kOptical;
        break;
      case ash::DeviceType::kMobile:
        volume_metadata->device_type = fmp::DeviceType::kMobile;
        break;
    }
    volume_metadata->device_path = volume.storage_device_path().AsUTF8Unsafe();
    volume_metadata->is_parent_device = volume.is_parent();
  } else {
    volume_metadata->device_type = fmp::DeviceType::kNone;
  }

  volume_metadata->is_read_only = volume.is_read_only();
  volume_metadata->has_media = volume.has_media();
  volume_metadata->hidden = volume.hidden();

  switch (volume.mount_condition()) {
    default:
      LOG(ERROR) << "Unexpected mount condition: " << volume.mount_condition();
      [[fallthrough]];
    case ash::MountError::kSuccess:
      volume_metadata->mount_condition = fmp::MountError::kNone;
      break;
    case ash::MountError::kUnknownFilesystem:
      volume_metadata->mount_condition = fmp::MountError::kUnknownFilesystem;
      break;
    case ash::MountError::kUnsupportedFilesystem:
      volume_metadata->mount_condition =
          fmp::MountError::kUnsupportedFilesystem;
      break;
  }

  // If the context is known, then pass it.
  switch (volume.mount_context()) {
    case MOUNT_CONTEXT_USER:
      volume_metadata->mount_context = fmp::MountContext::kUser;
      break;
    case MOUNT_CONTEXT_AUTO:
      volume_metadata->mount_context = fmp::MountContext::kAuto;
      break;
    case MOUNT_CONTEXT_UNKNOWN:
      break;
  }

  if (volume.vm_type()) {
    volume_metadata->vm_type = VmTypeToJs(*volume.vm_type());
  }
}

base::FilePath GetLocalPathFromURL(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url) {
  const storage::FileSystemURL filesystem_url(
      file_system_context->CrackURLInFirstPartyContext(url));
  base::FilePath path;
  if (!ash::FileSystemBackend::CanHandleURL(filesystem_url)) {
    return base::FilePath();
  }
  return filesystem_url.path();
}

void GetSelectedFileInfo(Profile* profile,
                         std::vector<base::FilePath> local_paths,
                         GetSelectedFileInfoLocalPathOption local_path_option,
                         GetSelectedFileInfoCallback callback) {
  std::unique_ptr<GetSelectedFileInfoParams> params =
      std::make_unique<GetSelectedFileInfoParams>();
  params->local_path_option = local_path_option;
  params->callback = std::move(callback);
  params->file_paths = std::move(local_paths);

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
  return service ? service->GetLogger() : nullptr;
}

std::vector<fmp::MountableGuest> CreateMountableGuestList(Profile* profile) {
  auto* service = guest_os::GuestOsServiceFactory::GetForProfile(profile);
  if (!service) {
    return {};
  }

  bool local_user_files_allowed =
      policy::local_user_files::LocalUserFilesAllowed();

  auto* registry = service->MountProviderRegistry();
  std::vector<fmp::MountableGuest> guests;
  for (const auto id : registry->List()) {
    fmp::MountableGuest guest;
    auto* provider = registry->Get(id);
    if (!local_user_files_allowed &&
        provider->vm_type() == guest_os::VmType::ARCVM) {
      continue;
    }
    guest.id = id;
    guest.display_name = provider->DisplayName();
    guest.vm_type = VmTypeToJs(provider->vm_type());
    guests.push_back(std::move(guest));
  }
  return guests;
}

bool ToRecentSourceFileType(fmp::FileCategory input_category,
                            ash::RecentSource::FileType* output_type) {
  switch (input_category) {
    using enum ash::RecentSource::FileType;
    case fmp::FileCategory::kNone:
      // The FileCategory is an optional parameter. Thus we convert NONE to All.
      // If the calling code does not specify the restrictions on the category
      // we do not enforce then.
    case fmp::FileCategory::kAll:
      *output_type = kAll;
      return true;
    case fmp::FileCategory::kAudio:
      *output_type = kAudio;
      return true;
    case fmp::FileCategory::kImage:
      *output_type = kImage;
      return true;
    case fmp::FileCategory::kVideo:
      *output_type = kVideo;
      return true;
    case fmp::FileCategory::kDocument:
      *output_type = kDocument;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

fmp::BulkPinProgress BulkPinProgressToJs(
    const drivefs::pinning::Progress& progress) {
  fmp::BulkPinProgress result;
  result.stage = DrivefsPinStageToJs(progress.stage);
  result.free_space_bytes = progress.free_space;
  result.required_space_bytes = progress.required_space;
  result.bytes_to_pin = progress.bytes_to_pin;
  result.pinned_bytes = progress.pinned_bytes;
  result.files_to_pin = progress.files_to_pin;
  result.listed_files = progress.listed_files;
  result.remaining_seconds = !progress.remaining_time.is_inf()
                                 ? progress.remaining_time.InSecondsF()
                                 : 0;
  result.should_pin = progress.should_pin;
  result.emptied_queue = progress.emptied_queue;
  return result;
}

void GURLToEntryData(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url,
    base::OnceCallback<void(base::FileErrorOr<fmp::EntryData>)> callback) {
  storage::FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(url);
  std::string entry_name = GetDisplayablePath(profile, file_system_url)
                               .value_or(base::FilePath())
                               .BaseName()
                               .value();
  file_system_context->ResolveURL(
      file_system_url,
      base::BindOnce(GURLToEntryDataOnResolve, std::move(entry_name), url,
                     std::move(callback)));
}

}  // namespace file_manager::util
