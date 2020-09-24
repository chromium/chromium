// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/snapshot_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/components/drivefs/drivefs_util.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_security_policy.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager_private = extensions::api::file_manager_private;

namespace file_manager {
namespace util {
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

  for (size_t i = params->selected_files.size();
       i < params->file_paths.size(); ++i) {
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
              chromeos::CreateExternalFileURLFromPath(profile, file_path);
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

std::unique_ptr<std::string> GetShareUrlFromAlternateUrl(
    const GURL& alternate_url) {
  // Set |share_url| to a modified version of |alternate_url| that opens the
  // sharing dialog for files and folders (add ?userstoinvite="" to the URL).
  GURL::Replacements replacements;
  std::string new_query =
      (alternate_url.has_query() ? alternate_url.query() + "&" : "") +
      "userstoinvite=%22%22";
  replacements.SetQueryStr(new_query);

  return std::make_unique<std::string>(
      alternate_url.ReplaceComponents(replacements).spec());
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
  base::FilePath path;
  if (!integration_service->GetRelativeDrivePath(file_system_url_.path(),
                                                 &path)) {
    CompleteGetEntryProperties(drive::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface) {
    CompleteGetEntryProperties(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
    return;
  }

  drivefs_interface->GetMetadata(
      path,
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

  properties_->size = std::make_unique<double>(metadata->size);
  properties_->present = std::make_unique<bool>(metadata->available_offline);
  properties_->dirty = std::make_unique<bool>(metadata->dirty);
  properties_->hosted =
      std::make_unique<bool>(drivefs::IsHosted(metadata->type));
  properties_->available_offline = std::make_unique<bool>(
      metadata->available_offline || *properties_->hosted);
  properties_->available_when_metered = std::make_unique<bool>(
      metadata->available_offline || *properties_->hosted);
  properties_->pinned = std::make_unique<bool>(metadata->pinned);
  properties_->shared = std::make_unique<bool>(metadata->shared);
  properties_->starred = std::make_unique<bool>(metadata->starred);

  if (metadata->modification_time != base::Time()) {
    properties_->modification_time =
        std::make_unique<double>(metadata->modification_time.ToJsTime());
  }
  if (metadata->last_viewed_by_me_time != base::Time()) {
    properties_->modification_by_me_time =
        std::make_unique<double>(metadata->last_viewed_by_me_time.ToJsTime());
  }
  if (!metadata->content_mime_type.empty()) {
    properties_->content_mime_type =
        std::make_unique<std::string>(metadata->content_mime_type);
  }
  if (!metadata->custom_icon_url.empty()) {
    properties_->custom_icon_url =
        std::make_unique<std::string>(std::move(metadata->custom_icon_url));
  }
  if (!metadata->alternate_url.empty()) {
    properties_->alternate_url =
        std::make_unique<std::string>(std::move(metadata->alternate_url));
    properties_->share_url =
        GetShareUrlFromAlternateUrl(GURL(*properties_->alternate_url));
  }
  if (metadata->image_metadata) {
    if (metadata->image_metadata->height) {
      properties_->image_height =
          std::make_unique<int32_t>(metadata->image_metadata->height);
    }
    if (metadata->image_metadata->width) {
      properties_->image_width =
          std::make_unique<int32_t>(metadata->image_metadata->width);
    }
    if (metadata->image_metadata->rotation) {
      properties_->image_rotation =
          std::make_unique<int32_t>(metadata->image_metadata->rotation);
    }
  }

  properties_->can_delete =
      std::make_unique<bool>(metadata->capabilities->can_delete);
  properties_->can_rename =
      std::make_unique<bool>(metadata->capabilities->can_rename);
  properties_->can_add_children =
      std::make_unique<bool>(metadata->capabilities->can_add_children);

  // Only set the |can_copy| capability for hosted documents; for other files,
  // we must have read access, so |can_copy| is implicitly true.
  properties_->can_copy = std::make_unique<bool>(
      !*properties_->hosted || metadata->capabilities->can_copy);
  properties_->can_share =
      std::make_unique<bool>(metadata->capabilities->can_share);

  if (drivefs::IsAFile(metadata->type)) {
    properties_->thumbnail_url = std::make_unique<std::string>(
        base::StrCat({"drivefs:", file_system_url_.ToGURL().spec()}));
    properties_->cropped_thumbnail_url =
        std::make_unique<std::string>(*properties_->thumbnail_url);
  }

  if (metadata->folder_feature) {
    properties_->is_machine_root =
        std::make_unique<bool>(metadata->folder_feature->is_machine_root);
    properties_->is_external_media =
        std::make_unique<bool>(metadata->folder_feature->is_external_media);
    properties_->is_arbitrary_sync_folder = std::make_unique<bool>(
        metadata->folder_feature->is_arbitrary_sync_folder);
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
                 const chromeos::file_system_provider::IconSet& input) {
  DCHECK(output);
  using chromeos::file_system_provider::IconSet;
  if (input.HasIcon(IconSet::IconSize::SIZE_16x16)) {
    output->icon16x16_url = std::make_unique<std::string>(
        input.GetIcon(IconSet::IconSize::SIZE_16x16).spec());
  }
  if (input.HasIcon(IconSet::IconSize::SIZE_32x32)) {
    output->icon32x32_url = std::make_unique<std::string>(
        input.GetIcon(IconSet::IconSize::SIZE_32x32).spec());
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
    volume_metadata->source_path =
        std::make_unique<std::string>(volume.source_path().AsUTF8Unsafe());
  }

  switch (volume.source()) {
    case SOURCE_FILE:
      volume_metadata->source = file_manager_private::SOURCE_FILE;
      break;
    case SOURCE_DEVICE:
      volume_metadata->source = file_manager_private::SOURCE_DEVICE;
      volume_metadata->is_read_only_removable_device = volume
          .is_read_only_removable_device();
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
    volume_metadata->provider_id =
        std::make_unique<std::string>(volume.provider_id().ToString());
    volume_metadata->file_system_id =
        std::make_unique<std::string>(volume.file_system_id());
  }

  FillIconSet(&volume_metadata->icon_set, volume.icon_set());

  volume_metadata->volume_label =
      std::make_unique<std::string>(volume.volume_label());
  volume_metadata->disk_file_system_type =
      std::make_unique<std::string>(volume.file_system_type());
  volume_metadata->drive_label =
      std::make_unique<std::string>(volume.drive_label());

  switch (volume.type()) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_DRIVE;
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
      volume_metadata->volume_type =
          file_manager_private::VOLUME_TYPE_TESTING;
      break;
    case VOLUME_TYPE_SMB:
      volume_metadata->volume_type = file_manager_private::VOLUME_TYPE_SMB;
      break;
    case NUM_VOLUME_TYPE:
      NOTREACHED();
      break;
  }

  // Fill device_type iff the volume is removable partition.
  if (volume.type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    switch (volume.device_type()) {
      case chromeos::DEVICE_TYPE_UNKNOWN:
        volume_metadata->device_type =
            file_manager_private::DEVICE_TYPE_UNKNOWN;
        break;
      case chromeos::DEVICE_TYPE_USB:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_USB;
        break;
      case chromeos::DEVICE_TYPE_SD:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_SD;
        break;
      case chromeos::DEVICE_TYPE_OPTICAL_DISC:
      case chromeos::DEVICE_TYPE_DVD:
        volume_metadata->device_type =
            file_manager_private::DEVICE_TYPE_OPTICAL;
        break;
      case chromeos::DEVICE_TYPE_MOBILE:
        volume_metadata->device_type = file_manager_private::DEVICE_TYPE_MOBILE;
        break;
    }
    volume_metadata->device_path = std::make_unique<std::string>(
        volume.storage_device_path().AsUTF8Unsafe());
    volume_metadata->is_parent_device =
        std::make_unique<bool>(volume.is_parent());
  } else {
    volume_metadata->device_type =
        file_manager_private::DEVICE_TYPE_NONE;
  }

  volume_metadata->is_read_only = volume.is_read_only();
  volume_metadata->has_media = volume.has_media();

  switch (volume.mount_condition()) {
    case chromeos::disks::MOUNT_CONDITION_NONE:
      volume_metadata->mount_condition =
          file_manager_private::MOUNT_CONDITION_NONE;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNKNOWN_FILESYSTEM:
      volume_metadata->mount_condition =
          file_manager_private::MOUNT_CONDITION_UNKNOWN;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM:
      volume_metadata->mount_condition =
          file_manager_private::MOUNT_CONDITION_UNSUPPORTED;
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
}

base::FilePath GetLocalPathFromURL(content::RenderFrameHost* render_frame_host,
                                   Profile* profile,
                                   const GURL& url) {
  DCHECK(render_frame_host);
  DCHECK(profile);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderFrameHost(profile, render_frame_host);

  const storage::FileSystemURL filesystem_url(
      file_system_context->CrackURL(url));
  base::FilePath path;
  if (!chromeos::FileSystemBackend::CanHandleURL(filesystem_url))
    return base::FilePath();
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

  for (size_t i = 0; i < file_urls.size(); ++i) {
    const GURL& file_url = file_urls[i];
    const base::FilePath path = GetLocalPathFromURL(
        render_frame_host, profile, file_url);
    if (!path.empty()) {
      DVLOG(1) << "Selected: file path: " << path.value();
      params->file_paths.push_back(path);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetSelectedFileInfoInternal, profile, std::move(params)));
}

drive::EventLogger* GetLogger(Profile* profile) {
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return service ? service->event_logger() : nullptr;
}

}  // namespace util
}  // namespace file_manager
