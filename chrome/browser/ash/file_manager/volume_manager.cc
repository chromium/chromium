// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/volume_manager.h"

#include <string_view>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "base/auto_reset.h"
#include "base/base64url.h"
#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/snapshot_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"

namespace file_manager {
namespace {

const uint32_t kAccessCapabilityReadWrite = 0;
const uint32_t kFilesystemTypeGenericHierarchical = 2;
const char kFileManagerMTPMountNamePrefix[] = "fileman-mtp-";
const char kMtpVolumeIdPrefix[] = "mtp:";
const char kRootPath[] = "/";

// Registers |path| as the "Downloads" folder to the FileSystem API backend.
// If another folder is already mounted. It revokes and overrides the old one.
bool RegisterDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  // Although we show only profile's own "Downloads" folder in the Files app,
  // in the backend we need to mount all profile's download directory globally.
  // Otherwise, the Files app cannot support cross-profile file copies, etc.
  // For this reason, we need to register to the global GetSystemInstance().
  const std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // In some tests we want to override existing Downloads mount point, so we
  // first revoke the existing mount point (if any).
  mount_points->RevokeFileSystem(mount_point_name);
  return mount_points->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), path);
}

// Revokes |path| as the "Downloads" folder from the FileSystem API backend,
// if mounted.
void RevokeDownloadsMountPoint(Profile* profile, const base::FilePath& path) {
  const std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  mount_points->RevokeFileSystem(mount_point_name);
}

// Registers a mount point for Android files to ExternalMountPoints.
bool RegisterAndroidFilesMountPoint() {
  if (arc::IsArcVmEnabled()) {
    return false;
  }
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      base::FilePath(util::kAndroidFilesPath));
}

// Revokes Android files mount point, if mounted.
void RevokeAndroidFilesMountPoint() {
  if (arc::IsArcVmEnabled()) {
    return;
  }
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RevokeFileSystem(
      file_manager::util::GetAndroidFilesMountPointName());
}

bool RegisterShareCacheMountPoint(Profile* profile) {
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->RegisterFileSystem(
      util::kShareCacheMountPointName, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), util::GetShareCacheFilePath(profile));
}

// Finds the path register as the "Downloads" folder to FileSystem API backend.
// Returns false if it is not registered.
bool FindDownloadsMountPointPath(Profile* profile, base::FilePath* path) {
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->GetRegisteredPath(mount_point_name, path);
}

// Returns true if the mount point is registered with FileSystem API backend.
// Return false if it is not registered.
bool FindExternalMountPoint(const std::string& mount_point_name) {
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  return mount_points->GetRegisteredPath(mount_point_name, &path);
}

std::string FuseBoxSubdirADP(const std::string& authority,
                             const std::string& root_id) {
  // Hash the authority and document ID
  // - because the ID can be quite long (400+ bytes) and
  // - to avoid sharing the ID in the file system.
  std::string hash = crypto::SHA256HashString(
      arc::GetDocumentsProviderMountPathSuffix(authority, root_id)
          .AsUTF8Unsafe());
  std::string b64;
  base::Base64UrlEncode(hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &b64);
  return base::StrCat({util::kFuseBoxSubdirPrefixADP, b64});
}

std::string FuseBoxSubdirFSP(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info) {
  std::string hash =
      crypto::SHA256HashString(file_system_info.mount_path().value());
  std::string b64;
  base::Base64UrlEncode(hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &b64);
  return base::StrCat({util::kFuseBoxSubdirPrefixFSP, b64});
}

std::string FuseBoxSubdirLOC(const base::FilePath& path) {
  std::string hash = crypto::SHA256HashString(path.value());
  std::string b64;
  base::Base64UrlEncode(hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &b64);
  return base::StrCat({util::kFuseBoxSubdirPrefixLOC, b64});
}

std::string FuseBoxSubdirMTP(const std::string& device_id) {
  // Derive the subdir name from the MTP device ID (which is stable even after
  // unplugging and replugging a phone). It's a hash of the ID, not the ID
  // itself, to avoid sharing the device's unique ID in the file system.
  std::string hash = crypto::SHA256HashString(device_id);
  std::string b64;
  base::Base64UrlEncode(hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &b64);
  return base::StrCat({util::kFuseBoxSubdirPrefixMTP, b64});
}

std::string GetMountPointNameForMediaStorage(
    const storage_monitor::StorageInfo& info) {
  std::string name(kFileManagerMTPMountNamePrefix);
  name += info.device_id();
  return name;
}

ash::MountAccessMode GetExternalStorageAccessMode(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(disks::prefs::kExternalStorageReadOnly)
             ? ash::MountAccessMode::kReadOnly
             : ash::MountAccessMode::kReadWrite;
}

void RecordDownloadsDiskUsageStats(base::FilePath downloads_path) {
  constexpr int64_t kOneMiB = 1024 * 1024;
  // For now assume a maximum bucket size of 512GB, which exceeds all current
  // chromeOS hard disk sizes.
  constexpr int64_t k512GiBInMiB = 512 * 1024;

  int64_t download_directory_size_in_bytes =
      base::ComputeDirectorySize(downloads_path);

  base::UmaHistogramCustomCounts(
      "FileBrowser.Downloads.DirectorySizeMiB",
      static_cast<int>(download_directory_size_in_bytes / kOneMiB), 1,
      k512GiBInMiB, 100);

  int64_t total_disk_space_in_bytes =
      base::SysInfo::AmountOfTotalDiskSpace(downloads_path);

  // total_disk_space_in_bytes can be -1 on error.
  if (total_disk_space_in_bytes > 0) {
    int percentage_space_used = std::lround(
        (download_directory_size_in_bytes * 100.0) / total_disk_space_in_bytes);

    base::UmaHistogramPercentageObsoleteDoNotUse(
        "FileBrowser.Downloads.DirectoryPercentageOfDiskUsage",
        percentage_space_used);
  }
}

std::unique_ptr<Volume> CreateForFuseBoxDownloads(
    Profile* profile,
    file_manager::FuseBoxDaemon* fusebox_daemon,
    const char* fusebox_volume_label) {
  if (!profile || !fusebox_daemon) {
    return nullptr;
  }

  // Get the FileSystemURL for the underlying Downloads folder.
  GURL gurl;
  base::FilePath downloads_path = util::GetDownloadsFolderForProfile(profile);
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, downloads_path, util::GetFileManagerURL(), &gurl)) {
    LOG(ERROR) << "could not convert Downloads to FileSystemURL";
    return nullptr;
  }

  // Attach the Downloads directory to the fusebox daemon.
  std::string subdir = FuseBoxSubdirLOC(downloads_path);
  static constexpr bool read_only = false;
  fusebox_daemon->AttachStorage(subdir, gurl.spec(), read_only);

  // Create a Volume for the fusebox edition of Downloads.
  std::unique_ptr<Volume> fusebox_volume = Volume::CreateForDownloads(
      {}, base::FilePath(util::kFuseBoxMediaPath).Append(subdir),
      fusebox_volume_label);

  // Register the fusebox file system with chrome::storage.
  const std::string fusebox_fsid =
      base::StrCat({util::kFuseBoxMountNamePrefix, subdir});
  if (!FindExternalMountPoint(fusebox_fsid)) {
    auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
    bool result = mount_points->RegisterFileSystem(
        fusebox_fsid, storage::kFileSystemTypeFuseBox,
        storage::FileSystemMountOption(), fusebox_volume->mount_path());
    LOG_IF(ERROR, !result) << "invalid FuseBox Downloads mount path";
    DCHECK(result);
  }

  return fusebox_volume;
}

