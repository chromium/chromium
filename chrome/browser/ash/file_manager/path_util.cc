// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/path_util.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/smb_client/smbfs_share.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/drive/file_system_core_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace file_manager {
namespace util {

namespace {

constexpr char kAndroidFilesMountPointName[] = "android_files";
constexpr char kCrostiniMapGoogleDrive[] = "GoogleDrive";
constexpr char kCrostiniMapLinuxFiles[] = "LinuxFiles";
constexpr char kCrostiniMapMyDrive[] = "MyDrive";
constexpr char kCrostiniMapPlayFiles[] = "PlayFiles";
constexpr char kCrostiniMapSmbFs[] = "SMB";
constexpr char kCrostiniMapTeamDrives[] = "SharedDrives";
constexpr char kCrostiniMapSharedWithMe[] = "SharedWithMe";
constexpr char kCrostiniMapShortcutsSharedWithMe[] = "ShortcutsSharedWithMe";
constexpr char kFolderNameDownloads[] = "Downloads";
constexpr char kFolderNameMyFiles[] = "MyFiles";
constexpr char kFolderNamePvmDefault[] = "PvmDefault";
constexpr char kFolderNameCamera[] = "Camera";
constexpr char kFolderNameShareCache[] = "ShareCache";
constexpr char kDisplayNameGoogleDrive[] = "Google Drive";
constexpr char kDriveFsDirComputers[] = "Computers";
constexpr char kDriveFsDirSharedWithMe[] = ".files-by-id";
constexpr char kDriveFsDirShortcutsSharedWithMe[] = ".shortcut-targets-by-id";
constexpr char kDriveFsDirRoot[] = "root";
constexpr char kDriveFsDirTeamDrives[] = "team_drives";

// Sync with the root name defined with the file provider in ARC++ side.
constexpr base::FilePath::CharType kArcDownloadRoot[] =
    FILE_PATH_LITERAL("/download");
constexpr base::FilePath::CharType kArcExternalFilesRoot[] =
    FILE_PATH_LITERAL("/external_files");
// Sync with the volume provider in ARC++ side.
constexpr char kArcStorageContentUrlPrefix[] =
    "content://org.chromium.arc.volumeprovider/";
// A predefined removable media UUID for testing. Defined in
// ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
// TODO(crbug.com/1274481): Move ash-wide constants to a common place.
constexpr char kArcRemovableMediaUuidForTesting[] =
    "00000000000000000000000000000000DEADBEEF";
// The dummy UUID of the MyFiles volume is taken from
// ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
// TODO(crbug.com/929031): Move MyFiles constants to a common place.
constexpr char kArcMyFilesContentUrlPrefix[] =
    "content://org.chromium.arc.volumeprovider/"
    "0000000000000000000000000000CAFEF00D2019/";

// Helper function for |ConvertToContentUrls|.
void OnSingleContentUrlResolved(const base::RepeatingClosure& barrier_closure,
                                std::vector<GURL>* out_urls,
                                size_t index,
                                const GURL& url) {
  (*out_urls)[index] = url;
  barrier_closure.Run();
}

// Helper function for |ConvertToContentUrls|.
void OnAllContentUrlsResolved(
    ConvertToContentUrlsCallback callback,
    std::unique_ptr<std::vector<GURL>> urls,
    std::unique_ptr<std::vector<base::FilePath>> paths_to_share) {
  std::move(callback).Run(*urls, *paths_to_share);
}

// By default, in ChromeOS it uses the $profile_dir/MyFiles however,
// for manual tests/development in linux-chromeos it uses $HOME/Downloads for
// chrome binary and a temp dir for browser tests. The flag
// `--use-myfiles-in-user-data-dir-for-testing` forces the ChromeOS pattern,
// even for non-ChromeOS environments.
bool ShouldMountPrimaryUserDownloads(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kUseMyFilesInUserDataDirForTesting)) {
    return false;
  }

  if (!base::SysInfo::IsRunningOnChromeOS() &&
      user_manager::UserManager::IsInitialized()) {
    const user_manager::User* const user =
        ash::ProfileHelper::Get()->GetUserByProfile(
            profile->GetOriginalProfile());
    const user_manager::User* const primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    return user == primary_user;
  }

  return false;
}

// Extracts the Drive path from the given path located under the legacy Drive
// mount point. Returns an empty path if |path| is not under the legacy Drive
// mount point.
// Example: ExtractLegacyDrivePath("/special/drive-xxx/foo.txt") =>
//   "drive/foo.txt"
base::FilePath ExtractLegacyDrivePath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  if (components.size() < 3) {
    return base::FilePath();
  }
  if (components[0] != FILE_PATH_LITERAL("/")) {
    return base::FilePath();
  }
  if (components[1] != FILE_PATH_LITERAL("special")) {
    return base::FilePath();
  }
  static const base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("drive");
  if (components[2].compare(0, std::size(kPrefix) - 1, kPrefix) != 0) {
    return base::FilePath();
  }

  base::FilePath drive_path = drive::util::GetDriveGrandRootPath();
  for (size_t i = 3; i < components.size(); ++i) {
    drive_path = drive_path.Append(components[i]);
  }
  return drive_path;
}

