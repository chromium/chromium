// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace file_manager {
namespace {

using l10n_util::GetStringUTF8;
const char kMtpVolumeIdPrefix[] = "mtp:";

VolumeType MountTypeToVolumeType(ash::MountType type) {
  switch (type) {
    case ash::MountType::kInvalid:
      // A zip mount with an invalid path will return type kInvalid. We can use
      // a default VolumeType in this case.
      return VOLUME_TYPE_DOWNLOADS_DIRECTORY;
    case ash::MountType::kDevice:
      return VOLUME_TYPE_REMOVABLE_DISK_PARTITION;
    case ash::MountType::kArchive:
      return VOLUME_TYPE_MOUNTED_ARCHIVE_FILE;
    case ash::MountType::kNetworkStorage:
      // Network storage mounts are handled by their mounters so
      // MountType::kNetworkStorage should never need to be handled
      // here.
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return VOLUME_TYPE_DOWNLOADS_DIRECTORY;
}

// Returns a string representation of the given volume type.
std::string_view VolumeTypeToString(const VolumeType type) {
  switch (type) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      return "drive";
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      return "downloads";
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return "removable";
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      return "archive";
    case VOLUME_TYPE_PROVIDED:
      return "provided";
    case VOLUME_TYPE_MTP:
      return "mtp";
    case VOLUME_TYPE_MEDIA_VIEW:
      return "media_view";
    case VOLUME_TYPE_ANDROID_FILES:
      return "android_files";
    case VOLUME_TYPE_DOCUMENTS_PROVIDER:
      return "documents_provider";
    case VOLUME_TYPE_TESTING:
      return "testing";
    case VOLUME_TYPE_CROSTINI:
      return "crostini";
    case VOLUME_TYPE_SMB:
      return "smb";
    case VOLUME_TYPE_SYSTEM_INTERNAL:
      return "system_internal";
    case VOLUME_TYPE_GUEST_OS:
      return "guest_os";
    case NUM_VOLUME_TYPE:
      break;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected VolumeType value "
      << static_cast<std::underlying_type_t<VolumeType>>(type);
  return "";
}

// Generates a unique volume ID for the given volume info.
std::string GenerateVolumeId(const Volume& volume) {
  // For the same volume type, base names are unique, as mount points are
  // flat for the same volume type.
  return base::StrCat({VolumeTypeToString(volume.type()), ":",
                       volume.mount_path().BaseName().AsUTF8Unsafe()});
}

// Returns the localized label for a given media view.
std::string MediaViewRootIdToLabel(std::string_view root_id) {
  if (root_id == arc::kAudioRootId) {
    return GetStringUTF8(IDS_FILE_BROWSER_MEDIA_VIEW_AUDIO_ROOT_LABEL);
  }

  if (root_id == arc::kImagesRootId) {
    return GetStringUTF8(IDS_FILE_BROWSER_MEDIA_VIEW_IMAGES_ROOT_LABEL);
  }

  if (root_id == arc::kVideosRootId) {
    return GetStringUTF8(IDS_FILE_BROWSER_MEDIA_VIEW_VIDEOS_ROOT_LABEL);
  }

  if (root_id == arc::kDocumentsRootId) {
    return GetStringUTF8(IDS_FILE_BROWSER_MEDIA_VIEW_DOCUMENTS_ROOT_LABEL);
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected root ID: " << root_id;
  return "";
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const VolumeType type) {
  return out << VolumeTypeToString(type);
}

Volume::Volume() = default;
Volume::~Volume() = default;

// static
std::unique_ptr<Volume> Volume::CreateForDrive(base::FilePath drive_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_GOOGLE_DRIVE;
  volume->source_path_ = drive_path;
  volume->source_ = SOURCE_NETWORK;
  volume->mount_path_ = std::move(drive_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ = GetStringUTF8(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForDownloads(
    base::FilePath downloads_path,
    base::FilePath optional_fusebox_path,
    const char* optional_fusebox_volume_label,
    bool read_only) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = std::move(downloads_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ = GetStringUTF8(IDS_FILE_BROWSER_MY_FILES_ROOT_LABEL);
  volume->watchable_ = true;
  volume->is_read_only_ = read_only;

  if (!optional_fusebox_path.empty()) {
    // Leaving the type_ as VOLUME_TYPE_DOWNLOADS_DIRECTORY means that, for
    // some unknown reason, it doesn't show up in the CrOS Files app. Use a
    // different but arbitrary type instead.
    //
    // It doesn't need to be well polished. It's just for debugging FuseBox. We
    // wouldn't normally need a FuseBox wrapper (exposing to the kernel-level
    // file system) for something like Downloads that's typically on local disk
    // (and hence already on the kernel-level file system).
    volume->type_ = VOLUME_TYPE_MTP;

    volume->file_system_type_ = util::kFuseBox;
    volume->mount_path_ = std::move(optional_fusebox_path);
    volume->volume_id_ =
        base::StrCat({util::kFuseBox, std::move(volume->volume_id_)});
    if (optional_fusebox_volume_label) {
      volume->volume_label_ = optional_fusebox_volume_label;
    }
  }

  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForRemovable(
    const ash::disks::DiskMountManager::MountPoint& mount_point,
    const ash::disks::Disk* disk) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = MountTypeToVolumeType(mount_point.mount_type);
  volume->source_path_ = base::FilePath(mount_point.source_path);
  volume->source_ = mount_point.mount_type == ash::MountType::kArchive
                        ? SOURCE_FILE
                        : SOURCE_DEVICE;
  volume->mount_path_ = base::FilePath(mount_point.mount_path);
  volume->mount_condition_ = mount_point.mount_error;

  if (disk) {
    volume->file_system_type_ = disk->file_system_type();
    volume->volume_label_ = disk->device_label();
    volume->device_type_ = disk->device_type();
    volume->storage_device_path_ = base::FilePath(disk->storage_device_path());
    volume->is_parent_ = disk->is_parent();
    volume->is_read_only_ = disk->is_read_only();
    volume->is_read_only_removable_device_ = disk->is_read_only_hardware();
    volume->has_media_ = disk->has_media();
    volume->drive_label_ = disk->drive_label();
  } else {
    volume->volume_label_ = volume->mount_path().BaseName().AsUTF8Unsafe();
    volume->is_read_only_ =
        (mount_point.mount_type == ash::MountType::kArchive);
  }
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForProvidedFileSystem(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    MountContext mount_context,
    base::FilePath optional_fusebox_path) {
  std::unique_ptr<Volume> volume(new Volume());

  switch (file_system_info.source()) {
    case extensions::SOURCE_FILE:
      volume->source_ = SOURCE_FILE;
      break;
    case extensions::SOURCE_DEVICE:
      volume->source_ = SOURCE_DEVICE;
      break;
    case extensions::SOURCE_NETWORK:
      volume->source_ = SOURCE_NETWORK;
      break;
  }

  volume->volume_label_ = file_system_info.display_name();
  volume->type_ = VOLUME_TYPE_PROVIDED;
  volume->mount_path_ = file_system_info.mount_path();
  volume->mount_context_ = mount_context;

  volume->is_parent_ = true;
  volume->is_read_only_ = !file_system_info.writable();
  volume->configurable_ = file_system_info.configurable();
  volume->watchable_ = file_system_info.watchable();
  volume->icon_set_ = file_system_info.icon_set();

  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->file_system_id_ = file_system_info.file_system_id();
  volume->provider_id_ = file_system_info.provider_id();

  if (!optional_fusebox_path.empty()) {
    volume->file_system_type_ = util::kFuseBox;
    if (ash::features::IsFileManagerFuseBoxDebugEnabled()) {
      volume->volume_label_.insert(0, "fusebox ");
    }
    volume->mount_path_ = std::move(optional_fusebox_path);
    // Even though the underlying FSP may support watchers, fusebox needs
    // to implement watchers in order to match the capability of the FSP.
    // TODO(crbug.com/1353673): Add watcher support to fusebox.
    volume->watchable_ = false;
    // "fusebox" prefix the original FSP volume id.
    volume->volume_id_ =
        base::StrCat({util::kFuseBox, GenerateVolumeId(*volume)});
  }

  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForMTP(base::FilePath mount_path,
                                             std::string label,
                                             bool read_only,
                                             bool use_fusebox) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_MTP;
  volume->mount_path_ = mount_path;
  volume->is_parent_ = true;
  volume->is_read_only_ = read_only;
  volume->volume_id_ = base::StrCat({kMtpVolumeIdPrefix, label});
  volume->volume_label_ = std::move(label);
  volume->source_path_ = std::move(mount_path);
  volume->source_ = SOURCE_DEVICE;
  volume->device_type_ = ash::DeviceType::kMobile;

  // MTP does have watcher support via WatcherManager but it doesn't
  // seem to work (perhaps something missing in mtpd).
  volume->watchable_ = false;

  if (use_fusebox) {
    volume->file_system_type_ = util::kFuseBox;
    volume->volume_id_ =
        base::StrCat({util::kFuseBox, std::move(volume->volume_id_)});
    if (ash::features::IsFileManagerFuseBoxDebugEnabled()) {
      volume->volume_label_.insert(0, "fusebox ");
    }
  }

  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForMediaView(const std::string& root_id) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_MEDIA_VIEW;
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = arc::GetDocumentsProviderMountPath(
      arc::kMediaDocumentsProviderAuthority, root_id);
  volume->volume_label_ = MediaViewRootIdToLabel(root_id);
  volume->is_read_only_ = false;
  volume->watchable_ = false;
  volume->volume_id_ = arc::GetMediaViewVolumeId(root_id);
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForSshfsCrostini(
    base::FilePath sshfs_mount_path,
    base::FilePath remote_mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_CROSTINI;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = std::move(sshfs_mount_path);
  volume->remote_mount_path_ = std::move(remote_mount_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ =
      GetStringUTF8(IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForSftpGuestOs(
    std::string display_name,
    base::FilePath sftp_mount_path,
    base::FilePath remote_mount_path,
    const guest_os::VmType vm_type) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = vm_type == guest_os::VmType::ARCVM ? VOLUME_TYPE_ANDROID_FILES
                                                     : VOLUME_TYPE_GUEST_OS;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = std::move(sftp_mount_path);
  volume->remote_mount_path_ = std::move(remote_mount_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ = std::move(display_name);
  volume->watchable_ = true;
  volume->vm_type_ = vm_type;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForAndroidFiles(
    base::FilePath mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_ANDROID_FILES;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = std::move(mount_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ =
      GetStringUTF8(IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL);
  volume->watchable_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForDocumentsProvider(
    const std::string& authority,
    const std::string& root_id,
    const std::string& title,
    const std::string& summary,
    const GURL& icon_url,
    bool read_only,
    const std::string& optional_fusebox_subdir) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_DOCUMENTS_PROVIDER;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = arc::GetDocumentsProviderMountPath(authority, root_id);
  volume->volume_label_ = title;
  volume->is_read_only_ = read_only;
  volume->watchable_ = false;
  volume->volume_id_ = arc::GetDocumentsProviderVolumeId(authority, root_id);
  if (!icon_url.is_empty()) {
    ash::file_system_provider::IconSet icon_set;
    icon_set.SetIcon(ash::file_system_provider::IconSet::IconSize::SIZE_32x32,
                     icon_url);
    volume->icon_set_ = icon_set;
  }

  if (!optional_fusebox_subdir.empty()) {
    volume->file_system_type_ = util::kFuseBox;
    volume->volume_id_.insert(0, util::kFuseBox);
    volume->mount_path_ =
        base::FilePath(util::kFuseBoxMediaPath).Append(optional_fusebox_subdir);
    if (ash::features::IsFileManagerFuseBoxDebugEnabled()) {
      volume->volume_label_.insert(0, "fusebox ");
    }
  }

  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForSmb(base::FilePath mount_point,
                                             std::string display_name) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_SMB;
  // Keep source_path empty.
  volume->source_ = SOURCE_NETWORK;
  volume->mount_path_ = std::move(mount_point);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->volume_label_ = std::move(display_name);
  volume->watchable_ = false;
  volume->is_read_only_ = false;
  return volume;
}

// ShareCache is not visible in the file manager and so this volume does not
// represent a real, user-visible Volume. However, shared files can be read
// through ImageLoader, which needs a Volume present to be able to read from the
// directory.
// static
std::unique_ptr<Volume> Volume::CreateForShareCache(base::FilePath mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = VOLUME_TYPE_SYSTEM_INTERNAL;
  // Keep source_path empty.
  volume->source_ = SOURCE_SYSTEM;
  volume->mount_path_ = std::move(mount_path);
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->watchable_ = false;
  volume->is_read_only_ = true;
  volume->hidden_ = true;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForTesting(base::FilePath path,
                                                 VolumeType volume_type,
                                                 ash::DeviceType device_type,
                                                 bool read_only,
                                                 base::FilePath device_path,
                                                 std::string drive_label,
                                                 std::string file_system_type,
                                                 bool hidden,
                                                 bool watchable) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->type_ = volume_type;
  volume->device_type_ = device_type;
  // Keep source_path empty.
  volume->source_ = SOURCE_DEVICE;
  volume->mount_path_ = std::move(path);
  volume->storage_device_path_ = std::move(device_path);
  volume->is_read_only_ = read_only;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->drive_label_ = std::move(drive_label);
  volume->file_system_type_ = std::move(file_system_type);
  volume->hidden_ = hidden;
  volume->watchable_ = watchable;
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForTesting(base::FilePath device_path,
                                                 base::FilePath mount_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->storage_device_path_ = std::move(device_path);
  volume->mount_path_ = std::move(mount_path);
  return volume;
}

// static
std::unique_ptr<Volume> Volume::CreateForTesting(
    base::FilePath path,
    VolumeType volume_type,
    std::optional<guest_os::VmType> vm_type,
    base::FilePath source_path) {
  std::unique_ptr<Volume> volume(new Volume());
  volume->mount_path_ = std::move(path);
  volume->type_ = volume_type;
  volume->vm_type_ = vm_type;
  volume->volume_id_ = GenerateVolumeId(*volume);
  volume->source_path_ = std::move(source_path);
  return volume;
}

}  // namespace file_manager