bool IsArcEnabled(Profile* profile) {
  return base::FeatureList::IsEnabled(arc::kMediaViewFeature) &&
         arc::IsArcAllowedForProfile(profile);
}

bool IsSkyVaultV2Enabled() {
  return base::FeatureList::IsEnabled(features::kSkyVaultV2);
}

}  // namespace

int VolumeManager::counter_ = 0;

VolumeManager::VolumeManager(
    Profile* profile,
    drive::DriveIntegrationService* drive_integration_service,
    chromeos::PowerManagerClient* power_manager_client,
    ash::disks::DiskMountManager* disk_mount_manager,
    ash::file_system_provider::Service* file_system_provider_service,
    GetMtpStorageInfoCallback get_mtp_storage_info_callback)
    : profile_(profile),
      drive_integration_service_(drive_integration_service),
      disk_mount_manager_(disk_mount_manager),
      file_system_provider_service_(file_system_provider_service),
      get_mtp_storage_info_callback_(get_mtp_storage_info_callback),
      snapshot_manager_(new SnapshotManager(profile_)),
      documents_provider_root_manager_(
          std::make_unique<DocumentsProviderRootManager>(
              profile_,
              arc::ArcFileSystemOperationRunner::GetForBrowserContext(
                  profile_))) {
  DCHECK(profile_);
  DCHECK(disk_mount_manager_);
  VLOG(1) << *this << "::Constructor with Profile: " << profile->GetDebugName();
}

VolumeManager::~VolumeManager() {
  VLOG(1) << *this << "::Destructor";
}

VolumeManager* VolumeManager::Get(content::BrowserContext* context) {
  return VolumeManagerFactory::Get(context);
}

void VolumeManager::Initialize() {
  VLOG(1) << *this << "::Initialize";

  // If in the Sign in profile or the lock screen app profile or lock screen
  // profile, skip mounting and listening for mount events.
  if (!ash::ProfileHelper::IsUserProfile(profile_)) {
    VLOG(1) << *this << ": Not a user profile: " << profile_->GetDebugName();
    return;
  }

  if (!fusebox_daemon_) {
    fusebox_daemon_ = file_manager::FuseBoxDaemon::GetInstance();
  }

  local_user_files_allowed_ = policy::local_user_files::LocalUserFilesAllowed();
  if (local_user_files_allowed_) {
    // Add local folders - MyFiles and ARC if enabled.
    OnLocalUserFilesEnabled();
  } else {
    OnLocalUserFilesDisabled();
  }
  // For GA, also subscribe to SkyVault LocalFilesMigrationManager.
  if (IsSkyVaultV2Enabled()) {
    if (policy::local_user_files::
            LocalFilesMigrationManager* migration_manager =
                policy::local_user_files::LocalFilesMigrationManagerFactory::
                    GetForBrowserContext(profile_)) {
      migration_manager->AddObserver(this);
    }
  }

  // Subscribe to DriveIntegrationService.
  Observe(drive_integration_service_);
  if (drive_integration_service_->IsMounted()) {
    DoMountEvent(Volume::CreateForDrive(GetDriveMountPointPath()));
  }

  // Subscribe to DiskMountManager.
  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->EnsureMountInfoRefreshed(
      base::BindOnce(&VolumeManager::OnDiskMountManagerRefreshed,
                     weak_ptr_factory_.GetWeakPtr()),
      false /* force */);

  // Subscribe to FileSystemProviderService and register currently mounted
  // volumes for the profile.
  if (file_system_provider_service_) {
    file_system_provider_service_->AddObserver(this);

    std::vector<ash::file_system_provider::ProvidedFileSystemInfo>
        file_system_info_list =
            file_system_provider_service_->GetProvidedFileSystemInfoList();
    for (const auto& file_system_info : file_system_info_list) {
      OnProvidedFileSystemMount(
          file_system_info, ash::file_system_provider::MOUNT_CONTEXT_RESTORE,
          base::File::FILE_OK);
    }
  }

  // Subscribe to Profile Preference change.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      disks::prefs::kExternalStorageDisabled,
      base::BindRepeating(&VolumeManager::OnExternalStorageDisabledChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      disks::prefs::kExternalStorageReadOnly,
      base::BindRepeating(&VolumeManager::OnExternalStorageReadOnlyChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Subscribe to storage monitor for MTP notifications.
  if (storage_monitor::StorageMonitor::GetInstance()) {
    storage_monitor::StorageMonitor::GetInstance()->EnsureInitialized(
        base::BindOnce(&VolumeManager::OnStorageMonitorInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Subscribe to clipboard events.
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);

  RegisterShareCacheMountPoint(profile_);
  DoMountEvent(
      Volume::CreateForShareCache(util::GetShareCacheFilePath(profile_)));
}

void VolumeManager::Shutdown() {
  VLOG(1) << *this << "::Shutdown";

  for (auto& observer : observers_) {
    observer.OnShutdownStart(this);
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  snapshot_manager_.reset();
  pref_change_registrar_.RemoveAll();
  disk_mount_manager_->RemoveObserver(this);
  documents_provider_root_manager_->RemoveObserver(this);
  documents_provider_root_manager_.reset();

  if (storage_monitor::StorageMonitor* const p =
          storage_monitor::StorageMonitor::GetInstance()) {
    p->RemoveObserver(this);
  }

  drive::DriveIntegrationService::Observer::Reset();

  if (file_system_provider_service_) {
    file_system_provider_service_->RemoveObserver(this);
  }

  UnsubscribeFromArcEvents();
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

  // If GA, unsubscribe from SkyVault LocalFilesMigrationManager.
  if (IsSkyVaultV2Enabled()) {
    if (policy::local_user_files::
            LocalFilesMigrationManager* migration_manager =
                policy::local_user_files::LocalFilesMigrationManagerFactory::
                    GetForBrowserContext(profile_, /*create=*/false)) {
      migration_manager->RemoveObserver(this);
    }
  }
}

void VolumeManager::AddObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void VolumeManager::RemoveObserver(VolumeManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::vector<base::WeakPtr<Volume>> VolumeManager::GetVolumeList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<base::WeakPtr<Volume>> result;
  result.reserve(mounted_volumes_.size());
  for (const auto& volume : mounted_volumes_) {
    DCHECK(volume);
    result.push_back(volume->AsWeakPtr());
  }
  return result;
}

base::WeakPtr<Volume> VolumeManager::FindVolumeById(
    const std::string& volume_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (const Volumes::const_iterator it = mounted_volumes_.find(volume_id);
      it != mounted_volumes_.end()) {
    DCHECK(*it);
    return (*it)->AsWeakPtr();
  }

  return nullptr;
}

base::WeakPtr<Volume> VolumeManager::FindVolumeFromPath(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& volume : mounted_volumes_) {
    DCHECK(volume);
    const base::FilePath& volume_mount_path = volume->mount_path();
    if (path == volume_mount_path || volume_mount_path.IsParent(path)) {
      return volume->AsWeakPtr();
    }
  }

  return nullptr;
}

void VolumeManager::AddSshfsCrostiniVolume(
    const base::FilePath& sshfs_mount_path,
    const base::FilePath& remote_mount_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore if volume already exists.
  if (!DoMountEvent(Volume::CreateForSshfsCrostini(sshfs_mount_path,
                                                   remote_mount_path))) {
    return;
  }

  // Listen for crostini container shutdown and remove volume.
  crostini::CrostiniManager::GetForProfile(profile_)
      ->AddShutdownContainerCallback(
          crostini::DefaultContainerId(),
          base::BindOnce(&VolumeManager::RemoveSshfsCrostiniVolume,
                         weak_ptr_factory_.GetWeakPtr(), sshfs_mount_path,
                         base::BindOnce([](bool result) {
                           if (!result) {
                             LOG(ERROR) << "Failed to remove sshfs mount";
                           }
                         })));
}

void VolumeManager::AddSftpGuestOsVolume(
    const std::string display_name,
    const base::FilePath& sftp_mount_path,
    const base::FilePath& remote_mount_path,
    const guest_os::VmType vm_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoMountEvent(Volume::CreateForSftpGuestOs(display_name, sftp_mount_path,
                                            remote_mount_path, vm_type));
}

void VolumeManager::RemoveSshfsCrostiniVolume(
    const base::FilePath& sshfs_mount_path,
    RemoveSshfsCrostiniVolumeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  disk_mount_manager_->UnmountPath(
      sshfs_mount_path.value(),
      base::BindOnce(&VolumeManager::OnSshfsCrostiniUnmountCallback,
                     weak_ptr_factory_.GetWeakPtr(), sshfs_mount_path,
                     std::move(callback)));
}

void VolumeManager::RemoveSftpGuestOsVolume(
    const base::FilePath& sftp_mount_path,
    const guest_os::VmType vm_type,
    RemoveSshfsCrostiniVolumeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  disk_mount_manager_->UnmountPath(
      sftp_mount_path.value(),
      base::BindOnce(&VolumeManager::OnSftpGuestOsUnmountCallback,
                     weak_ptr_factory_.GetWeakPtr(), sftp_mount_path, vm_type,
                     std::move(callback)));
}

bool VolumeManager::RegisterAndroidFilesDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool result =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetAndroidFilesMountPointName(),
          storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
          path);
  DCHECK(result);
  return DoMountEvent(Volume::CreateForAndroidFiles(path));
}

bool VolumeManager::RegisterMediaViewForTesting(
    const std::string& root_document_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return DoMountEvent(Volume::CreateForMediaView(root_document_id));
}

bool VolumeManager::RemoveAndroidFilesDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(*Volume::CreateForAndroidFiles(path));
  return true;
}

void VolumeManager::RemoveDownloadsDirectoryForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::FilePath path;
  if (FindDownloadsMountPointPath(profile_, &path)) {
    DoUnmountEvent(*Volume::CreateForDownloads(path));
  }
}