// Extracts the volume name of a removable device. |relative_path| is expected
// to be of the form <volume name>/..., which is relative to /media/removable.
std::string ExtractVolumeNameFromRelativePathForRemovableMedia(
    const base::FilePath& relative_path) {
  std::vector<base::FilePath::StringType> components =
      relative_path.GetComponents();
  if (components.empty()) {
    LOG(WARNING) << "Failed to extract volume name from relative path: "
                 << relative_path;
    return std::string();
  }
  return components[0];
}

// Returns the source path of a removable device using its volume name as a key.
// An empty string is returned when it fails to get a valid mount point from
// DiskMountManager.
std::string GetSourcePathForRemovableMedia(const std::string& volume_name) {
  const std::string mount_path(
      base::StringPrintf("%s/%s", kRemovableMediaPath, volume_name.c_str()));
  const auto& mount_points =
      ash::disks::DiskMountManager::GetInstance()->mount_points();
  const auto found = mount_points.find(mount_path);
  return found == mount_points.end() ? std::string() : found->source_path;
}

// Returns the UUID of a removable device using its volume name as a key.
// An empty string is returned when it fails to get valid source path and disk
// from DiskMountManager.
std::string GetFsUuidForRemovableMedia(const std::string& volume_name) {
  const std::string source_path = GetSourcePathForRemovableMedia(volume_name);
  if (source_path.empty()) {
    LOG(WARNING) << "No source path is found for volume name: " << volume_name;
    return std::string();
  }
  const ash::disks::Disk* disk =
      ash::disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(
          source_path);
  std::string fs_uuid = disk == nullptr ? std::string() : disk->fs_uuid();
  if (fs_uuid.empty()) {
    LOG(WARNING) << "No UUID is found for volume name: " << volume_name;
  }
  return fs_uuid;
}

// Same as parent.AppendRelativePath(child, path) except that it allows
// parent == child, in which case path is unchanged.
bool AppendRelativePath(const base::FilePath& parent,
                        const base::FilePath& child,
                        base::FilePath* path) {
  return child == parent || parent.AppendRelativePath(child, path);
}

// Translates known DriveFS folders into their localized message id.
absl::optional<int> DriveFsFolderToMessageId(std::string folder) {
  if (folder == kDriveFsDirRoot) {
    return IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL;
  } else if (folder == kDriveFsDirTeamDrives) {
    return IDS_FILE_BROWSER_DRIVE_SHARED_DRIVES_LABEL;
  } else if (folder == kDriveFsDirComputers) {
    return IDS_FILE_BROWSER_DRIVE_COMPUTERS_LABEL;
  } else if (folder == kDriveFsDirSharedWithMe) {
    return IDS_FILE_BROWSER_DRIVE_SHARED_WITH_ME_COLLECTION_LABEL;
  } else if (folder == kDriveFsDirShortcutsSharedWithMe) {
    return IDS_FILE_BROWSER_DRIVE_SHARED_WITH_ME_COLLECTION_LABEL;
  }
  return absl::nullopt;
}

// Translates special My Files folders into their localized message id.
absl::optional<int> MyFilesFolderToMessageId(std::string folder) {
  if (folder == kFolderNameDownloads) {
    return IDS_FILE_BROWSER_DOWNLOADS_DIRECTORY_LABEL;
  } else if (folder == kFolderNamePvmDefault) {
    return IDS_FILE_BROWSER_PLUGIN_VM_DIRECTORY_LABEL;
  } else if (folder == kFolderNameCamera) {
    return IDS_FILE_BROWSER_CAMERA_DIRECTORY_LABEL;
  }
  return absl::nullopt;
}

}  // namespace

const base::FilePath::CharType kFuseBoxMediaPath[] =
    FILE_PATH_LITERAL("/media/fuse/fusebox");

const base::FilePath::CharType kFuseBoxMediaSlashPath[] =
    FILE_PATH_LITERAL("/media/fuse/fusebox/");

const base::FilePath::CharType kRemovableMediaPath[] =
    FILE_PATH_LITERAL("/media/removable");

const base::FilePath::CharType kAndroidFilesPath[] =
    FILE_PATH_LITERAL("/run/arc/sdcard/write/emulated/0");

const base::FilePath::CharType kGuestOsAndroidFilesPath[] =
    FILE_PATH_LITERAL("/media/fuse/android_files");

const base::FilePath::CharType kSystemFontsPath[] =
    FILE_PATH_LITERAL("/usr/share/fonts");

const base::FilePath::CharType kArchiveMountPath[] =
    FILE_PATH_LITERAL("/media/archive");

const char kFuseBox[] = "fusebox";

// The actual value of this string is arbitrary (other than, per the comments
// in external_mount_points.h, mount names should not contain '/'), but this
// nonsense-looking word (based on the first two letters of each of "fuse box
// mount name") is unique enough so that, when seeing "fubomona" in a log
// message (e.g. in a storage::FileSystemURL's debug string form), code-
// searching for that string should quickly find this definition here (and the
// kFuseBoxMountNamePrefix name that code-search can find references for).
//
// This is just a prefix. The complete mount name (for FuseBox mounts, not for
// storage::ExternalMountPoints generally) looks like "fubomona:volumetype.etc"
// where the volumetype (e.g. "adp", "fsp") acts like a namespace so that ADP's
// "etc" format won't accidentally conflict with FSP's "etc" format.
//
// This means that the "volumetype.etc" string value *can* be the same as a
// FuseBox subdir string value, as they're both prefixed with "volumetype.",
// but they don't *have* to be. Specifically, the "etc" may contain identifiers
// that other in-process Chromium code wants to parse but those identifiers
// might be longer than Linux's NAME_MAX.
const char kFuseBoxMountNamePrefix[] = "fubomona:";

