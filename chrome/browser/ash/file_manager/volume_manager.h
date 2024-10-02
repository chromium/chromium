// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/documents_provider_root_manager.h"
#include "chrome/browser/ash/file_manager/fusebox_daemon.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "ui/base/clipboard/clipboard_observer.h"

class Profile;

namespace chromeos {
class PowerManagerClient;

}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class SnapshotManager;
class VolumeManagerObserver;

// Manages Volumes for file manager. Example of Volumes:
// - Drive File System.
// - Downloads directory.
// - Removable disks (volume will be created for each partition, not only one
//   for a device).
// - Mounted zip archives.
// - Linux/Crostini file system.
// - Android/Arc++ file system.
// - File System Providers.
class VolumeManager
    : public KeyedService,
      arc::ArcSessionManagerObserver,
      drive::DriveIntegrationService::Observer,
      ash::disks::DiskMountManager::Observer,
      ash::file_system_provider::Observer,
      storage_monitor::RemovableStorageObserver,
      ui::ClipboardObserver,
      DocumentsProviderRootManager::Observer,
      policy::local_user_files::LocalUserFilesPolicyObserver,
      policy::local_user_files::LocalFilesMigrationManager::Observer {
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
  void RemoveVolumeForTesting(const std::string& volume_id);

  // DriveIntegrationService::Observer implementation.
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
  void OnShutdown() override;

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
                                      const std::string& root_id) override;

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

  // For SmbFs.
  void AddSmbFsVolume(const base::FilePath& mount_point,
                      const std::string& display_name);
  void RemoveSmbFsVolume(const base::FilePath& mount_point);

  void ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(std::string* volume_id) const;

  // policy::local_user_files::Observer:
  void OnLocalUserFilesPolicyChanged() override;

  SnapshotManager* snapshot_manager() { return snapshot_manager_.get(); }

  io_task::IOTaskController* io_task_controller() {
    return &io_task_controller_;
  }

  friend std::ostream& operator<<(std::ostream& out, const VolumeManager& vm) {
    return out << "VolumeManager[" << vm.id_ << "]";
  }

  // Skips the migration and immediately unmounts My Files.
  void OnMigrationSucceededForTesting();

 private:
  // Comparator sorting Volume objects by volume ID .
  struct SortByVolumeId {
    using is_transparent = void;

    template <typename A, typename B>
    bool operator()(const A& a, const B& b) const {
      return GetKey(a) < GetKey(b);
    }

    static std::string_view GetKey(std::string_view a) { return a; }

    static std::string_view GetKey(const std::unique_ptr<Volume>& volume) {
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
  void DoUnmountEvent(std::string_view volume_id,
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

  // Registers and mounts the downloads volume.
  void MountDownloadsVolume(bool read_only = false);

  // Unmounts and revokes the downloads volume.
  void UnmountDownloadsVolume();

  // Mounts all ARC roots declared in arc_media_view_util.cc.
  void MountArcRoots();

  // Unmounts all ARC roots declared in arc_media_view_util.cc.
  void UnmountArcRoots();

  void UnsubscribeFromArcEvents();

  // Subscribes to ARC file system events and if needed, registers and mounts
  // the arc volumes.
  void SubscribeAndMountArc();

  // Unsubscribes from ARC file system events and if needed, unmounts and
  // revokes the arc volumes.
  void UnsubscribeAndUnmountArc();

  // Mounts local folders (MyFiles, Play and Linux files).
  void OnLocalUserFilesEnabled();
  // Unmounts local folders (MyFiles, Play and Linux files).
  void OnLocalUserFilesDisabled();

  // Removes My Files after SkyVault migration completes successufully.
  void OnMigrationSucceeded() override;

  static int counter_;
  const int id_ = ++counter_;  // Only used in log traces

  const raw_ptr<Profile> profile_;
  const raw_ptr<drive::DriveIntegrationService> drive_integration_service_;
  const raw_ptr<ash::disks::DiskMountManager> disk_mount_manager_;
  const raw_ptr<ash::file_system_provider::Service>
      file_system_provider_service_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<VolumeManagerObserver>::Unchecked observers_;
  GetMtpStorageInfoCallback get_mtp_storage_info_callback_;
  Volumes mounted_volumes_;
  scoped_refptr<file_manager::FuseBoxDaemon> fusebox_daemon_;
  std::unique_ptr<SnapshotManager> snapshot_manager_;
  std::unique_ptr<DocumentsProviderRootManager>
      documents_provider_root_manager_;
  io_task::IOTaskController io_task_controller_;
  // TODO(b/328006921): Replace with a check if the volumes are mounted.
  bool arc_volumes_mounted_ = false;
  bool ignore_clipboard_changed_ = false;
  // TODO(b/328006921): Replace with a check if the volumes are mounted.
  bool local_user_files_allowed_ = true;
  // Whether a read only version of local folders (My Files) is needed.
  bool read_only_local_folders_ = true;

  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_session_manager_observation_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VolumeManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(VolumeManagerTest, OnBootDeviceDiskEvent);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_H_