bool VolumeManager::RegisterDownloadsDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath old_path;
  if (FindDownloadsMountPointPath(profile_, &old_path)) {
    DoUnmountEvent(*Volume::CreateForDownloads(old_path));
  }

  const bool ok = RegisterDownloadsMountPoint(profile_, path);
  // Determine if the local user files directory should be mounted as read-only:
  //  - SkyVault GA is enabled
  //  - Local storage of user files is disallowed by policy
  const bool read_only = IsSkyVaultV2Enabled() && !local_user_files_allowed_;

  // In production, once read_only_local_folders_ is false, the
  // MyFiles/Downloads are unmounted and not mounted again. However, in tests,
  // it's possible to call this function after the SkyVault migration has
  // completed.
  if (read_only && !read_only_local_folders_) {
    LOG(WARNING) << "Adding Downloads volume for testing, even though it "
                    "should've been removed because of SkyVault.";
  }
  return DoMountEvent(
      Volume::CreateForDownloads(path, {}, nullptr, read_only),
      ok ? ash::MountError::kSuccess : ash::MountError::kInvalidPath);
}

bool VolumeManager::RegisterCrostiniDirectoryForTesting(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool ok =
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          file_manager::util::GetCrostiniMountPointName(profile_),
          storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
          path);
  return DoMountEvent(
      Volume::CreateForSshfsCrostini(path, base::FilePath("/home/testuser")),
      ok ? ash::MountError::kSuccess : ash::MountError::kInvalidPath);
}

bool VolumeManager::AddVolumeForTesting(base::FilePath path,
                                        VolumeType volume_type,
                                        ash::DeviceType device_type,
                                        bool read_only,
                                        base::FilePath device_path,
                                        std::string drive_label,
                                        std::string file_system_type,
                                        bool hidden,
                                        bool watchable) {
  return AddVolumeForTesting(Volume::CreateForTesting(
      std::move(path), volume_type, device_type, read_only,
      std::move(device_path), std::move(drive_label),
      std::move(file_system_type), hidden, watchable));
}

bool VolumeManager::AddVolumeForTesting(std::unique_ptr<Volume> volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return DoMountEvent(std::move(volume));
}

void VolumeManager::RemoveVolumeForTesting(
    const base::FilePath& path,
    VolumeType volume_type,
    ash::DeviceType device_type,
    bool read_only,
    const base::FilePath& device_path,
    const std::string& drive_label,
    const std::string& file_system_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(*Volume::CreateForTesting(path, volume_type, device_type,
                                           read_only, device_path, drive_label,
                                           file_system_type));
}

void VolumeManager::RemoveVolumeForTesting(const std::string& volume_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(volume_id);
}

void VolumeManager::OnFileSystemMounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Raise mount event.
  // We can pass ash::MountError::kNone even when authentication is failed
  // or network is unreachable. These two errors will be handled later.
  DoMountEvent(Volume::CreateForDrive(GetDriveMountPointPath()));
}

void VolumeManager::OnFileSystemBeingUnmounted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DoUnmountEvent(*Volume::CreateForDrive(GetDriveMountPointPath()));
}

