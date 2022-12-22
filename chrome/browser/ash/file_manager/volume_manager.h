// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/documents_provider_root_manager.h"
#include "chrome/browser/ash/file_manager/fusebox_mounter.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace chromeos {
class PowerManagerClient;

namespace disks {
class Disk;
}  // namespace disks

}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class SnapshotManager;
class VolumeManagerObserver;

// Identifiers for volume types managed by Chrome OS file manager.
// The enum values must be kept in sync with FileManagerVolumeType defined in
// tools/metrics/histograms/enums.xml.
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
class Volume : public base::SupportsWeakPtr<Volume> {
 public:
  Volume(const Volume&) = delete;
  Volume& operator=(const Volume&) = delete;

  ~Volume();

  // Factory static methods for different volume types.
  static std::unique_ptr<Volume> CreateForDrive(base::FilePath drive_path);

  static std::unique_ptr<Volume> CreateForDownloads(
      base::FilePath downloads_path);

  static std::unique_ptr<Volume> CreateForRemovable(
      const ash::disks::DiskMountManager::MountPoint& mount_point,
      const ash::disks::Disk* disk);

  static std::unique_ptr<Volume> CreateForProvidedFileSystem(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      MountContext mount_context);

  static std::unique_ptr<Volume> CreateForFuseBoxProvidedFileSystem(
      base::FilePath mount_path,
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      MountContext mount_context);

  static std::unique_ptr<Volume> CreateForMTP(base::FilePath mount_path,
                                              std::string label,
                                              bool read_only);

  static std::unique_ptr<Volume> CreateForFuseBoxMTP(base::FilePath mount_path,
                                                     std::string label,
                                                     bool read_only);

  static std::unique_ptr<Volume> CreateForMediaView(
      const std::string& root_document_id);

  static std::unique_ptr<Volume> CreateMediaViewForTesting(
      base::FilePath mount_path,
      const std::string& root_document_id);

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
      const std::string& document_id,
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
      absl::optional<guest_os::VmType> vm_type,
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
  absl::optional<guest_os::VmType> vm_type() const { return vm_type_; }

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
  // - /home/chronos/user/Downloads/zipfile_path.zip
  base::FilePath source_path_;

  // The mount path of the volume.
  // E.g.:
  // - /home/chronos/user/Downloads
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
  absl::optional<guest_os::VmType> vm_type_;
};