const char kFuseBoxSubdirPrefixADP[] = "adp.";
const char kFuseBoxSubdirPrefixFSP[] = "fsp.";
const char kFuseBoxSubdirPrefixMTP[] = "mtp.";
const char kFuseBoxSubdirPrefixTMP[] = "tmp.";

const char kShareCacheMountPointName[] = "ShareCache";

const url::Origin& GetFilesAppOrigin() {
  static const base::NoDestructor<url::Origin> origin(
      [] { return url::Origin::Create(GetFileManagerURL()); }());
  return *origin;
}

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // Check if FilesApp has a registered path already.  This happens for tests.
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path)) {
    return path.AppendASCII(kFolderNameDownloads);
  }

  // Return $HOME/Downloads as Download folder.
  if (ShouldMountPrimaryUserDownloads(profile)) {
    return DownloadPrefs::GetDefaultDownloadDirectory();
  }

  // Return <cryptohome>/MyFiles/Downloads if it feature is enabled.
  return profile->GetPath()
      .AppendASCII(kFolderNameMyFiles)
      .AppendASCII(kFolderNameDownloads);
}

base::FilePath GetMyFilesFolderForProfile(Profile* profile) {
  // Check if FilesApp has a registered path already. This happens for tests.
  const std::string mount_point_name =
      util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path)) {
    return path;
  }

  // Return $HOME/Downloads as MyFiles folder.
  if (ShouldMountPrimaryUserDownloads(profile)) {
    return DownloadPrefs::GetDefaultDownloadDirectory();
  }

  // Return <cryptohome>/MyFiles.
  return profile->GetPath().AppendASCII(kFolderNameMyFiles);
}

base::FilePath GetShareCacheFilePath(Profile* profile) {
  return profile->GetPath().AppendASCII(kFolderNameShareCache);
}

base::FilePath GetAndroidFilesPath() {
  // Check if Android has a registered path already. This happens for tests.
  const std::string mount_point_name = util::GetAndroidFilesMountPointName();
  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath path;
  if (mount_points->GetRegisteredPath(mount_point_name, &path)) {
    return path;
  }
  if (arc::IsArcVmEnabled()) {
    return base::FilePath(file_manager::util::kGuestOsAndroidFilesPath);
  }
  return base::FilePath(file_manager::util::kAndroidFilesPath);
}

bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_base,
                              const base::FilePath& old_path,
                              base::FilePath* new_path) {
  // Special case, migrating /home/chronos/user which is set early (before a
  // profile is attached to the browser process) to default to
  // /home/chronos/u-{hash}/MyFiles/Downloads.
  if (old_path == old_base &&
      old_path == base::FilePath("/home/chronos/user")) {
    *new_path = GetDownloadsFolderForProfile(profile);
    return true;
  }

  // If the `new_base` is already parent of `old_path`, no need to migrate.
  const base::FilePath new_base = GetMyFilesFolderForProfile(profile);
  if (new_base.IsParent(old_path)) {
    return false;
  }

  base::FilePath relative;
  if (old_base.AppendRelativePath(old_path, &relative)) {
    *new_path = new_base.Append(relative);
    return old_path != *new_path;
  }

  return false;
}

bool MigrateToDriveFs(Profile* profile,
                      const base::FilePath& old_path,
                      base::FilePath* new_path) {
  const auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->is_enabled() || !user ||
      !user->GetAccountId().HasAccountIdKey()) {
    return false;
  }
  *new_path = integration_service->GetMountPointPath();
  return drive::util::GetDriveGrandRootPath().AppendRelativePath(
      ExtractLegacyDrivePath(old_path), new_path);
}

std::string GetDownloadsMountPointName(Profile* profile) {
  // To distinguish profiles in multi-profile session, we append user name hash
  // to "Downloads". Note that some profiles (like login or test profiles)
  // are not associated with an user account. In that case, no suffix is added
  // because such a profile never belongs to a multi-profile session.
  const user_manager::User* const user =
      user_manager::UserManager::IsInitialized()
          ? ash::ProfileHelper::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : nullptr;
  const std::string id = user ? "-" + user->username_hash() : "";
  return base::EscapeQueryParamValue(kFolderNameDownloads + id, false);
}

std::string GetAndroidFilesMountPointName() {
  return kAndroidFilesMountPointName;
}

// Returns true if |name| is a known Bruschetta mount point name (e.g. as
// produced by GetGuestOsMountPointName), and populates |guest_id|.
bool IsBruschettaMountPointName(const std::string& name,
                                Profile* profile,
                                guest_os::GuestId* guest_id) {
  auto* service = guest_os::GuestOsService::GetForProfile(profile);
  if (!service) {
    return false;
  }
  auto* registry = service->MountProviderRegistry();
  for (const auto id : registry->List()) {
    auto* provider = registry->Get(id);
    if (provider->vm_type() != vm_tools::apps::VmType::BRUSCHETTA) {
      continue;
    }
    if (name == util::GetGuestOsMountPointName(profile, provider->GuestId())) {
      *guest_id = provider->GuestId();
      return true;
    }
  }
  return false;
}