void VolumeManager::OnAutoMountableDiskEvent(
    ash::disks::DiskMountManager::DiskEvent event,
    const ash::disks::Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Disregard hidden devices.
  if (disk.is_hidden()) {
    return;
  }

  switch (event) {
    case ash::disks::DiskMountManager::DISK_ADDED:
    case ash::disks::DiskMountManager::DISK_CHANGED: {
      if (disk.device_path().empty()) {
        DVLOG(1) << "Empty system path for disk " << disk.device_number();
        return;
      }

      bool mounting = false;
      if (disk.mount_path().empty() && disk.has_media() &&
          !profile_->GetPrefs()->GetBoolean(
              disks::prefs::kExternalStorageDisabled)) {
        // If disk is not mounted yet and it has media and there is no policy
        // forbidding external storage, give it a try.
        // Initiate disk mount operation. MountPath auto-detects the filesystem
        // format if the second argument is empty.
        disk_mount_manager_->MountPath(
            disk.device_path(), {}, disk.device_label(), {},
            ash::MountType::kDevice, GetExternalStorageAccessMode(profile_),
            base::DoNothing());
        mounting = true;
      }

      // Notify to observers.
      for (auto& observer : observers_) {
        observer.OnDiskAdded(disk, mounting);
      }

      return;
    }

    case ash::disks::DiskMountManager::DISK_REMOVED:
      // If the disk is already mounted, unmount it.
      if (!disk.mount_path().empty()) {
        disk_mount_manager_->UnmountPath(disk.mount_path(), base::DoNothing());
      }

      // Notify to observers.
      for (auto& observer : observers_) {
        observer.OnDiskRemoved(disk);
      }

      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void VolumeManager::OnDeviceEvent(
    ash::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "OnDeviceEvent: " << event << ", " << device_path;
  switch (event) {
    case ash::disks::DiskMountManager::DEVICE_ADDED:
      for (auto& observer : observers_) {
        observer.OnDeviceAdded(device_path);
      }
      return;
    case ash::disks::DiskMountManager::DEVICE_REMOVED: {
      for (auto& observer : observers_) {
        observer.OnDeviceRemoved(device_path);
      }
      return;
    }
    case ash::disks::DiskMountManager::DEVICE_SCANNED:
      DVLOG(1) << "Ignore SCANNED event: " << device_path;
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void VolumeManager::OnMountEvent(
    ash::disks::DiskMountManager::MountEvent event,
    ash::MountError error,
    const ash::disks::DiskMountManager::MountPoint& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Network storage is responsible for doing its own mounting.
  if (mount_info.mount_type == ash::MountType::kNetworkStorage) {
    return;
  }

  // Notify a mounting/unmounting event to observers.
  const ash::disks::Disk* const disk =
      disk_mount_manager_->FindDiskBySourcePath(mount_info.source_path);
  std::unique_ptr<Volume> volume = Volume::CreateForRemovable(mount_info, disk);

  switch (event) {
    case ash::disks::DiskMountManager::MOUNTING: {
      DoMountEvent(std::move(volume), error);
      return;
    }

    case ash::disks::DiskMountManager::UNMOUNTING:
      DoUnmountEvent(*volume, error);
      return;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected event type " << event;
}

void VolumeManager::OnFormatEvent(
    ash::disks::DiskMountManager::FormatEvent event,
    ash::FormatError error,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnFormatEvent: " << event << ", error = " << error
           << ", device_path = " << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::FORMAT_STARTED:
      for (auto& observer : observers_) {
        observer.OnFormatStarted(device_path, device_label,
                                 error == ash::FormatError::kSuccess);
      }
      return;

    case ash::disks::DiskMountManager::FORMAT_COMPLETED:
      // Even if format did not complete successfully, try to mount the device
      // so the user can retry.
      // MountPath auto-detects filesystem format if second argument is
      // empty. The third argument (mount label) is not used in a disk mount
      // operation.
      disk_mount_manager_->MountPath(
          device_path, {}, {}, {}, ash::MountType::kDevice,
          GetExternalStorageAccessMode(profile_), base::DoNothing());

      for (auto& observer : observers_) {
        observer.OnFormatCompleted(device_path, device_label,
                                   error == ash::FormatError::kSuccess);
      }

      return;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected FormatEvent " << event;
}

void VolumeManager::OnPartitionEvent(
    ash::disks::DiskMountManager::PartitionEvent event,
    ash::PartitionError error,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnPartitionEvent: " << event << ", error = " << error
           << ", device_path = " << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::PARTITION_STARTED:
      for (auto& observer : observers_) {
        observer.OnPartitionStarted(device_path, device_label,
                                    error == ash::PartitionError::kSuccess);
      }
      return;

    case ash::disks::DiskMountManager::PARTITION_COMPLETED:
      // If partitioning failed, try to mount the device so the user can retry.
      // MountPath auto-detects filesystem format if second argument is
      // empty. The third argument (mount label) is not used in a disk mount
      // operation.
      if (error != ash::PartitionError::kSuccess) {
        disk_mount_manager_->MountPath(
            device_path, {}, {}, {}, ash::MountType::kDevice,
            GetExternalStorageAccessMode(profile_), base::DoNothing());
      }

      for (auto& observer : observers_) {
        observer.OnPartitionCompleted(device_path, device_label,
                                      error == ash::PartitionError::kSuccess);
      }
      return;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected PartitionEvent " << event;
}

void VolumeManager::OnRenameEvent(
    ash::disks::DiskMountManager::RenameEvent event,
    ash::RenameError error,
    const std::string& device_path,
    const std::string& device_label) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnRenameEvent: " << event << ", error = " << error
           << ", device_path = " << device_path;

  switch (event) {
    case ash::disks::DiskMountManager::RENAME_STARTED:
      for (auto& observer : observers_) {
        observer.OnRenameStarted(device_path, device_label,
                                 error == ash::RenameError::kSuccess);
      }
      return;

    case ash::disks::DiskMountManager::RENAME_COMPLETED:
      // Find previous mount point label if it exists
      std::string mount_label;
      auto disk_map_iter = disk_mount_manager_->disks().find(device_path);
      if (disk_map_iter != disk_mount_manager_->disks().end() &&
          !disk_map_iter->get()->base_mount_path().empty()) {
        mount_label = base::FilePath(disk_map_iter->get()->base_mount_path())
                          .BaseName()
                          .AsUTF8Unsafe();
      }

      // Try to mount the device. MountPath auto-detects filesystem format if
      // second argument is empty. Third argument is a mount point name of the
      // disk when it was first time mounted (to preserve mount point regardless
      // of the volume name).
      disk_mount_manager_->MountPath(
          device_path, {}, mount_label, {}, ash::MountType::kDevice,
          GetExternalStorageAccessMode(profile_), base::DoNothing());

      bool successfully_renamed = error == ash::RenameError::kSuccess;
      for (auto& observer : observers_) {
        observer.OnRenameCompleted(device_path, device_label,
                                   successfully_renamed);
      }

      return;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected RenameEvent " << event;
}

void VolumeManager::OnProvidedFileSystemMount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    ash::file_system_provider::MountContext context,
    base::File::Error error) {
  MountContext volume_context = MOUNT_CONTEXT_UNKNOWN;
  switch (context) {
    case ash::file_system_provider::MOUNT_CONTEXT_USER:
      volume_context = MOUNT_CONTEXT_USER;
      break;
    case ash::file_system_provider::MOUNT_CONTEXT_RESTORE:
      volume_context = MOUNT_CONTEXT_AUTO;
      break;
  }

  std::unique_ptr<Volume> volume =
      Volume::CreateForProvidedFileSystem(file_system_info, volume_context);

  ash::MountError mount_error;
  switch (error) {
    case base::File::FILE_OK:
      mount_error = ash::MountError::kSuccess;
      break;
    case base::File::FILE_ERROR_EXISTS:
      mount_error = ash::MountError::kPathAlreadyMounted;
      break;
    default:
      mount_error = ash::MountError::kUnknownError;
      break;
  }

  DoMountEvent(std::move(volume), mount_error);

  // The FSP is not added to chrome::storage if mounting failed.
  if (error != base::File::FILE_OK) {
    return;
  }

  // Get the FuseBoxDaemon instance.
  if (!fusebox_daemon_) {
    fusebox_daemon_ = file_manager::FuseBoxDaemon::GetInstance();
  }

  // Get the FileSystemURL of the FSP storage device.
  const std::string fsid =
      file_system_info.mount_path().BaseName().AsUTF8Unsafe();
  auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  auto fsp_file_system_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(util::GetFilesAppOrigin()), fsid, {});
  const std::string url = fsp_file_system_url.ToGURL().spec();
  DCHECK(fsp_file_system_url.is_valid());

  // Attach the FSP storage device to the fusebox daemon.
  const std::string subdir = FuseBoxSubdirFSP(file_system_info);
  fusebox_daemon_->AttachStorage(subdir, url, !file_system_info.writable());

  // Create a Volume for the fusebox FSP storage device.
  std::unique_ptr<Volume> fusebox_volume = Volume::CreateForProvidedFileSystem(
      file_system_info, volume_context,
      base::FilePath(util::kFuseBoxMediaPath).Append(subdir));

  // Register the fusebox FSP storage device with chrome::storage.
  const std::string fusebox_fsid = base::StrCat(
      {util::kFuseBoxMountNamePrefix, util::kFuseBoxSubdirPrefixFSP, fsid});
  if (!FindExternalMountPoint(fusebox_fsid)) {
    bool result = mount_points->RegisterFileSystem(
        fusebox_fsid, storage::kFileSystemTypeFuseBox,
        storage::FileSystemMountOption(), fusebox_volume->mount_path());
    LOG_IF(ERROR, !result) << "invalid FuseBox FSP mount path";
    DCHECK(result);
  }

  // Mount the fusebox FSP storage device in files app.
  DoMountEvent(std::move(fusebox_volume));
}

void VolumeManager::ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(
    std::string* volume_id) const {
  DCHECK(volume_id);

  static const base::FilePath::CharType kFuseBoxFSPVolumeIdPrefix[] =
      FILE_PATH_LITERAL("fuseboxprovided:fsp:");
  if (!base::StartsWith(*volume_id, kFuseBoxFSPVolumeIdPrefix)) {
    return;
  }

  int prefix = strlen(kFuseBoxFSPVolumeIdPrefix);
  *volume_id = volume_id->substr(prefix).insert(0, "provided:");
}

// TODO(aidazolic): Figure out why it's called twice for every update.
void VolumeManager::OnLocalUserFilesPolicyChanged() {
  if (!base::FeatureList::IsEnabled(features::kSkyVault)) {
    return;
  }

  bool allowed = policy::local_user_files::LocalUserFilesAllowed();
  if (allowed == local_user_files_allowed_) {
    return;
  }
  local_user_files_allowed_ = allowed;

  if (allowed) {
    OnLocalUserFilesEnabled();
  } else {
    OnLocalUserFilesDisabled();
  }
}

void VolumeManager::OnProvidedFileSystemUnmount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // TODO(mtomasz): Introduce own type, and avoid using MountError internally,
  // since it is related to cros disks only.
  const ash::MountError mount_error = error == base::File::FILE_OK
                                          ? ash::MountError::kSuccess
                                          : ash::MountError::kUnknownError;
  std::unique_ptr<Volume> volume = Volume::CreateForProvidedFileSystem(
      file_system_info, MOUNT_CONTEXT_UNKNOWN);
  DoUnmountEvent(*volume, mount_error);

  // Get FSP chrome::storage |fsid| and fusebox daemon |subdir|.
  const std::string fsid =
      file_system_info.mount_path().BaseName().AsUTF8Unsafe();
  const std::string subdir = FuseBoxSubdirFSP(file_system_info);

  // Unmount the fusebox FSP storage device in files app.
  std::unique_ptr<Volume> fusebox_volume = Volume::CreateForProvidedFileSystem(
      file_system_info, MOUNT_CONTEXT_UNKNOWN,
      base::FilePath(util::kFuseBoxMediaPath).Append(subdir));
  DoUnmountEvent(*fusebox_volume, mount_error);

  // Remove the fusebox FSP storage device from chrome::storage.
  auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  const std::string fusebox_fsid = base::StrCat(
      {util::kFuseBoxMountNamePrefix, util::kFuseBoxSubdirPrefixFSP, fsid});
  mount_points->RevokeFileSystem(fusebox_fsid);

  // Detach the fusebox FSP storage device from the fusebox daemon.
  if (fusebox_daemon_) {
    fusebox_daemon_->DetachStorage(subdir);
  }
}

void VolumeManager::OnExternalStorageDisabledChangedUnmountCallback(
    std::vector<std::string> remaining_mount_paths,
    ash::MountError error) {
  LOG_IF(ERROR, error != ash::MountError::kSuccess)
      << "Unmount on ExternalStorageDisabled policy change failed: " << error;

  while (!remaining_mount_paths.empty()) {
    std::string mount_path = remaining_mount_paths.back();
    remaining_mount_paths.pop_back();
    if (!base::Contains(disk_mount_manager_->mount_points(), mount_path)) {
      // The mount point could have already been removed for another reason
      // (i.e. the disk was removed by the user).
      continue;
    }

    disk_mount_manager_->UnmountPath(
        mount_path,
        base::BindOnce(
            &VolumeManager::OnExternalStorageDisabledChangedUnmountCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(remaining_mount_paths)));
    return;
  }
}

void VolumeManager::OnArcPlayStoreEnabledChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsArcEnabled(profile_));
  const bool mounting =
      arc::ShouldAlwaysMountAndroidVolumesInFilesForTesting() || enabled;

  if (mounting == arc_volumes_mounted_) {
    return;
  }

  if (mounting) {
    MountArcRoots();
  } else {
    UnmountArcRoots();
  }

  documents_provider_root_manager_->SetEnabled(mounting);
  arc_volumes_mounted_ = mounting;
}