// Manages Volumes for file manager. Example of Volumes:
// - Drive File System.
// - Downloads directory.
// - Removable disks (volume will be created for each partition, not only one
//   for a device).
// - Mounted zip archives.
// - Linux/Crostini file system.
// - Android/Arc++ file system.
// - File System Providers.
class VolumeManager : public KeyedService,
                      public arc::ArcSessionManagerObserver,
                      public drive::DriveIntegrationServiceObserver,
                      public ash::disks::DiskMountManager::Observer,
                      public ash::file_system_provider::Observer,
                      public storage_monitor::RemovableStorageObserver,
                      public DocumentsProviderRootManager::Observer {
 public:
  // An alternate to device::mojom::MtpManager::GetStorageInfo.
  // Used for injecting fake MTP manager for testing in VolumeManagerTest.
  using GetMtpStorageInfoCallback = base::RepeatingCallback<void(
      const std::string&,
      device::mojom::MtpManager::GetStorageInfoCallback)>;

  // Callback for `RemoveSshfsCrostiniVolume`.
  using RemoveSshfsCrostiniVolumeCallback = base::OnceCallback<void(bool)>;

  // Callback for `RemoveSftpGuestOsVolume`.
  using RemoveSftpGuestOsVolumeCallback = base::OnceCallback<void(bool)>;

  VolumeManager(
      Profile* profile,
      drive::DriveIntegrationService* drive_integration_service,
      chromeos::PowerManagerClient* power_manager_client,
      ash::disks::DiskMountManager* disk_mount_manager,
      ash::file_system_provider::Service* file_system_provider_service,
      GetMtpStorageInfoCallback get_mtp_storage_info_callback);

  VolumeManager(const VolumeManager&) = delete;
  VolumeManager& operator=(const VolumeManager&) = delete;

  ~VolumeManager() override;

  // Returns the instance corresponding to the |context|.
  static VolumeManager* Get(content::BrowserContext* context);

  // Initializes this instance.
  void Initialize();

  // Disposes this instance.
  void Shutdown() override;

  // Adds an observer.
  void AddObserver(VolumeManagerObserver* observer);

  // Removes the observer.
  void RemoveObserver(VolumeManagerObserver* observer);

  // Returns the information about all volumes currently mounted. The returned
  // weak pointers are valid as long as the volumes are mounted.
  std::vector<base::WeakPtr<Volume>> GetVolumeList();

  // Finds Volume for the given volume ID. If found, then the returned weak
  // pointer is valid. It is invalidated as soon as the volume is removed from
  // the volume manager.
  base::WeakPtr<Volume> FindVolumeById(const std::string& volume_id);

  // Returns the volume on which an entry, identified by its local (cracked)
  // path, is located. Returns nullptr if no volume is found.
  base::WeakPtr<Volume> FindVolumeFromPath(const base::FilePath& path);

  // Add sshfs crostini volume mounted at `sshfs_mount_path` path. Will
  // automatically remove the volume on container shutdown.
  void AddSshfsCrostiniVolume(const base::FilePath& sshfs_mount_path,
                              const base::FilePath& remote_mount_path);

  // Add sftp Guest OS volume mounted at `sftp_mount_path`. Note: volume must be
  // removed on unmount (including Guest OS shutdown).
  void AddSftpGuestOsVolume(const std::string display_name,
                            const base::FilePath& sftp_mount_path,
                            const base::FilePath& remote_mount_path,
                            const guest_os::VmType vm_type);

  // Removes specified sshfs crostini mount. Runs `callback` with true if the
  // mount was removed successfully or wasn't mounted to begin with. Runs
  // `callback` with false in all other cases.
  void RemoveSshfsCrostiniVolume(const base::FilePath& sshfs_mount_path,
                                 RemoveSshfsCrostiniVolumeCallback callback);

  // Removes specified sftp Guest OS mount. Runs `callback` with true if the
  // mount was removed successfully or wasn't mounted to begin with. Runs
  // `callback` with false in all other cases.
  void RemoveSftpGuestOsVolume(const base::FilePath& sftp_mount_path,
                               const guest_os::VmType vm_type,
                               RemoveSftpGuestOsVolumeCallback callback);

  // Removes Downloads volume used for testing.
  void RemoveDownloadsDirectoryForTesting();

  // For testing purposes, registers a native local file system pointing to
  // |path| with DOWNLOADS type, and adds its volume info.
  bool RegisterDownloadsDirectoryForTesting(const base::FilePath& path);

  // For testing purposes, registers a native local file system pointing to
  // |path| with CROSTINI type, and adds its volume info.
  bool RegisterCrostiniDirectoryForTesting(const base::FilePath& path);

  // For testing purposes, registers a native local file system pointing to
  // |path| with ANDROID_FILES type, and adds its volume info.
  bool RegisterAndroidFilesDirectoryForTesting(const base::FilePath& path);

  // For testing purposes, register a DocumentsProvider root with
  // VOLUME_TYPE_MEDIA_VIEW, and adds its volume info
  bool RegisterMediaViewForTesting(const std::string& root_document_id);

  // For testing purposes, removes a registered native local file system
  // pointing to |path| with ANDROID_FILES type, and removes its volume info.
  bool RemoveAndroidFilesDirectoryForTesting(const base::FilePath& path);

  // For testing purposes, adds a volume info pointing to |path|, with TESTING
  // type. Assumes that the mount point is already registered.
  bool AddVolumeForTesting(base::FilePath path,
                           VolumeType volume_type,
                           ash::DeviceType device_type,
                           bool read_only,
                           base::FilePath device_path = {},
                           std::string drive_label = {},
                           std::string file_system_type = {},
                           bool hidden = false,
                           bool watchable = false);

  // For testing purposes, adds the volume info to the volume manager.
  bool AddVolumeForTesting(std::unique_ptr<Volume> volume);

  void RemoveVolumeForTesting(
      const base::FilePath& path,
      VolumeType volume_type,
      ash::DeviceType device_type,
      bool read_only,
      const base::FilePath& device_path = base::FilePath(),
      const std::string& drive_label = "",
      const std::string& file_system_type = "");

  // drive::DriveIntegrationServiceObserver overrides.
  void OnFileSystemMounted() override;
  void OnFileSystemBeingUnmounted() override;

  // ash::disks::DiskMountManager::Observer overrides.
  void OnAutoMountableDiskEvent(ash::disks::DiskMountManager::DiskEvent event,
                                const ash::disks::Disk& disk) override;
  void OnDeviceEvent(ash::disks::DiskMountManager::DeviceEvent event,
                     const std::string& device_path) override;
  void OnMountEvent(
      ash::disks::DiskMountManager::MountEvent event,
      ash::MountError error,
      const ash::disks::DiskMountManager::MountPoint& mount_info) override;
  void OnFormatEvent(ash::disks::DiskMountManager::FormatEvent event,
                     ash::FormatError error,
                     const std::string& device_path,
                     const std::string& device_label) override;
  void OnPartitionEvent(ash::disks::DiskMountManager::PartitionEvent event,
                        ash::PartitionError error,
                        const std::string& device_path,
                        const std::string& device_label) override;
  void OnRenameEvent(ash::disks::DiskMountManager::RenameEvent event,
                     ash::RenameError error,
                     const std::string& device_path,
                     const std::string& device_label) override;

  // ash::file_system_provider::Observer overrides.
  void OnProvidedFileSystemMount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      ash::file_system_provider::MountContext context,
      base::File::Error error) override;
  void OnProvidedFileSystemUnmount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override;

  // arc::ArcSessionManagerObserver overrides.
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // Called on change to kExternalStorageDisabled pref.
  void OnExternalStorageDisabledChanged();

  // Called on change to kExternalStorageReadOnly pref.
  void OnExternalStorageReadOnlyChanged();

  // RemovableStorageObserver overrides.
  void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) override;
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  // file_manager::DocumentsProviderRootManager::Observer overrides.
  void OnDocumentsProviderRootAdded(
      const std::string& authority,
      const std::string& root_id,
      const std::string& document_id,
      const std::string& title,
      const std::string& summary,
      const GURL& icon_url,
      bool read_only,
      const std::vector<std::string>& mime_types) override;
  void OnDocumentsProviderRootRemoved(const std::string& authority,
                                      const std::string& root_id,
                                      const std::string& document_id) override;

  // For SmbFs.
  void AddSmbFsVolume(const base::FilePath& mount_point,
                      const std::string& display_name);
  void RemoveSmbFsVolume(const base::FilePath& mount_point);

  void ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(std::string* volume_id) const;

  SnapshotManager* snapshot_manager() { return snapshot_manager_.get(); }

  io_task::IOTaskController* io_task_controller() {
    return &io_task_controller_;
  }

  friend std::ostream& operator<<(std::ostream& out, const VolumeManager& vm) {
    return out << "VolumeManager[" << vm.id_ << "]";
  }

 private:
  // Comparator sorting Volume objects by volume ID .
  struct SortByVolumeId {
    using is_transparent = void;

    template <typename A, typename B>
    bool operator()(const A& a, const B& b) const {
      return GetKey(a) < GetKey(b);
    }

    static base::StringPiece GetKey(const base::StringPiece a) { return a; }

    static base::StringPiece GetKey(const std::unique_ptr<Volume>& volume) {
      DCHECK(volume);
      return volume->volume_id();
    }
  };

  // Set of Volume objects indexed by volume ID.
  using Volumes = std::set<std::unique_ptr<Volume>, SortByVolumeId>;

  void OnDiskMountManagerRefreshed(bool success);
  void OnStorageMonitorInitialized();
  void DoAttachMtpStorage(const storage_monitor::StorageInfo& info,
                          device::mojom::MtpStorageInfoPtr mtp_storage_info);

  // Adds |volume| to the set |mounted_volumes_| if |error| is |kNone|.
  // Returns true if the volume was actually added, ie if |error| is
  // |kNone| and there was no previous volume with the same ID.
  bool DoMountEvent(std::unique_ptr<Volume> volume,
                    ash::MountError error = ash::MountError::kSuccess);

  // Removes the Volume at position |it| if |error| is |kNone|.
  // Precondition: it != mounted_volumes_.end()
  void DoUnmountEvent(Volumes::const_iterator it,
                      ash::MountError error = ash::MountError::kSuccess);

  // Removes the Volume with the given ID if |error| is |kNone|.
  void DoUnmountEvent(base::StringPiece volume_id,
                      ash::MountError error = ash::MountError::kSuccess);

  // Removes the Volume with the same ID as |volume| if |error| is |kNone|.
  void DoUnmountEvent(const Volume& volume,
                      ash::MountError error = ash::MountError::kSuccess) {
    DoUnmountEvent(volume.volume_id(), error);
  }

  void OnExternalStorageDisabledChangedUnmountCallback(
      std::vector<std::string> remaining_mount_paths,
      ash::MountError error);

  // Returns the path of the mount point for drive.
  base::FilePath GetDriveMountPointPath() const;

  void OnSshfsCrostiniUnmountCallback(
      const base::FilePath& sshfs_mount_path,
      RemoveSshfsCrostiniVolumeCallback callback,
      ash::MountError error);

  void OnSftpGuestOsUnmountCallback(const base::FilePath& sftp_mount_path,
                                    const guest_os::VmType vm_type,
                                    RemoveSftpGuestOsVolumeCallback callback,
                                    ash::MountError error);

  static int counter_;
  const int id_ = ++counter_;  // Only used in log traces

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_integration_service_;
  ash::disks::DiskMountManager* const disk_mount_manager_;
  ash::file_system_provider::Service* const file_system_provider_service_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<VolumeManagerObserver>::Unchecked observers_;
  GetMtpStorageInfoCallback get_mtp_storage_info_callback_;
  Volumes mounted_volumes_;
  FuseBoxMounter fusebox_mounter_;
  std::unique_ptr<SnapshotManager> snapshot_manager_;
  std::unique_ptr<DocumentsProviderRootManager>
      documents_provider_root_manager_;
  io_task::IOTaskController io_task_controller_;
  bool arc_volumes_mounted_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VolumeManager> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_
