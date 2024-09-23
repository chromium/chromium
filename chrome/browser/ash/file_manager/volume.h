// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace file_manager {

// TODO(b/304383409): convert to enum class.
// Identifiers for volume types managed by Chrome OS file manager.
// The enum values must be kept in sync with FileManagerVolumeType and
// OfficeFilesSourceVolume defined in tools/metrics/histograms/enums.xml.
enum VolumeType {
  VOLUME_TYPE_TESTING = -1,  // Used only in tests.
  VOLUME_TYPE_GOOGLE_DRIVE = 0,
  VOLUME_TYPE_DOWNLOADS_DIRECTORY = 1,
  VOLUME_TYPE_REMOVABLE_DISK_PARTITION = 2,
  VOLUME_TYPE_MOUNTED_ARCHIVE_FILE = 3,
  VOLUME_TYPE_PROVIDED = 4,  // File system provided by FileSystemProvider API.
  VOLUME_TYPE_MTP = 5,
  VOLUME_TYPE_MEDIA_VIEW = 6,
  VOLUME_TYPE_CROSTINI = 7,
  VOLUME_TYPE_ANDROID_FILES = 8,
  VOLUME_TYPE_DOCUMENTS_PROVIDER = 9,
  VOLUME_TYPE_SMB = 10,
  VOLUME_TYPE_SYSTEM_INTERNAL = 11,  // Internal volume never exposed to users.
  VOLUME_TYPE_GUEST_OS = 12,  // Guest OS volumes (Crostini, Bruschetta, etc)
  // Append new values here.
  NUM_VOLUME_TYPE,
};

// Output operator for logging.
std::ostream& operator<<(std::ostream& out, VolumeType type);

// Says how was the mount performed, whether due to user interaction, or
// automatic. User interaction includes both hardware (pluggins a USB stick)
// or software (mounting a ZIP archive) interaction.
enum MountContext {
  MOUNT_CONTEXT_USER,
  MOUNT_CONTEXT_AUTO,
  MOUNT_CONTEXT_UNKNOWN
};

// Source of a volume's data.
enum Source { SOURCE_FILE, SOURCE_DEVICE, SOURCE_NETWORK, SOURCE_SYSTEM };

// Represents a volume (mount point) in the volume manager. Validity of the data
// is guaranteed by the weak pointer. Simply saying, the weak pointer should be
// valid as long as the volume is mounted.
class Volume {
 public:
  Volume(const Volume&) = delete;
  Volume& operator=(const Volume&) = delete;

  ~Volume();

  // Factory static methods for different volume types.
  static std::unique_ptr<Volume> CreateForDrive(base::FilePath drive_path);

  static std::unique_ptr<Volume> CreateForDownloads(
      base::FilePath downloads_path,
      base::FilePath optional_fusebox_path = {},
      const char* optional_fusebox_volume_label = nullptr,
      bool read_only = false);

  static std::unique_ptr<Volume> CreateForRemovable(
      const ash::disks::DiskMountManager::MountPoint& mount_point,
      const ash::disks::Disk* disk);

  static std::unique_ptr<Volume> CreateForProvidedFileSystem(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      MountContext mount_context,
      base::FilePath optional_fusebox_path = {});

  static std::unique_ptr<Volume> CreateForMTP(base::FilePath mount_path,
                                              std::string label,
                                              bool read_only,
                                              bool use_fusebox = false);

  static std::unique_ptr<Volume> CreateForMediaView(const std::string& root_id);

  static std::unique_ptr<Volume> CreateMediaViewForTesting(
      base::FilePath mount_path,
      const std::string& root_id);

  static std::unique_ptr<Volume> CreateForSshfsCrostini(
      base::FilePath crostini_path,
      base::FilePath remote_mount_path);

  static std::unique_ptr<Volume> CreateForSftpGuestOs(
      std::string display_name,
      base::FilePath sftp_mount_path,
      base::FilePath remote_mount_path,
      const guest_os::VmType vm_type);

  static std::unique_ptr<Volume> CreateForAndroidFiles(
      base::FilePath mount_path);

  static std::unique_ptr<Volume> CreateForDocumentsProvider(
      const std::string& authority,
      const std::string& root_id,
      const std::string& title,
      const std::string& summary,
      const GURL& icon_url,
      bool read_only,
      const std::string& optional_fusebox_subdir);

  static std::unique_ptr<Volume> CreateForSmb(base::FilePath mount_point,
                                              std::string display_name);

  static std::unique_ptr<Volume> CreateForShareCache(base::FilePath mount_path);

  static std::unique_ptr<Volume> CreateForTesting(
      base::FilePath path,
      VolumeType volume_type,
      ash::DeviceType device_type,
      bool read_only,
      base::FilePath device_path,
      std::string drive_label,
      std::string file_system_type = {},
      bool hidden = false,
      bool watchable = false);

  static std::unique_ptr<Volume> CreateForTesting(base::FilePath device_path,
                                                  base::FilePath mount_path);

  // Create a volume at `path` with the specified `volume_type`.
  // For `volume_type`==VOLUME_TYPE_GUEST_OS, `vm_type` should also be
  // specified. For `volume_type`==VOLUME_TYPE_MOUNTED_ARCHIVE_FILE,
  // `source_path` has to be specified and point to the (not necessarily
  // existing) path of the archive file.
  static std::unique_ptr<Volume> CreateForTesting(
      base::FilePath path,
      VolumeType volume_type,
      std::optional<guest_os::VmType> vm_type,
      base::FilePath source_path = {});