std::string GetCrostiniMountPointName(Profile* profile) {
  // crostini_<hash>_termina_penguin
  return base::JoinString(
      {"crostini", crostini::CryptohomeIdForProfile(profile),
       crostini::kCrostiniDefaultVmName,
       crostini::kCrostiniDefaultContainerName},
      "_");
}

std::string GetGuestOsMountPointName(Profile* profile,
                                     const guest_os::GuestId& id) {
  if (id.vm_type == guest_os::VmType::ARCVM) {
    return kAndroidFilesMountPointName;
  }
  return base::JoinString(
      {"guestos", ash::ProfileHelper::GetUserIdHashFromProfile(profile),
       base::EscapeAllExceptUnreserved(id.vm_name),
       base::EscapeAllExceptUnreserved(id.container_name)},
      "+");
}

base::FilePath GetCrostiniMountDirectory(Profile* profile) {
  return base::FilePath("/media/fuse/" + GetCrostiniMountPointName(profile));
}

base::FilePath GetGuestOsMountDirectory(std::string mountPointName) {
  return base::FilePath("/media/fuse/" + mountPointName);
}

bool ConvertFileSystemURLToPathInsideVM(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    const base::FilePath& vm_mount,
    bool map_crostini_home,
    base::FilePath* inside) {
  const std::string& id(file_system_url.mount_filesystem_id());
  // File system root requires strip trailing separator.
  base::FilePath path =
      base::FilePath(file_system_url.virtual_path()).StripTrailingSeparators();
  // Include drive if using DriveFS.
  std::string mount_point_name_drive;
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service) {
    mount_point_name_drive =
        integration_service->GetMountPointPath().BaseName().value();
  }

  // Reformat virtual_path() from:
  //   <mount_label>/path/to/file
  // To:
  //   <vm_mount>/<mapping>/path/to/file
  // If |map_crostini_home| is set, paths in crostini mount map to:
  //   /<home-directory>/path/to/file
  base::FilePath base_to_exclude(id);
  guest_os::GuestId guest_id("", "");
  if (id == GetDownloadsMountPointName(profile)) {
    // MyFiles.
    *inside = vm_mount.Append(kFolderNameMyFiles);
  } else if (!mount_point_name_drive.empty() && id == mount_point_name_drive) {
    // DriveFS has some more complicated mappings.
    std::vector<base::FilePath::StringType> components = path.GetComponents();
    *inside = vm_mount.Append(kCrostiniMapGoogleDrive);
    if (components.size() >= 2 && components[1] == kDriveFsDirRoot) {
      // root -> MyDrive.
      base_to_exclude = base_to_exclude.Append(kDriveFsDirRoot);
      *inside = inside->Append(kCrostiniMapMyDrive);
    } else if (components.size() >= 2 &&
               components[1] == kDriveFsDirTeamDrives) {
      // team_drives -> SharedDrives.
      base_to_exclude = base_to_exclude.Append(kDriveFsDirTeamDrives);
      *inside = inside->Append(kCrostiniMapTeamDrives);
    } else if (components.size() >= 2 &&
               components[1] == kDriveFsDirSharedWithMe) {
      // .files-by-id -> SharedWithMe.
      base_to_exclude = base_to_exclude.Append(kDriveFsDirSharedWithMe);
      *inside = inside->Append(kCrostiniMapSharedWithMe);
    } else if (components.size() >= 2 &&
               components[1] == kDriveFsDirShortcutsSharedWithMe) {
      // .shortcut-targets-by-id -> ShortcutsSharedWithMe.
      base_to_exclude =
          base_to_exclude.Append(kDriveFsDirShortcutsSharedWithMe);
      *inside = inside->Append(kCrostiniMapShortcutsSharedWithMe);
    }
    // Computers -> Computers
  } else if (id == ash::kSystemMountNameRemovable) {
    // Removable.
    *inside = vm_mount.Append(ash::kSystemMountNameRemovable);
  } else if (id == GetAndroidFilesMountPointName()) {
    // PlayFiles.
    *inside = vm_mount.Append(kCrostiniMapPlayFiles);
  } else if (id == ash::kSystemMountNameArchive) {
    // Archive.
    *inside = vm_mount.Append(ash::kSystemMountNameArchive);
  } else if (id == GetCrostiniMountPointName(profile)) {
    // Crostini.
    if (map_crostini_home) {
      auto container_info =
          guest_os::GuestOsSessionTracker::GetForProfile(profile)->GetInfo(
              crostini::DefaultContainerId());
      if (!container_info) {
        return false;
      }
      *inside = container_info->homedir;
    } else {
      *inside = vm_mount.Append(kCrostiniMapLinuxFiles);
    }
  } else if (IsBruschettaMountPointName(id, profile, &guest_id)) {
    // Bruschetta: use path to homedir, which is currently the empty string
    // because sftp-server inside the VM runs in the homedir.
    auto container_info =
        guest_os::GuestOsSessionTracker::GetForProfile(profile)->GetInfo(
            guest_id);
    if (!container_info) {
      return false;
    }
    *inside = container_info->homedir;
  } else if (file_system_url.type() == storage::kFileSystemTypeSmbFs) {
    // SMB. Do not assume the share is currently accessible via SmbService
    // as this function is called during unmount when SmbFsShare is
    // destroyed. The only information safely available is the stable
    // mount ID.
    *inside = vm_mount.Append(kCrostiniMapSmbFs);
    *inside = inside->Append(id);
  } else {
    return false;
  }
  return AppendRelativePath(base_to_exclude, path, inside);
}