void VolumeManager::OnShutdown() {
  arc_session_manager_observation_.Reset();
}

void VolumeManager::OnExternalStorageDisabledChanged() {
  // If the policy just got disabled we have to unmount every device currently
  // mounted. The opposite is fine - we can let the user re-plug their device to
  // make it available.
  if (profile_->GetPrefs()->GetBoolean(
          disks::prefs::kExternalStorageDisabled)) {
    // We do not iterate on mount_points directly, because mount_points can
    // be changed by UnmountPath(). Also, a failing unmount shouldn't be retried
    // indefinitely. So make a set of all the mount points that should be
    // unmounted (all external media mounts), and iterate through them.
    std::vector<std::string> remaining_mount_paths;
    for (const auto& mount_point : disk_mount_manager_->mount_points()) {
      if (mount_point.mount_type == ash::MountType::kDevice) {
        remaining_mount_paths.push_back(mount_point.mount_path);
      }
    }
    if (remaining_mount_paths.empty()) {
      return;
    }

    std::string mount_path = remaining_mount_paths.back();
    remaining_mount_paths.pop_back();
    disk_mount_manager_->UnmountPath(
        mount_path,
        base::BindOnce(
            &VolumeManager::OnExternalStorageDisabledChangedUnmountCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(remaining_mount_paths)));
  }
}

void VolumeManager::OnExternalStorageReadOnlyChanged() {
  disk_mount_manager_->RemountAllRemovableDrives(
      GetExternalStorageAccessMode(profile_));
}

void VolumeManager::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id())) {
    return;
  }
  if (profile_->GetPrefs()->GetBoolean(
          disks::prefs::kExternalStorageDisabled)) {
    return;
  }

  // Resolve mtp storage name and get MtpStorageInfo.
  std::string storage_name;
  base::RemoveChars(info.location(), kRootPath, &storage_name);
  DCHECK(!storage_name.empty());
  if (get_mtp_storage_info_callback_.is_null()) {
    storage_monitor::StorageMonitor::GetInstance()
        ->media_transfer_protocol_manager()
        ->GetStorageInfo(storage_name,
                         base::BindOnce(&VolumeManager::DoAttachMtpStorage,
                                        weak_ptr_factory_.GetWeakPtr(), info));
  } else {
    get_mtp_storage_info_callback_.Run(
        storage_name, base::BindOnce(&VolumeManager::DoAttachMtpStorage,
                                     weak_ptr_factory_.GetWeakPtr(), info));
  }
}