  // Getters for all members. See below for details.
  const std::string& volume_id() const { return volume_id_; }
  const std::string& file_system_id() const { return file_system_id_; }
  const ash::file_system_provider::ProviderId& provider_id() const {
    return provider_id_;
  }
  Source source() const { return source_; }
  VolumeType type() const { return type_; }
  ash::DeviceType device_type() const { return device_type_; }
  const base::FilePath& source_path() const { return source_path_; }
  const base::FilePath& mount_path() const { return mount_path_; }
  const base::FilePath& remote_mount_path() const { return remote_mount_path_; }
  ash::MountError mount_condition() const { return mount_condition_; }
  MountContext mount_context() const { return mount_context_; }
  const base::FilePath& storage_device_path() const {
    return storage_device_path_;
  }
  const std::string& volume_label() const { return volume_label_; }
  bool is_parent() const { return is_parent_; }
  // Whether the applications can write to the volume. True if not writable.
  // For example, when write access to external storage is restricted by the
  // policy (ExternalStorageReadOnly), is_read_only() will be true even when
  // is_read_only_removable_device() is false.
  bool is_read_only() const { return is_read_only_; }
  // Whether the device is write-protected by hardware. This field is valid
  // only when device_type is VOLUME_TYPE_REMOVABLE_DISK_PARTITION and
  // source is SOURCE_DEVICE.
  // When this value is true, is_read_only() is also true.
  bool is_read_only_removable_device() const {
    return is_read_only_removable_device_;
  }
  bool has_media() const { return has_media_; }
  bool configurable() const { return configurable_; }
  bool watchable() const { return watchable_; }
  const std::string& file_system_type() const { return file_system_type_; }
  const std::string& drive_label() const { return drive_label_; }
  const ash::file_system_provider::IconSet& icon_set() const {
    return icon_set_;
  }
  bool hidden() const { return hidden_; }
  std::optional<guest_os::VmType> vm_type() const { return vm_type_; }

  base::WeakPtr<Volume> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  Volume();

  // The ID of the volume.
  std::string volume_id_;

  // The ID for provided file systems. If other type, then empty string. Unique
  // per providing extension or native provider.
  std::string file_system_id_;

  // The ID of an extension or native provider providing the file system. If
  // other type, then equal to a ProviderId of the type INVALID.
  ash::file_system_provider::ProviderId provider_id_;

  // The source of the volume's data.
  Source source_ = SOURCE_FILE;

  // The type of mounted volume.
  VolumeType type_ = VOLUME_TYPE_GOOGLE_DRIVE;

  // The type of device. (e.g. USB, SD card, DVD etc.)
  ash::DeviceType device_type_ = ash::DeviceType::kUnknown;

  // The source path of the volume.
  // E.g.:
  // - /home/chronos/user/MyFiles/Downloads/zipfile_path.zip
  base::FilePath source_path_;

  // The mount path of the volume.
  // E.g.:
  // - /media/removable/usb1
  // - /media/archive/zip1
  base::FilePath mount_path_;

  // The path on the remote host where this volume is mounted, for crostini this
  // is the user's homedir (/home/<username>).
  base::FilePath remote_mount_path_;

  // The mounting condition. See the enum for the details.
  ash::MountError mount_condition_ = ash::MountError::kSuccess;

  // The context of the mount. Whether mounting was performed due to a user
  // interaction or not.
  MountContext mount_context_ = MOUNT_CONTEXT_UNKNOWN;

  // Path of the storage device this device's block is a part of.
  // (e.g. /sys/devices/pci0000:00/.../8:0:0:0/)
  base::FilePath storage_device_path_;

  // Label for the volume if the volume is either removable or a provided file
  // system. In case of removables, if disk is a parent, then its label, else
  // parents label (e.g. "TransMemory").
  std::string volume_label_;

  // Identifier for the file system type
  std::string file_system_type_;

  // Volume icon set.
  ash::file_system_provider::IconSet icon_set_;

  // Device label of a physical removable device. Removable partitions belonging
  // to the same device share the same device label.
  std::string drive_label_;

  // Is the device is a parent device (i.e. sdb rather than sdb1).
  bool is_parent_ = false;

  // True if the volume is not writable by applications.
  bool is_read_only_ = false;

  // True if the volume is made read_only due to its hardware.
  // This implies is_read_only_.
  bool is_read_only_removable_device_ = false;

  // True if the volume contains media.
  bool has_media_ = false;

  // True if the volume is configurable.
  bool configurable_ = false;

  // True if the volume notifies about changes via file/directory watchers.
  //
  // Requests to add file watchers for paths on the volume will fail when
  // set to false and Files app will add a "Refresh" icon to the toolbar
  // so that the directory contents can be manually refreshed.
  bool watchable_ = false;

  // True if the volume is hidden and never shown to the user through File
  // Manager.
  bool hidden_ = false;

  // Only set for VOLUME_TYPE_GUEST_OS, identifies the type of Guest OS VM.
  std::optional<guest_os::VmType> vm_type_;

  base::WeakPtrFactory<Volume> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_H_