bool ConvertFileSystemURLToPathInsideCrostini(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    base::FilePath* inside) {
  return ConvertFileSystemURLToPathInsideVM(
      profile, file_system_url, crostini::ContainerChromeOSBaseDirectory(),
      /*map_crostini_home=*/true, inside);
}

bool ConvertPathInsideVMToFileSystemURL(
    Profile* profile,
    const base::FilePath& inside,
    const base::FilePath& vm_mount,
    bool map_crostini_home,
    storage::FileSystemURL* file_system_url) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // Include drive if using DriveFS.
  std::string mount_point_name_drive;
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service) {
    mount_point_name_drive =
        integration_service->GetMountPointPath().BaseName().value();
  }

  std::string mount_name;
  base::FilePath path;
  base::FilePath relative_path;

  if (map_crostini_home) {
    auto container_info =
        guest_os::GuestOsSessionTracker::GetForProfile(profile)->GetInfo(
            crostini::DefaultContainerId());
    if (container_info &&
        AppendRelativePath(container_info->homedir, inside, &relative_path)) {
      *file_system_url = mount_points->CreateExternalFileSystemURL(
          blink::StorageKey::CreateFirstParty(GetFilesAppOrigin()),
          GetCrostiniMountPointName(profile), relative_path);
      return file_system_url->is_valid();
    }
  }

  if (!vm_mount.AppendRelativePath(inside, &path)) {
    return false;
  }

  if (AppendRelativePath(base::FilePath(kFolderNameMyFiles), path,
                         &relative_path)) {
    // MyFiles.
    mount_name = GetDownloadsMountPointName(profile);
    path = relative_path;
  } else if (AppendRelativePath(base::FilePath(kCrostiniMapLinuxFiles), path,
                                &relative_path)) {
    // LinuxFiles.
    mount_name = GetCrostiniMountPointName(profile);
    path = relative_path;
  } else if (base::FilePath(kCrostiniMapGoogleDrive)
                 .AppendRelativePath(path, &relative_path)) {
    mount_name = mount_point_name_drive;
    path = relative_path;
    relative_path.clear();
    // GoogleDrive
    if (AppendRelativePath(base::FilePath(kCrostiniMapMyDrive), path,
                           &relative_path)) {
      // /GoogleDrive/MyDrive -> root
      path = base::FilePath(kDriveFsDirRoot).Append(relative_path);
    } else if (AppendRelativePath(base::FilePath(kCrostiniMapTeamDrives), path,
                                  &relative_path)) {
      // /GoogleDrive/SharedDrive -> team_drives
      path = base::FilePath(kDriveFsDirTeamDrives).Append(relative_path);
    } else if (AppendRelativePath(base::FilePath(kCrostiniMapSharedWithMe),
                                  path, &relative_path)) {
      // /GoogleDrive/SharedWithMe -> .files-by-id
      path = base::FilePath(kDriveFsDirSharedWithMe).Append(relative_path);
    } else if (AppendRelativePath(
                   base::FilePath(kCrostiniMapShortcutsSharedWithMe), path,
                   &relative_path)) {
      // /GoogleDrive/ShortcutsSharedWithMe -> .shortcut-targets-by-id
      path = base::FilePath(kDriveFsDirShortcutsSharedWithMe)
                 .Append(relative_path);
    }
    // Computers -> Computers
  } else if (base::FilePath(ash::kSystemMountNameRemovable)
                 .AppendRelativePath(path, &relative_path)) {
    // Removable subdirs only.
    mount_name = ash::kSystemMountNameRemovable;
    path = relative_path;
  } else if (AppendRelativePath(base::FilePath(kCrostiniMapPlayFiles), path,
                                &relative_path)) {
    // PlayFiles.
    mount_name = GetAndroidFilesMountPointName();
    path = relative_path;
  } else if (base::FilePath(ash::kSystemMountNameArchive)
                 .AppendRelativePath(path, &relative_path)) {
    // Archive subdirs only.
    mount_name = ash::kSystemMountNameArchive;
    path = relative_path;
  } else if (base::FilePath(kCrostiniMapSmbFs)
                 .AppendRelativePath(path, &relative_path)) {
    // SMB.
    std::vector<base::FilePath::StringType> components =
        relative_path.GetComponents();
    if (components.size() < 1) {
      return false;
    }
    mount_name = components[0];
    path.clear();
    base::FilePath(mount_name).AppendRelativePath(relative_path, &path);
  } else {
    return false;
  }

  *file_system_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(GetFilesAppOrigin()), mount_name,
      path);
  return file_system_url->is_valid();
}