void VolumeManager::DoAttachMtpStorage(
    const storage_monitor::StorageInfo& info,
    device::mojom::MtpStorageInfoPtr mtp_storage_info) {
  if (!mtp_storage_info) {
    // |mtp_storage_info| can be null. e.g. As OnRemovableStorageAttached and
    // DoAttachMtpStorage are called asynchronously, there can be a race
    // condition where the storage has been already removed in
    // MediaTransferProtocolManager at the time when this method is called.
    return;
  }

  // Mtp write is enabled only when the device is writable, supports generic
  // hierarchical file system, and writing to external storage devices is not
  // prohibited by the preference.
  const bool read_only =
      mtp_storage_info->access_capability != kAccessCapabilityReadWrite ||
      mtp_storage_info->filesystem_type != kFilesystemTypeGenericHierarchical ||
      GetExternalStorageAccessMode(profile_) == ash::MountAccessMode::kReadOnly;

  const base::FilePath path = base::FilePath::FromUTF8Unsafe(info.location());
  const std::string fsid = GetMountPointNameForMediaStorage(info);
  const std::string base_name = base::UTF16ToUTF8(info.model_name());

  // Assign a fresh volume ID based on the volume name.
  std::string label = base_name;
  for (int i = 2; mounted_volumes_.count(kMtpVolumeIdPrefix + label) != 0;
       ++i) {
    label = base_name + base::StringPrintf(" (%d)", i);
  }

  // Register the MTP storage device with chrome::storage.
  auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  if (!FindExternalMountPoint(fsid)) {
    bool result = mount_points->RegisterFileSystem(
        fsid, storage::kFileSystemTypeDeviceMediaAsFileStorage,
        storage::FileSystemMountOption(), path);
    LOG_IF(ERROR, !result) << "invalid MTP mount path";
    DCHECK(result);
  }

  // Register the MTP storage device with the MTPDeviceMapService.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MTPDeviceMapService::RegisterMTPFileSystem,
                     base::Unretained(MTPDeviceMapService::GetInstance()),
                     info.location(), fsid, read_only));

  // Mount the MTP storage device in files app.
  std::unique_ptr<Volume> volume = Volume::CreateForMTP(path, label, read_only);
  DoMountEvent(std::move(volume));

  // Get the FuseBoxDaemon instance.
  if (!fusebox_daemon_) {
    fusebox_daemon_ = file_manager::FuseBoxDaemon::GetInstance();
  }

  // Get the FileSystemURL of the MTP storage device.
  auto mtp_file_system_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(util::GetFilesAppOrigin()), fsid, {});
  const std::string url = mtp_file_system_url.ToGURL().spec();
  DCHECK(mtp_file_system_url.is_valid());

  // Attach the MTP storage device to the fusebox daemon.
  std::string subdir = FuseBoxSubdirMTP(info.device_id());
  fusebox_daemon_->AttachStorage(subdir, url, read_only);

  // Create a Volume for the fusebox MTP storage device.
  std::unique_ptr<Volume> fusebox_volume = Volume::CreateForMTP(
      base::FilePath(util::kFuseBoxMediaPath).Append(subdir), label, read_only,
      /*use_fusebox=*/true);

  // Register the fusebox MTP storage device with chrome::storage.
  const std::string fusebox_fsid =
      base::StrCat({util::kFuseBoxMountNamePrefix, subdir});
  if (!FindExternalMountPoint(fusebox_fsid)) {
    bool result = mount_points->RegisterFileSystem(
        fusebox_fsid, storage::kFileSystemTypeFuseBox,
        storage::FileSystemMountOption(), fusebox_volume->mount_path());
    LOG_IF(ERROR, !result) << "invalid FuseBox MTP mount path";
    DCHECK(result);
  }

  // Mount the fusebox MTP storage device in files app.
  DoMountEvent(std::move(fusebox_volume));
}

void VolumeManager::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  if (!storage_monitor::StorageInfo::IsMTPDevice(info.device_id())) {
    return;
  }

  Volumes::const_iterator it = mounted_volumes_.begin();
  for (const Volumes::const_iterator end = mounted_volumes_.end();; ++it) {
    if (it == end) {
      return;
    }
    DCHECK(*it);
    if ((*it)->source_path().value() == info.location()) {
      break;
    }
  }

  // Unmount the MTP storage device in files app.
  const std::string volume_id = (*it)->volume_id();
  DoUnmountEvent(std::move(it));

  // Remove the MTP storage device from chrome::storage.
  const std::string fsid = GetMountPointNameForMediaStorage(info);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RevokeFileSystem(fsid);

  // Remove the MTP storage device from the MTPDeviceMapService.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MTPDeviceMapService::RevokeMTPFileSystem,
                     base::Unretained(MTPDeviceMapService::GetInstance()),
                     fsid));

  // Unmount the fusebox MTP storage device in files app.
  base::WeakPtr<Volume> fusebox_volume =
      FindVolumeById(util::kFuseBox + volume_id);
  if (fusebox_volume) {
    DoUnmountEvent(*fusebox_volume);
  }

  // Remove the fusebox MTP storage device from chrome::storage.
  std::string subdir = FuseBoxSubdirMTP(info.device_id());
  const std::string fusebox_fsid =
      base::StrCat({util::kFuseBoxMountNamePrefix, subdir});
  mount_points->RevokeFileSystem(fusebox_fsid);

  // Detach the fusebox MTP storage device from the fusebox daemon.
  if (fusebox_daemon_) {
    fusebox_daemon_->DetachStorage(subdir);
  }
}

void VolumeManager::OnDocumentsProviderRootAdded(
    const std::string& authority,
    const std::string& root_id,
    const std::string& document_id,
    const std::string& title,
    const std::string& summary,
    const GURL& icon_url,
    bool read_only,
    const std::vector<std::string>& mime_types) {
  arc::ArcDocumentsProviderRootMap::GetForArcBrowserContext()->RegisterRoot(
      authority, document_id, root_id, read_only, mime_types);
  DoMountEvent(Volume::CreateForDocumentsProvider(
      authority, root_id, title, summary, icon_url, read_only,
      /*optional_fusebox_subdir=*/std::string()));

  // Get the FuseBoxDaemon instance.
  if (!fusebox_daemon_) {
    fusebox_daemon_ = file_manager::FuseBoxDaemon::GetInstance();
  }

  // Get the FileSystemURL of the ADP storage device.
  auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  auto adp_file_system_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(util::GetFilesAppOrigin()),
      arc::kDocumentsProviderMountPointName,
      arc::GetDocumentsProviderMountPathSuffix(authority, root_id));
  const std::string url = adp_file_system_url.ToGURL().spec();
  DCHECK(adp_file_system_url.is_valid());

  // Attach the ADP storage device to the fusebox daemon.
  std::string subdir = FuseBoxSubdirADP(authority, root_id);
  fusebox_daemon_->AttachStorage(subdir, url, read_only);

  // Create a Volume for the fusebox ADP storage device.
  std::unique_ptr<Volume> fusebox_volume = Volume::CreateForDocumentsProvider(
      authority, root_id, title, summary, icon_url, read_only, subdir);

  // Register the fusebox ADP storage device with chrome::storage.
  const std::string fusebox_fsid =
      base::StrCat({util::kFuseBoxMountNamePrefix, subdir});
  if (!FindExternalMountPoint(fusebox_fsid)) {
    bool result = mount_points->RegisterFileSystem(
        fusebox_fsid, storage::kFileSystemTypeFuseBox,
        storage::FileSystemMountOption(), fusebox_volume->mount_path());
    LOG_IF(ERROR, !result) << "invalid FuseBox ADP mount path";
    DCHECK(result);
  }

  // Mount the fusebox ADP storage device in files app.
  DoMountEvent(std::move(fusebox_volume));
}

void VolumeManager::OnDocumentsProviderRootRemoved(const std::string& authority,
                                                   const std::string& root_id) {
  DoUnmountEvent(*Volume::CreateForDocumentsProvider(
      authority, root_id, std::string(), std::string(), GURL(), false,
      /*optional_fusebox_subdir=*/std::string()));
  arc::ArcDocumentsProviderRootMap::GetForArcBrowserContext()->UnregisterRoot(
      authority, root_id);

  // Unmount the fusebox ADP storage device in files app.
  std::string volume_id = arc::GetDocumentsProviderVolumeId(authority, root_id);
  base::WeakPtr<Volume> fusebox_volume =
      FindVolumeById(util::kFuseBox + volume_id);
  if (fusebox_volume) {
    DoUnmountEvent(*fusebox_volume);
  }

  // Remove the fusebox ADP storage device from chrome::storage.
  std::string subdir = FuseBoxSubdirADP(authority, root_id);
  auto* mount_points = storage::ExternalMountPoints::GetSystemInstance();
  const std::string fusebox_fsid =
      base::StrCat({util::kFuseBoxMountNamePrefix, subdir});
  mount_points->RevokeFileSystem(fusebox_fsid);

  // Detach the fusebox ADP storage device from the fusebox daemon.
  if (fusebox_daemon_) {
    fusebox_daemon_->DetachStorage(subdir);
  }
}