bool ConvertPathToArcUrl(const base::FilePath& path,
                         GURL* const arc_url_out,
                         bool* const requires_sharing_out) {
  DCHECK(arc_url_out);
  DCHECK(requires_sharing_out);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  *requires_sharing_out = false;

  // Obtain the primary profile. This information is required because currently
  // only the file systems for the primary profile is exposed to ARC.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (!primary_profile) {
    return false;
  }

  // Convert paths under primary profile's Downloads directory.
  base::FilePath primary_downloads =
      GetDownloadsFolderForProfile(primary_profile);
  base::FilePath result_path(kArcDownloadRoot);
  if (primary_downloads.AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(kArcStorageContentUrlPrefix)
                       .Resolve(base::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under Android files root (e.g.,
  // /run/arc/sdcard/write/emulated/0).
  result_path = base::FilePath(kArcExternalFilesRoot);
  if (GetAndroidFilesPath().AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(kArcStorageContentUrlPrefix)
                       .Resolve(base::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under /media/removable.
  base::FilePath relative_path;
  if (base::FilePath(kRemovableMediaPath)
          .AppendRelativePath(path, &relative_path)) {
    const std::string volume_name =
        ExtractVolumeNameFromRelativePathForRemovableMedia(relative_path);
    if (volume_name.empty()) {
      return false;
    }
    const std::string fs_uuid = GetFsUuidForRemovableMedia(volume_name);
    // Replace the volume name in the relative path with the UUID.
    // When no UUID is found for the volume, use the predefined one for testing.
    base::FilePath relative_path_with_uuid = base::FilePath(
        fs_uuid.empty() ? kArcRemovableMediaUuidForTesting : fs_uuid);
    if (!base::FilePath(volume_name)
             .AppendRelativePath(relative_path, &relative_path_with_uuid)) {
      LOG(WARNING) << "Failed to replace volume name \"" << volume_name
                   << "\" in relative path \"" << relative_path
                   << "\" with UUID \"" << fs_uuid << "\"";
      return false;
    }
    *arc_url_out =
        GURL(kArcStorageContentUrlPrefix)
            .Resolve(base::EscapePath(relative_path_with_uuid.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under MyFiles.
  if (GetMyFilesFolderForProfile(primary_profile)
          .AppendRelativePath(path, &relative_path)) {
    *arc_url_out = GURL(kArcMyFilesContentUrlPrefix)
                       .Resolve(base::EscapePath(relative_path.AsUTF8Unsafe()));
    return true;
  }

  bool force_external = false;
  // Convert paths under DriveFS.
  const drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(primary_profile);
  if (integration_service &&
      integration_service->GetMountPointPath().AppendRelativePath(
          path, &relative_path)) {
    // TODO(b/157297349) Remove this condition.
    if (arc::IsArcVmEnabled()) {
      *arc_url_out =
          GURL("content://org.chromium.arc.volumeprovider/MyDrive/")
              .Resolve(base::EscapePath(relative_path.AsUTF8Unsafe()));
      *requires_sharing_out = true;
      return true;
    }

    // TODO(b/157297349): For backward compatibility with ARC++ P, force
    // external URL for DriveFS.
    force_external = true;
  }

  // Force external URL for Crostini.
  if (GetCrostiniMountDirectory(primary_profile)
          .AppendRelativePath(path, &relative_path)) {
    force_external = true;
  }

  // Convert path under /media/archive.
  if (base::FilePath(kArchiveMountPath)
          .AppendRelativePath(path, &relative_path)) {
    // TODO(b/157297349) Remove this condition.
    if (arc::IsArcVmEnabled()) {
      *arc_url_out =
          GURL("content://org.chromium.arc.volumeprovider/archive/")
              .Resolve(base::EscapePath(relative_path.AsUTF8Unsafe()));
      *requires_sharing_out = true;
      return true;
    }

    force_external = true;
  }

  // Convert path under /media/fuse/smb-...
  if (ash::smb_client::SmbService* const service =
          ash::smb_client::SmbServiceFactory::Get(primary_profile)) {
    if (const ash::smb_client::SmbFsShare* const share =
            service->GetSmbFsShareForPath(path)) {
      if (share->mount_path().AppendRelativePath(path, &relative_path)) {
        // TODO(b/157297349) Remove this condition.
        if (arc::IsArcVmEnabled()) {
          *arc_url_out =
              GURL(base::StrCat(
                       {"content://org.chromium.arc.volumeprovider/smb/",
                        share->mount_id(), "/"}))
                  .Resolve(base::EscapePath(relative_path.AsUTF8Unsafe()));
          *requires_sharing_out = true;
          return true;
        }

        force_external = true;
      }
    }
  }

  // ShareCache files are not available as mount-passthrough and must be shared
  // through ChromeContentProvider.
  if (GetShareCacheFilePath(primary_profile)
          .AppendRelativePath(path, &relative_path)) {
    force_external = true;
  }

  // Convert paths under /special or other paths forced to use external URL.
  GURL external_file_url =
      ash::CreateExternalFileURLFromPath(primary_profile, path, force_external);

  if (!external_file_url.is_empty()) {
    *arc_url_out = arc::EncodeToChromeContentProviderUrl(external_file_url);
    return true;
  }

  // TODO(kinaba): Add conversion logic once other file systems are supported.
  return false;
}

void ConvertToContentUrls(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (file_system_urls.empty()) {
    std::move(callback).Run(std::vector<GURL>(), std::vector<base::FilePath>());
    return;
  }

  auto* documents_provider_root_map =
      profile ? arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile)
              : nullptr;

  // To keep the original order, prefill |out_urls| with empty URLs and
  // specify index when updating it like (*out_urls)[index] = url.
  auto out_urls = std::make_unique<std::vector<GURL>>(file_system_urls.size());
  auto* out_urls_ptr = out_urls.get();
  auto paths_to_share = std::make_unique<std::vector<base::FilePath>>();
  auto* paths_to_share_ptr = paths_to_share.get();
  auto barrier = base::BarrierClosure(
      file_system_urls.size(),
      base::BindOnce(&OnAllContentUrlsResolved, std::move(callback),
                     std::move(out_urls), std::move(paths_to_share)));
  auto single_content_url_callback =
      base::BindRepeating(&OnSingleContentUrlResolved, barrier, out_urls_ptr);

  for (size_t index = 0; index < file_system_urls.size(); ++index) {
    const auto& file_system_url = file_system_urls[index];

    // Run DocumentsProvider check before running ConvertPathToArcUrl.
    // Otherwise, DocumentsProvider file path would be encoded to a
    // ChromeContentProvider URL (b/132314050).
    if (documents_provider_root_map) {
      base::FilePath file_path;
      auto* documents_provider_root =
          documents_provider_root_map->ParseAndLookup(file_system_url,
                                                      &file_path);
      if (documents_provider_root) {
        documents_provider_root->ResolveToContentUrl(
            file_path, base::BindOnce(single_content_url_callback, index));
        continue;
      }
    }

    GURL arc_url;
    bool requires_sharing = false;
    if (file_system_url.mount_type() == storage::kFileSystemTypeExternal &&
        ConvertPathToArcUrl(file_system_url.path(), &arc_url,
                            &requires_sharing)) {
      if (requires_sharing) {
        paths_to_share_ptr->push_back(file_system_url.path());
      }
      single_content_url_callback.Run(index, arc_url);
      continue;
    }

    single_content_url_callback.Run(index, GURL());
  }
}

bool ReplacePrefix(std::string* s,
                   const std::string& prefix,
                   const std::string& replacement) {
  if (base::StartsWith(*s, prefix, base::CompareCase::SENSITIVE)) {
    base::ReplaceFirstSubstringAfterOffset(s, 0, prefix, replacement);
    return true;
  }
  return false;
}

std::string GetPathDisplayTextForSettings(Profile* profile,
                                          const std::string& path) {
  std::string result(path);
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (drive_integration_service && !drive_integration_service->is_enabled()) {
    drive_integration_service = nullptr;
  }
  if (ReplacePrefix(&result, "/home/chronos/user/Downloads",
                    kFolderNameDownloads)) {
  } else if (ReplacePrefix(&result,
                           "/home/chronos/" + profile->GetBaseName().value() +
                               "/Downloads",
                           kFolderNameDownloads)) {
  } else if (ReplacePrefix(
                 &result,
                 std::string("/home/chronos/user/") + kFolderNameMyFiles,
                 "My files")) {
  } else if (ReplacePrefix(&result,
                           "/home/chronos/" + profile->GetBaseName().value() +
                               "/" + kFolderNameMyFiles,
                           "My files")) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirRoot)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL))
                               .value())) {
  } else if (ReplacePrefix(&result,
                           download_dir_util::kDriveNamePolicyVariableName,
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL))
                               .value())) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirTeamDrives)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_SHARED_DRIVES_LABEL))
                               .value())) {
  } else if (drive_integration_service &&
             ReplacePrefix(&result,
                           drive_integration_service->GetMountPointPath()
                               .Append(kDriveFsDirComputers)
                               .value(),
                           base::FilePath(kDisplayNameGoogleDrive)
                               .Append(l10n_util::GetStringUTF8(
                                   IDS_FILE_BROWSER_DRIVE_COMPUTERS_LABEL))
                               .value())) {
  } else if (
      drive_integration_service &&
      ReplacePrefix(
          &result,
          drive_integration_service->GetMountPointPath()
              .Append(kDriveFsDirSharedWithMe)
              .value(),
          base::FilePath(kDisplayNameGoogleDrive)
              .Append(l10n_util::GetStringUTF8(
                  IDS_FILE_BROWSER_DRIVE_SHARED_WITH_ME_COLLECTION_LABEL))
              .value())) {
  } else if (
      drive_integration_service &&
      ReplacePrefix(
          &result,
          drive_integration_service->GetMountPointPath()
              .Append(kDriveFsDirShortcutsSharedWithMe)
              .value(),
          base::FilePath(kDisplayNameGoogleDrive)
              .Append(l10n_util::GetStringUTF8(
                  IDS_FILE_BROWSER_DRIVE_SHARED_WITH_ME_COLLECTION_LABEL))
              .value())) {
  } else if (ReplacePrefix(&result, GetAndroidFilesPath().value(),
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL))) {
  } else if (ReplacePrefix(&result, GetCrostiniMountDirectory(profile).value(),
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL))) {
  } else if (ReplacePrefix(&result,
                           base::FilePath(kRemovableMediaPath)
                               .AsEndingWithSeparator()
                               .value(),
                           "")) {
    // Strip prefix of "/media/removable/" including trailing slash.
  } else if (ReplacePrefix(&result,
                           base::FilePath(kArchiveMountPath)
                               .AsEndingWithSeparator()
                               .value(),
                           "")) {
    // Strip prefix of "/media/archive/" including trailing slash.
  }

  base::ReplaceChars(result, "/", " \u203a ", &result);
  return result;
}

bool ExtractMountNameFileSystemNameFullPath(const base::FilePath& absolute_path,
                                            std::string* mount_name,
                                            std::string* file_system_name,
                                            std::string* full_path) {
  DCHECK(absolute_path.IsAbsolute());
  DCHECK(mount_name);
  DCHECK(full_path);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  if (!mount_points->GetVirtualPath(absolute_path, &virtual_path)) {
    return false;
  }
  // |virtual_path| format is: <mount_name>/<full_path>, and
  // |file_system_name| == |mount_name|, except for 'removable' and 'archive',
  // |mount_name| is the first two segments, |file_system_name| is the second.
  const std::string& value = virtual_path.value();
  size_t fs_start = 0;
  size_t slash_pos = value.find(base::FilePath::kSeparators[0]);
  *mount_name = *file_system_name = value.substr(0, slash_pos);
  if (*mount_name == ash::kSystemMountNameRemovable ||
      *mount_name == ash::kSystemMountNameArchive) {
    if (slash_pos == std::string::npos) {
      return false;
    }
    fs_start = slash_pos + 1;
    slash_pos = value.find(base::FilePath::kSeparators[0], fs_start);
    *mount_name = value.substr(0, slash_pos);
  }

  // Set full_path to '/' if |absolute_path| is a root.
  if (slash_pos == std::string::npos) {
    *file_system_name = value.substr(fs_start);
    *full_path = "/";
  } else {
    *file_system_name = value.substr(fs_start, slash_pos - fs_start);
    *full_path = value.substr(slash_pos);
  }
  return true;
}