void VolumeManager::OnClipboardDataChanged() {
  // Ignore the event created when we change the clipboard.
  if (ignore_clipboard_changed_) {
    return;
  }

  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  if (!clipboard) {
    return;
  }

  ui::DataTransferEndpoint dte(ui::EndpointType::kClipboardHistory);
  std::string web_custom_data;
  const ui::ClipboardData* data = clipboard->GetClipboardData(&dte);
  if (data) {
    web_custom_data = data->GetDataTransferCustomData();
  }
  if (web_custom_data.empty()) {
    return;
  }

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(web_custom_data));
  std::vector<ui::FileInfo> file_info =
      file_manager::util::ParseFileSystemSources(
          base::OptionalToPtr(data->source()), pickle);
  if (!file_info.empty()) {
    auto with_files = std::make_unique<ui::ClipboardData>(*data);
    with_files->set_filenames(std::move(file_info));
    base::AutoReset<bool> reset(&ignore_clipboard_changed_, true);
    clipboard->WriteClipboardData(std::move(with_files));
  }
}

void VolumeManager::AddSmbFsVolume(const base::FilePath& mount_point,
                                   const std::string& display_name) {
  DoMountEvent(Volume::CreateForSmb(mount_point, display_name));
}

void VolumeManager::RemoveSmbFsVolume(const base::FilePath& mount_point) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DoUnmountEvent(*Volume::CreateForSmb(mount_point, ""));
}

void VolumeManager::OnMigrationSucceededForTesting() {
  OnMigrationSucceeded();
}