std::string GetDisplayableFileName(GURL file_url) {
  // Try to convert %20 to spaces, if this produces any invalid char, use the
  // file name URL encoded.
  std::string file_name;
  if (!base::UnescapeBinaryURLComponentSafe(file_url.ExtractFileName(),
                                            /*fail_on_path_separators=*/true,
                                            &file_name)) {
    file_name = file_url.ExtractFileName();
  }

  return file_name;
}

std::string GetDisplayableFileName(storage::FileSystemURL file_url) {
  return GetDisplayableFileName(file_url.ToGURL());
}

std::u16string GetDisplayableFileName16(GURL file_url) {
  return base::UTF8ToUTF16(GetDisplayableFileName(file_url));
}

std::u16string GetDisplayableFileName16(storage::FileSystemURL file_url) {
  return base::UTF8ToUTF16(GetDisplayableFileName(file_url.ToGURL()));
}

absl::optional<base::FilePath> GetDisplayablePath(Profile* profile,
                                                  base::FilePath path) {
  base::WeakPtr<Volume> volume =
      file_manager::VolumeManager::Get(profile)->FindVolumeFromPath(path);
  if (!volume) {
    return absl::nullopt;
  }

  base::FilePath mount_relative_path;
  // AppendRelativePath fails if |mount_path| is the same as |path|, but in that
  // case |mount_relative_path| will be empty, which is what we want.
  volume->mount_path().AppendRelativePath(path, &mount_relative_path);
  auto path_components = mount_relative_path.GetComponents();

  auto cur_component = path_components.begin();
  base::FilePath result;
  switch (volume->type()) {
    case VOLUME_TYPE_GOOGLE_DRIVE: {
      // Start with the Google Drive root.
      result = base::FilePath(volume->volume_label());

      // The first directory indicates which Drive the path is in, so check it
      // against the expected directories. e.g. My Drive, Shared with me, etc.
      if (cur_component == path_components.end()) {
        return absl::nullopt;
      }
      auto maybe_id = DriveFsFolderToMessageId(*cur_component);
      if (!maybe_id.has_value()) {
        return absl::nullopt;
      }
      result = result.Append(l10n_util::GetStringUTF8(*maybe_id));
      cur_component++;

      // Skip the first directory in the Shared With Me folders as those are
      // just an opaque id.
      if (cur_component != path_components.end() &&
          (path_components[0] == kDriveFsDirSharedWithMe ||
           path_components[0] == kDriveFsDirShortcutsSharedWithMe)) {
        ++cur_component;
      }
      break;
    }
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      // Start with My Files root.
      result = base::FilePath(volume->volume_label());

      // Handle special folders under My Files.
      if (cur_component != path_components.end()) {
        auto maybe_id = MyFilesFolderToMessageId(*cur_component);
        if (maybe_id.has_value()) {
          result = result.Append(l10n_util::GetStringUTF8(*maybe_id));
          ++cur_component;
        }
      }
      break;
    case VOLUME_TYPE_ANDROID_FILES:
    case VOLUME_TYPE_CROSTINI:
    case VOLUME_TYPE_GUEST_OS:
      result = base::FilePath(l10n_util::GetStringUTF8(
                                  IDS_FILE_BROWSER_MY_FILES_ROOT_LABEL))
                   .Append(volume->volume_label());
      break;
    case VOLUME_TYPE_MEDIA_VIEW:
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
    case VOLUME_TYPE_PROVIDED:
    case VOLUME_TYPE_DOCUMENTS_PROVIDER:
    case VOLUME_TYPE_MTP:
    case VOLUME_TYPE_SMB:
      result = base::FilePath(volume->volume_label());
      break;
    case VOLUME_TYPE_TESTING:
    case VOLUME_TYPE_SYSTEM_INTERNAL:
      return absl::nullopt;
    case NUM_VOLUME_TYPE:
      NOTREACHED();
      return absl::nullopt;
  }
  while (cur_component != path_components.end()) {
    result = result.Append(*cur_component);
    cur_component++;
  }
  return result;
}

absl::optional<base::FilePath> GetDisplayablePath(
    Profile* profile,
    storage::FileSystemURL file_url) {
  return GetDisplayablePath(profile, file_url.path());
}

}  // namespace util
}  // namespace file_manager