void VolumeManager::OnDiskMountManagerRefreshed(bool success) {
  if (!success) {
    LOG(ERROR) << "Cannot refresh disk mount manager";
    return;
  }

  std::vector<std::unique_ptr<Volume>> archives;

  const ash::disks::DiskMountManager::MountPoints& mount_points =
      disk_mount_manager_->mount_points();
  for (const auto& mount_point : mount_points) {
    switch (mount_point.mount_type) {
      case ash::MountType::kArchive: {
        // Archives are mounted after other types of volume. See below.
        archives.push_back(Volume::CreateForRemovable(mount_point, nullptr));
        break;
      }
      case ash::MountType::kDevice: {
        DoMountEvent(Volume::CreateForRemovable(
            mount_point, disk_mount_manager_->FindDiskBySourcePath(
                             mount_point.source_path)));
        break;
      }
      case ash::MountType::kNetworkStorage: {
        break;
      }
      case ash::MountType::kInvalid: {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  // We mount archives only if they are opened from currently mounted volumes.
  // To check the condition correctly in DoMountEvent, we care about the order.
  std::vector<bool> done(archives.size(), false);
  for (size_t i = 0; i < archives.size(); ++i) {
    if (done[i]) {
      continue;
    }

    std::vector<std::unique_ptr<Volume>> chain;
    // done[x] = true means archives[x] is null and that volume is in |chain|.
    done[i] = true;
    chain.push_back(std::move(archives[i]));

    // If archives[i]'s source_path is in another archive, mount it first.
    for (size_t parent = i + 1; parent < archives.size(); ++parent) {
      if (!done[parent] && archives[parent]->mount_path().IsParent(
                               chain.back()->source_path())) {
        // done[parent] started false, so archives[parent] is non-null.
        done[parent] = true;
        chain.push_back(std::move(archives[parent]));
        parent = i + 1;  // Search archives[parent]'s parent from the beginning.
      }
    }

    // Mount from the tail of chain.
    while (!chain.empty()) {
      DoMountEvent(std::move(chain.back()));
      chain.pop_back();
    }
  }
}

void VolumeManager::OnStorageMonitorInitialized() {
  VLOG(1) << *this << "::OnStorageMonitorInitialized";

  const std::vector<storage_monitor::StorageInfo> storages =
      storage_monitor::StorageMonitor::GetInstance()->GetAllAvailableStorages();
  for (const storage_monitor::StorageInfo& storage : storages) {
    OnRemovableStorageAttached(storage);
  }

  storage_monitor::StorageMonitor::GetInstance()->AddObserver(this);
}

bool VolumeManager::DoMountEvent(std::unique_ptr<Volume> volume_ptr,
                                 ash::MountError error) {
  DCHECK(volume_ptr);
  const Volume& volume = *volume_ptr;

  // Archive files are mounted globally in system. We however don't want to show
  // archives from profile-specific folders (Drive/Downloads) of other users in
  // multi-profile session. To this end, we filter out archives not on the
  // volumes already mounted on this VolumeManager instance.
  if (volume.type() == VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
    // Source may be in Drive cache folder under the current profile directory.
    bool from_current_profile =
        profile_->GetPath().IsParent(volume.source_path());
    for (const auto& mounted_volume : mounted_volumes_) {
      DCHECK(mounted_volume);
      if (mounted_volume->mount_path().IsParent(volume.source_path())) {
        from_current_profile = true;
        break;
      }
    }
    if (!from_current_profile) {
      return false;
    }
  }

  // Filter out removable disks if forbidden by policy for this profile.
  if (volume.type() == VOLUME_TYPE_REMOVABLE_DISK_PARTITION &&
      profile_->GetPrefs()->GetBoolean(
          disks::prefs::kExternalStorageDisabled)) {
    return false;
  }

  bool inserted = false;

  if (error == ash::MountError::kSuccess ||
      volume.mount_condition() != ash::MountError::kSuccess) {
    const auto [it, ok] = mounted_volumes_.insert(std::move(volume_ptr));
    if (ok) {
      inserted = true;
      VLOG(1) << "Added volume '" << volume.volume_id() << "'";
      UMA_HISTOGRAM_ENUMERATION("FileBrowser.VolumeType", volume.type(),
                                NUM_VOLUME_TYPE);
    } else {
      DCHECK(volume_ptr);
      DCHECK_EQ((*it)->volume_id(), volume.volume_id());

      // It is possible for a Volume object with different properties to be
      // inserted here. Replace the Volume in |mounted_volumes_|.
      const_cast<std::unique_ptr<Volume>&>(*it) = std::move(volume_ptr);
      VLOG(1) << "Replaced volume '" << volume.volume_id() << "'";
    }

    DCHECK_EQ(&volume, it->get());
  }

  for (auto& observer : observers_) {
    observer.OnVolumeMounted(error, volume);
  }

  return inserted;
}

void VolumeManager::DoUnmountEvent(Volumes::const_iterator it,
                                   const ash::MountError error) {
  DCHECK(it != mounted_volumes_.end());

  // Hold a reference to the removed Volume from |mounted_volumes_|, because
  // OnVolumeMounted() will access it.
  const Volume& volume = **it;
  Volumes::node_type node_to_delete;
  if (error == ash::MountError::kSuccess) {
    node_to_delete = mounted_volumes_.extract(std::move(it));
  }

  VLOG_IF(1, node_to_delete) << "Removed volume '" << volume.volume_id() << "'";

  for (auto& observer : observers_) {
    observer.OnVolumeUnmounted(error, volume);
  }
}

base::FilePath VolumeManager::GetDriveMountPointPath() const {
  return drive_integration_service_->GetMountPointPath();
}

void VolumeManager::DoUnmountEvent(std::string_view volume_id,
                                   ash::MountError error) {
  Volumes::const_iterator it = mounted_volumes_.find(volume_id);
  if (it == mounted_volumes_.end()) {
    LOG(WARNING) << "Cannot find volume '" << volume_id << "' to unmount it";
    return;
  }

  DoUnmountEvent(std::move(it), error);
}

void VolumeManager::OnSshfsCrostiniUnmountCallback(
    const base::FilePath& sshfs_mount_path,
    RemoveSshfsCrostiniVolumeCallback callback,
    ash::MountError error) {
  if ((error == ash::MountError::kSuccess) ||
      (error == ash::MountError::kPathNotMounted)) {
    // Remove metadata associated with the mount. It will be a no-op if it
    // wasn't mounted or unmounted out of band.
    DoUnmountEvent(
        *Volume::CreateForSshfsCrostini(sshfs_mount_path, base::FilePath()));
    if (callback) {
      std::move(callback).Run(true);
    }
    return;
  }

  LOG(ERROR) << "Cannot unmount '" << sshfs_mount_path << "'";
  if (callback) {
    std::move(callback).Run(false);
  }
}

void VolumeManager::OnSftpGuestOsUnmountCallback(
    const base::FilePath& sftp_mount_path,
    const guest_os::VmType vm_type,
    RemoveSftpGuestOsVolumeCallback callback,
    ash::MountError error) {
  if ((error == ash::MountError::kSuccess) ||
      (error == ash::MountError::kPathNotMounted)) {
    // Remove metadata associated with the mount. It will be a no-op if it
    // wasn't mounted or unmounted out of band. We need the VolumeId to be
    // consistent, which means the mount path needs to be the same.
    // display_name, remote_mount_path and vm_type aren't needed and we don't
    // know them at unmount so leave them blank.
    DoUnmountEvent(*Volume::CreateForSftpGuestOs("", sftp_mount_path,
                                                 base::FilePath(), vm_type));
    if (callback) {
      std::move(callback).Run(true);
    }
    return;
  }

  LOG(ERROR) << "Cannot unmount SFTP path '" << sftp_mount_path
             << "': " << error;
  if (callback) {
    std::move(callback).Run(false);
  }
}

void VolumeManager::MountDownloadsVolume(bool read_only) {
  const base::FilePath localVolume =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  const bool success = RegisterDownloadsMountPoint(profile_, localVolume);
  DCHECK(success);
  DoMountEvent(Volume::CreateForDownloads(localVolume, {}, nullptr, read_only));
  if (ash::features::IsFileManagerFuseBoxDebugEnabled()) {
    if (auto volume = CreateForFuseBoxDownloads(profile_, fusebox_daemon_.get(),
                                                "fusebox Downloads")) {
      DoMountEvent(std::move(volume));
    }
  }

  // Asynchronously record the disk usage for the downloads path.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RecordDownloadsDiskUsageStats, std::move(localVolume)));
}

void VolumeManager::UnmountDownloadsVolume() {
  const base::FilePath localVolume =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  RevokeDownloadsMountPoint(profile_, localVolume);
  DoUnmountEvent(*Volume::CreateForDownloads(localVolume));
  if (ash::features::IsFileManagerFuseBoxDebugEnabled()) {
    if (auto volume = CreateForFuseBoxDownloads(profile_, fusebox_daemon_.get(),
                                                "fusebox Downloads")) {
      DoUnmountEvent(*volume);
    }
  }
}

void VolumeManager::MountArcRoots() {
  DCHECK(IsArcEnabled(profile_));
  if (arc_volumes_mounted_) {
    return;
  }
  DoMountEvent(Volume::CreateForMediaView(arc::kImagesRootId));
  DoMountEvent(Volume::CreateForMediaView(arc::kVideosRootId));
  DoMountEvent(Volume::CreateForMediaView(arc::kAudioRootId));
  DoMountEvent(Volume::CreateForMediaView(arc::kDocumentsRootId));
  if (!arc::IsArcVmEnabled()) {
    DoMountEvent(
        Volume::CreateForAndroidFiles(base::FilePath(util::kAndroidFilesPath)));
  }
  arc_volumes_mounted_ = true;
}

void VolumeManager::UnmountArcRoots() {
  DCHECK(IsArcEnabled(profile_));
  if (!arc_volumes_mounted_) {
    return;
  }
  DoUnmountEvent(*Volume::CreateForMediaView(arc::kImagesRootId));
  DoUnmountEvent(*Volume::CreateForMediaView(arc::kVideosRootId));
  DoUnmountEvent(*Volume::CreateForMediaView(arc::kAudioRootId));
  DoUnmountEvent(*Volume::CreateForMediaView(arc::kDocumentsRootId));
  if (!arc::IsArcVmEnabled()) {
    DoUnmountEvent(*Volume::CreateForAndroidFiles(
        base::FilePath(util::kAndroidFilesPath)));
  }
  arc_volumes_mounted_ = false;
}

void VolumeManager::UnsubscribeFromArcEvents() {
  if (!IsArcEnabled(profile_)) {
    return;
  }
  // TODO(crbug.com/40497410): We need nullptr check here because
  // ArcSessionManager may or may not be alive at this point.
  if (arc::ArcSessionManager::Get()) {
    arc_session_manager_observation_.Reset();
  }
}

void VolumeManager::SubscribeAndMountArc() {
  if (!IsArcEnabled(profile_)) {
    return;
  }
  documents_provider_root_manager_->AddObserver(this);
  // Registers a mount point for Android files only when the flag is enabled.
  RegisterAndroidFilesMountPoint();
  if (arc::ArcSessionManager::Get()) {
    arc_session_manager_observation_.Observe(arc::ArcSessionManager::Get());
  } else {
    // Can be NULL only in tests.
    CHECK_IS_TEST();
  }
  // Trigger mounting if enabled by policy.
  OnArcPlayStoreEnabledChanged(arc::IsArcPlayStoreEnabledForProfile(profile_));
}

void VolumeManager::UnsubscribeAndUnmountArc() {
  if (!IsArcEnabled(profile_)) {
    return;
  }
  documents_provider_root_manager_->RemoveObserver(this);
  UnsubscribeFromArcEvents();
  UnmountArcRoots();
  RevokeAndroidFilesMountPoint();
}

void VolumeManager::OnLocalUserFilesEnabled() {
  CHECK(policy::local_user_files::LocalUserFilesAllowed());
  MountDownloadsVolume();
  SubscribeAndMountArc();
}

void VolumeManager::OnLocalUserFilesDisabled() {
  CHECK(!policy::local_user_files::LocalUserFilesAllowed());
  UnsubscribeAndUnmountArc();
  UnmountDownloadsVolume();
  if (IsSkyVaultV2Enabled() && read_only_local_folders_) {
    // Keep the volume in GA version. It will be removed after migration.
    // TODO(aidazolic): Do not mount if the local files migration succeeded.
    MountDownloadsVolume(/*read_only=*/true);
  }
}

void VolumeManager::OnMigrationSucceeded() {
  if (policy::local_user_files::LocalUserFilesAllowed()) {
    LOG(ERROR)
        << "OnMigrationSucceeded() called but local files allowed, ignoring.";
    return;
  }

  read_only_local_folders_ = false;
  OnLocalUserFilesDisabled();
}

}  // namespace file_manager
