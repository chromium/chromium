// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/path_util.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using base::FilePath;
using storage::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

class FileManagerPathUtilTest : public testing::Test {
 public:
  FileManagerPathUtilTest() = default;

  FileManagerPathUtilTest(const FileManagerPathUtilTest&) = delete;
  FileManagerPathUtilTest& operator=(const FileManagerPathUtilTest&) = delete;

  ~FileManagerPathUtilTest() override = default;

  void SetUp() override {
    ash::disks::DiskMountManager::InitializeForTesting(
        new FakeDiskMountManager);
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath("/home/chronos/u-0123456789abcdef"));
    VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(std::make_unique<VolumeManager>(
              Profile::FromBrowserContext(context), nullptr, nullptr,
              ash::disks::DiskMountManager::GetInstance(), nullptr,
              VolumeManager::GetMtpStorageInfoCallback()));
        }));
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    profile_.reset();
    ash::disks::DiskMountManager::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FileManagerPathUtilTest, GetDownloadsFolderForProfile) {
  std::string mount_point_name = GetDownloadsMountPointName(profile_.get());
  EXPECT_EQ("Downloads", mount_point_name);
}

TEST_F(FileManagerPathUtilTest, GetMyFilesFolderForProfile) {
  base::FilePath profile_path =
      base::FilePath("/home/chronos/u-0123456789abcdef");

  // When running outside ChromeOS, it should return $HOME/Downloads for both
  // MyFiles and Downloads.
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetMyFilesFolderForProfile(profile_.get()));
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetDownloadsFolderForProfile(profile_.get()));

  // When running inside ChromeOS, it should return /home/u-{hash}/MyFiles.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
            GetMyFilesFolderForProfile(profile_.get()).value());
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles/Downloads",
            GetDownloadsFolderForProfile(profile_.get()).value());

  // Mount the volume to test the return from mount_points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      GetDownloadsMountPointName(profile_.get()), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), profile_path.Append("MyFiles"));

  // When returning from the mount_point Downloads should still point to
  // MyFiles/Downloads.
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles/Downloads",
            GetDownloadsFolderForProfile(profile_.get()).value());

  // Still the same: /home/u-{hash}/MyFiles.
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
            GetMyFilesFolderForProfile(profile_.get()).value());
}

TEST_F(FileManagerPathUtilTest, GetPathDisplayTextForSettings) {
  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             profile_.get(), "/home/chronos/user/Downloads"));
  EXPECT_EQ("Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/u-0123456789abcdef/Downloads"));

  EXPECT_EQ("My files \u203a Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/Downloads"));
  EXPECT_EQ("My files \u203a Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/Downloads"));

  EXPECT_EQ("My files \u203a other-folder",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/other-folder"));
  EXPECT_EQ("My files \u203a other-folder",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/other-folder"));

  EXPECT_EQ("Play files \u203a foo \u203a bar",
            GetPathDisplayTextForSettings(
                profile_.get(), "/run/arc/sdcard/write/emulated/0/foo/bar"));
  EXPECT_EQ("Linux files \u203a foo",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/media/fuse/crostini_0123456789abcdef_termina_penguin/foo"));
  EXPECT_EQ("foo", GetPathDisplayTextForSettings(profile_.get(),
                                                 "/media/removable/foo"));
  EXPECT_EQ("foo", GetPathDisplayTextForSettings(profile_.get(),
                                                 "/media/archive/foo"));

  TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
  ash::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  PrefService* prefs = profile2.GetPrefs();
  prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);
  EXPECT_EQ(
      "Google Drive \u203a My Drive \u203a foo",
      GetPathDisplayTextForSettings(
          &profile2,
          "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root/foo"));
  EXPECT_EQ("Google Drive \u203a Shared drives \u203a A Team Drive \u203a foo",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "team_drives/A Team Drive/foo"));
  EXPECT_EQ("Google Drive \u203a Computers \u203a My Other Computer \u203a bar",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "Computers/My Other Computer/bar"));
  EXPECT_EQ("Google Drive \u203a Shared with me \u203a 1234 \u203a shared",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                ".files-by-id/1234/shared"));
  EXPECT_EQ(
      "Google Drive \u203a Shared with me \u203a 1-abc-xyz \u203a shortcut",
      GetPathDisplayTextForSettings(
          &profile2,
          "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
          ".shortcut-targets-by-id/1-abc-xyz/shortcut"));

  EXPECT_EQ("Google Drive \u203a My Drive \u203a foo",
            GetPathDisplayTextForSettings(&profile2, "${google_drive}/foo"));

  TestingProfile guest_profile(base::FilePath("/home/chronos/guest"));
  guest_profile.SetGuestSession(true);
  guest_profile.set_profile_name("$guest");
  ASSERT_TRUE(
      drive::DriveIntegrationServiceFactory::GetForProfile(&guest_profile));

  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             &guest_profile, "/home/chronos/user/Downloads"));
  // Test that a passthrough path doesn't crash on requesting the Drive mount
  // path for a guest profile.
  EXPECT_EQ("foo", GetPathDisplayTextForSettings(&guest_profile, "foo"));
}

TEST_F(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  // MigratePathFromOldFormat is explicitly disabled on Linux build.
  // So we need to fake that this is real ChromeOS system.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;

  // /home/chronos/u-${HASH}/MyFiles/Downloads
  const FilePath kDownloadsFolder =
      GetDownloadsFolderForProfile(profile_.get());
  // /home/chronos/u-${HASH}/MyFiles/
  const FilePath kMyFilesFolder = GetMyFilesFolderForProfile(profile_.get());
  // In the device: /home/chronos/user
  // In browser tests: /tmp/.org.chromium.Chromium.F0Ejp5
  const FilePath old_base = DownloadPrefs::GetDefaultDownloadDirectory();

  FilePath path;

  // Special case to convert the base pkth directly to MyFiles/Downloads,
  // because DownloadPrefs is initially initialized to /home/chronos/user before
  // we have the Profile fully set up and we want to set it to MyFiles/Downloads
  // which is the default download folder for new users.
  EXPECT_TRUE(MigratePathFromOldFormat(profile_.get(),
                                       FilePath("/home/chronos/user"),
                                       FilePath("/home/chronos/user"), &path));
  EXPECT_EQ(kDownloadsFolder, path);

  EXPECT_TRUE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/user"),
                               FilePath("/home/chronos/user/a/b"), &path));
  EXPECT_EQ(kMyFilesFolder.AppendASCII("a/b"), path);
  EXPECT_TRUE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/u-1234"),
                               FilePath("/home/chronos/u-1234/a/b"), &path));
  EXPECT_EQ(kMyFilesFolder.AppendASCII("a/b"), path);

  // Path already in the new format is not converted, it's already inside
  // MyFiles or MyFiles/Downloads.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      kMyFilesFolder.AppendASCII("a/b"), &path));
  EXPECT_FALSE(MigratePathFromOldFormat(profile_.get(), kMyFilesFolder,
                                        kMyFilesFolder.AppendASCII("a/b"),
                                        &path));
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      kDownloadsFolder.AppendASCII("a/b"), &path));
  EXPECT_FALSE(MigratePathFromOldFormat(profile_.get(), kMyFilesFolder,
                                        kDownloadsFolder.AppendASCII("a/b"),
                                        &path));

  // Only /home/chronos/user is migrated when old_base == old_path.
  EXPECT_FALSE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/u-1234"),
                               FilePath("/home/chronos/u-1234"), &path));
  // Won't migrate because old_path isn't inside the default downloads
  // directory.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      FilePath::FromUTF8Unsafe("/home/chronos/user/dl"), &path));

  // Won't migrate because old_path is already migrated.
  EXPECT_FALSE(
      MigratePathFromOldFormat(profile_.get(), kMyFilesFolder.DirName(),
                               kMyFilesFolder.AppendASCII("a/b"), &path));
}

TEST_F(FileManagerPathUtilTest, MigrateToDriveFs) {
  base::FilePath home("/home/chronos/u-0123456789abcdef");
  base::FilePath other("/some/other/path");
  base::FilePath old_drive("/special/drive-0123456789abcdef");
  base::FilePath my_drive = old_drive.Append("root");
  base::FilePath file_in_my_drive = old_drive.Append("root").Append("file.txt");

  // Migrate paths under old drive mount.
  TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
  ash::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  PrefService* prefs = profile2.GetPrefs();
  prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");
  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);

  base::FilePath result;
  EXPECT_FALSE(MigrateToDriveFs(&profile2, other, &result));
  EXPECT_TRUE(MigrateToDriveFs(&profile2, my_drive, &result));
  EXPECT_EQ(base::FilePath(
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root"),
            result);
  EXPECT_TRUE(MigrateToDriveFs(&profile2, file_in_my_drive, &result));
  EXPECT_EQ(
      base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                     "root/file.txt"),
      result);
}

TEST_F(FileManagerPathUtilTest, ConvertBetweenFileSystemURLAndPathInsideVM) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  // Setup for DriveFS.
  ash::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), "12345"));
  profile_->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  // Initialize D-Bus clients.
  ash::ChunneldClient::InitializeFake();
  ash::CiceroneClient::InitializeFake();
  ash::ConciergeClient::InitializeFake();
  ash::SeneschalClient::InitializeFake();

  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile_.get());
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
  crostini_manager->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/home/testuser", "PLACEHOLDER_IP"));

  // Register crostini, my files, drive, android.
  mount_points->RegisterFileSystem(GetCrostiniMountPointName(profile_.get()),
                                   storage::kFileSystemTypeLocal,
                                   storage::FileSystemMountOption(),
                                   GetCrostiniMountDirectory(profile_.get()));
  mount_points->RegisterFileSystem(GetDownloadsMountPointName(profile_.get()),
                                   storage::kFileSystemTypeLocal,
                                   storage::FileSystemMountOption(),
                                   GetMyFilesFolderForProfile(profile_.get()));
  mount_points->RegisterFileSystem(
      GetAndroidFilesMountPointName(), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath(kAndroidFilesPath));
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());
  base::FilePath mount_point_drive = integration_service->GetMountPointPath();
  mount_points->RegisterFileSystem(
      mount_point_drive.BaseName().value(), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), mount_point_drive);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath(kRemovableMediaPath));
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath(kArchiveMountPath));
  // SmbFsShare comes up with a unique stable ID for the share, it can
  // just be faked here, the mount ID is expected to appear in the mount
  // path, it's faked too.
  mount_points->RegisterFileSystem(
      "smbfsmountid" /* fake mount ID for mount name */,
      storage::kFileSystemTypeSmbFs, storage::FileSystemMountOption(),
      base::FilePath("/media/fuse/smbfs-smbfsmountid"));

  base::FilePath inside;
  storage::FileSystemURL url;
  base::FilePath vm_mount("/mnt/chromeos");

  struct Test {
    std::string mount_name;
    std::string relative_path;
    std::string inside;
  };

  Test tests[] = {
      {
          "Downloads-testing_profile%40test-hash",
          "path/in/myfiles",
          "/mnt/chromeos/MyFiles/path/in/myfiles",
      },
      {
          "crostini_0123456789abcdef_termina_penguin",
          "path/in/crostini",
          "/mnt/chromeos/LinuxFiles/path/in/crostini",
      },
      {
          "android_files",
          "path/in/android",
          "/mnt/chromeos/PlayFiles/path/in/android",
      },
      {
          "drivefs-84675c855b63e12f384d45f033826980",
          "root/path/in/mydrive",
          "/mnt/chromeos/GoogleDrive/MyDrive/path/in/mydrive",
      },
      {
          "drivefs-84675c855b63e12f384d45f033826980",
          "team_drives/path/in/teamdrives",
          "/mnt/chromeos/GoogleDrive/SharedDrives/path/in/teamdrives",
      },
      {
          "drivefs-84675c855b63e12f384d45f033826980",
          "Computers/path/in/computers",
          "/mnt/chromeos/GoogleDrive/Computers/path/in/computers",
      },
      {
          "drivefs-84675c855b63e12f384d45f033826980",
          ".files-by-id/1234/shared",
          "/mnt/chromeos/GoogleDrive/SharedWithMe/1234/shared",
      },
      {
          "removable",
          "MyUSB/path/in/removable",
          "/mnt/chromeos/removable/MyUSB/path/in/removable",
      },
      {
          "archive",
          "file.rar/path/in/archive",
          "/mnt/chromeos/archive/file.rar/path/in/archive",
      },
      {
          "smbfsmountid",
          "path/in/smb",
          "/mnt/chromeos/SMB/smbfsmountid/path/in/smb",
      },
  };

  for (const auto& test : tests) {
    // ConvertFileSystemURLToPathInsideVM.
    EXPECT_TRUE(ConvertFileSystemURLToPathInsideVM(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            blink::StorageKey(), test.mount_name,
            base::FilePath(test.relative_path)),
        vm_mount, /*map_crostini_home=*/false, &inside));
    EXPECT_EQ(test.inside, inside.value());

    // ConvertPathInsideVMToFileSystemURL.
    EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
        profile_.get(), base::FilePath(test.inside), vm_mount,
        /*map_crostini_home=*/false, &url));
    EXPECT_TRUE(url.is_valid());
    EXPECT_EQ(file_manager::util::GetFileManagerURL(), url.origin().GetURL());
    EXPECT_EQ(test.mount_name, url.filesystem_id());
    EXPECT_EQ(test.mount_name + "/" + test.relative_path,
              url.virtual_path().value());
  }

  // Special case for Crostini $HOME ConvertFileSystemURLToPathInsideVM.
  EXPECT_TRUE(ConvertFileSystemURLToPathInsideVM(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(
          blink::StorageKey(), "crostini_0123456789abcdef_termina_penguin",
          base::FilePath("path/in/crostini")),
      vm_mount, /*map_crostini_home=*/true, &inside));
  EXPECT_EQ("/home/testuser/path/in/crostini", inside.value());
  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(
          blink::StorageKey(), "crostini_0123456789abcdef_termina_penguin",
          base::FilePath("path/in/crostini")),
      &inside));
  EXPECT_EQ("/home/testuser/path/in/crostini", inside.value());

  EXPECT_FALSE(ConvertFileSystemURLToPathInsideVM(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(
          blink::StorageKey(), "unknown", base::FilePath("path/in/unknown")),
      vm_mount, /*map_crostini_home=*/false, &inside));

  // Special case for Crostini $HOME ConvertPathInsideVMToFileSystemURL.
  EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), base::FilePath("/home/testuser/path/in/crostini"),
      vm_mount, /*map_crostini_home=*/true, &url));
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(file_manager::util::GetFileManagerURL(), url.origin().GetURL());
  EXPECT_EQ("crostini_0123456789abcdef_termina_penguin/path/in/crostini",
            url.virtual_path().value());
  EXPECT_FALSE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), base::FilePath("/home/testuser/path/in/crostini"),
      vm_mount, /*map_crostini_home=*/false, &url));

  EXPECT_FALSE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), base::FilePath("/path/not/under/mount"), vm_mount,
      /*map_crostini_home=*/false, &url));

  // Special case for PluginVM case-insensitive hostname matching.
  EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), base::FilePath("//chromeos/MyFiles/path/in/pluginvm"),
      base::FilePath("//ChromeOS"), /*map_crostini_home=*/false, &url));
  EXPECT_EQ("Downloads-testing_profile%40test-hash/path/in/pluginvm",
            url.virtual_path().value());

  profile_.reset();
  ash::SeneschalClient::Shutdown();
  ash::ConciergeClient::Shutdown();
  ash::ChunneldClient::Shutdown();
}

TEST_F(FileManagerPathUtilTest, ExtractMountNameFileSystemNameFullPath) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  std::string downloads_mount_name = GetDownloadsMountPointName(profile_.get());
  base::FilePath downloads = GetDownloadsFolderForProfile(profile_.get());
  mount_points->RegisterFileSystem(downloads_mount_name,
                                   storage::kFileSystemTypeLocal,
                                   storage::FileSystemMountOption(), downloads);
  base::FilePath removable = base::FilePath(kRemovableMediaPath);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath(kRemovableMediaPath));
  base::FilePath archive = base::FilePath(kArchiveMountPath);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath(kArchiveMountPath));
  std::string relative_path_1 = "foo";
  std::string relative_path_2 = "foo/bar";
  std::string mount_name;
  std::string file_system_name;
  std::string full_path;

  // <Downloads>/
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads, &mount_name, &file_system_name, &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/");

  // <Downloads>/foo
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads.Append(relative_path_1), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/foo");

  // <Downloads>/foo/bar
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads.Append(relative_path_2), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/foo/bar");

  // <removable>/
  EXPECT_FALSE(ExtractMountNameFileSystemNameFullPath(
      removable, &mount_name, &file_system_name, &full_path));

  // <removable>/foo/
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      removable.Append(relative_path_1), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, "removable/foo");
  EXPECT_EQ(file_system_name, "foo");
  EXPECT_EQ(full_path, "/");

  // <removable>/foo/bar
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      removable.Append(relative_path_2), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, "removable/foo");
  EXPECT_EQ(file_system_name, "foo");
  EXPECT_EQ(full_path, "/bar");

  // <archive>/foo/bar
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      archive.Append(relative_path_2), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, "archive/foo");
  EXPECT_EQ(file_system_name, "foo");
  EXPECT_EQ(full_path, "/bar");

  // Unknown.
  EXPECT_FALSE(ExtractMountNameFileSystemNameFullPath(
      base::FilePath("/unknown/path"), &mount_name, &file_system_name,
      &full_path));
}

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

class FileManagerPathUtilConvertUrlTest : public testing::Test {
 public:
  FileManagerPathUtilConvertUrlTest() = default;

  FileManagerPathUtilConvertUrlTest(const FileManagerPathUtilConvertUrlTest&) =
      delete;
  FileManagerPathUtilConvertUrlTest& operator=(
      const FileManagerPathUtilConvertUrlTest&) = delete;

  ~FileManagerPathUtilConvertUrlTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up fake user manager.
    auto* fake_user_manager = new ash::FakeChromeUserManager();
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
    const AccountId account_id_2(
        AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222"));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
    fake_user_manager->AddUser(account_id_2);
    fake_user_manager->LoginUser(account_id_2);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(std::move(fake_user_manager)));

    primary_profile_ =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    ASSERT_TRUE(primary_profile_);
    secondary_profile_ =
        profile_manager_->CreateTestingProfile(account_id_2.GetUserEmail());
    ASSERT_TRUE(secondary_profile_);
    primary_profile_->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt,
                                            "a");
    primary_profile_->GetPrefs()->SetBoolean(
        drive::prefs::kDriveFsPinnedMigrated, true);

    // Set up an Arc service manager with a fake file system.
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_service_manager_->set_browser_context(primary_profile_);
    arc::ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        primary_profile_,
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    ASSERT_TRUE(fake_file_system_.InitCalled());

    // Add a drive mount point for the primary profile.
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(primary_profile_);
    drive_mount_point_ = integration_service->GetMountPointPath();
    integration_service->OnMounted(drive_mount_point_);

    // Add a crostini mount point for the primary profile.
    crostini_mount_point_ = GetCrostiniMountDirectory(primary_profile_);
    mount_points->RegisterFileSystem(
        GetCrostiniMountPointName(primary_profile_),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        crostini_mount_point_);

    ash::disks::DiskMountManager::InitializeForTesting(
        new FakeDiskMountManager);

    // Add the disk and mount point for a fake removable device.
    ASSERT_TRUE(ash::disks::DiskMountManager::GetInstance()->AddDiskForTest(
        ash::disks::Disk::Builder()
            .SetDevicePath("/device/source_path")
            .SetFileSystemUUID("0123-abcd")
            .Build()));
    ASSERT_TRUE(
        ash::disks::DiskMountManager::GetInstance()->AddMountPointForTest(
            {"/device/source_path", "/media/removable/a",
             ash::MountType::kDevice}));

    // Add a Share Cache mount point for the primary profile.
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        kShareCacheMountPointName, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(),
        util::GetShareCacheFilePath(primary_profile_)));

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    user_manager_enabler_.reset();
    profile_manager_.reset();

    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();

    ash::disks::DiskMountManager::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  arc::FakeFileSystemInstance fake_file_system_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, ExperimentalAsh> primary_profile_;
  raw_ptr<TestingProfile, ExperimentalAsh> secondary_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  base::FilePath drive_mount_point_;
  base::FilePath crostini_mount_point_;
};

FileSystemURL CreateExternalURL(const base::FilePath& path) {
  return FileSystemURL::CreateForTest(blink::StorageKey(),
                                      storage::kFileSystemTypeExternal, path);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Archive) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"", "--enable-arcvm"});
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/archive/Smile 🙂.zip/"
                                     "Folder ({[<!@#$%^&*_-+=`~;:'\"?>\\]})/"
                                     ".File.txt"),
      &url, &requires_sharing));
  EXPECT_EQ(
      GURL("content://org.chromium.arc.volumeprovider/archive/"
           "Smile%20%F0%9F%99%82.zip/"
           "Folder%20(%7B%5B%3C!@%23$%25%5E&*_-+=%60~;%3A'%22%3F%3E%5C%5D%7D)/"
           ".File.txt"),
      url);
  EXPECT_TRUE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Removable) {
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"), &url,
      &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
            url);
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_MyFiles) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  GURL url;
  bool requires_sharing = false;
  const base::FilePath myfiles = GetMyFilesFolderForProfile(primary_profile_);
  EXPECT_TRUE(ConvertPathToArcUrl(myfiles.AppendASCII("a/b/c"), &url,
                                  &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "0000000000000000000000000000CAFEF00D2019/"
                 "a/b/c"),
            url);
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidRemovable) {
  GURL url;
  bool requires_sharing = false;
  EXPECT_FALSE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable_foobar"), &url,
      &requires_sharing));
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidDownloads) {
  // Non-primary profile's downloads folder is not supported for ARC yet.
  GURL url;
  bool requires_sharing = false;
  const base::FilePath downloads2 =
      GetDownloadsFolderForProfile(secondary_profile_);
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url,
                                   &requires_sharing));
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Crostini) {
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(crostini_mount_point_.AppendASCII("a/b/c"),
                                  &url, &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3A"
                 "crostini_user%40gmail.com-hash_termina_penguin%2Fa%2Fb%2Fc"),
            url);
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_MyDriveLegacy) {
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(drive_mount_point_.AppendASCII("a/b/c"), &url,
                                  &requires_sharing));
  // "@" appears escaped 3 times here because escaping happens when:
  // - creating drive mount point name for user
  // - creating externalfile: URL from the path
  // - encoding the URL to Chrome content provider URL
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5c569%2Fa"
                 "%2Fb%2Fc"),
            url);
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_MyDriveArcvm) {
  ash::CiceroneClient::InitializeFake();
  ash::ConciergeClient::InitializeFake();
  ash::SeneschalClient::InitializeFake();

  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(drive_mount_point_.AppendASCII("a/b/c"), &url,
                                  &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "MyDrive/a/b/c"),
            url);
  EXPECT_TRUE(requires_sharing);

  ash::SeneschalClient::Shutdown();
  ash::ConciergeClient::Shutdown();
  ash::CiceroneClient::Shutdown();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_ShareCache) {
  GURL url;
  bool requires_seneschal_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      util::GetShareCacheFilePath(ProfileManager::GetPrimaryUserProfile())
          .AppendASCII("a/b/c"),
      &url, &requires_seneschal_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3AShareCache%2Fa%2Fb%2Fc"),
            url);
  // ShareCache files do not need to be shared to ARC through Seneschal.
  EXPECT_FALSE(requires_seneschal_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidMountType) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Removable) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
                urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MyFiles) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  const base::FilePath myfiles = GetMyFilesFolderForProfile(primary_profile_);
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(myfiles.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "0000000000000000000000000000CAFEF00D2019/"
                           "a/b/c"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidRemovable) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable_foobar"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Downloads) {
  const base::FilePath downloads =
      GetDownloadsFolderForProfile(primary_profile_);
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "download/a/b/c"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidDownloads) {
  const base::FilePath downloads =
      GetDownloadsFolderForProfile(secondary_profile_);
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Special) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5"
                           "c569%2Fa%2Fb%2Fc"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_ArcDocumentsProvider) {
  // Add images_root/Download/photo.jpg to the fake file system.
  const char kAuthority[] = "com.android.providers.media.documents";
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "images_root", "", "", arc::kAndroidDirectoryMimeType, -1,
      0));
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "dir-id", "images_root", "Download",
      arc::kAndroidDirectoryMimeType, -1, 22));
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "photo-id", "dir-id", "photo.jpg", "image/jpeg", 3, 33));

  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://com.android.providers.media.documents/"
                           "document/photo-id"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_ArcDocumentsProviderFileNotFound) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(""), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_AndroidFiles) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "external_files/Pictures/a/b.jpg"),
                      urls[0]);
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/read/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_AndroidFiles_GuestOs) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/media/fuse/android_files/Pictures/a/b.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "external_files/Pictures/a/b.jpg"),
                      urls[0]);
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles_GuestOs) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MultipleUrls) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe("/invalid")),
          CreateExternalURL(
              base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c")),
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c")),
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls,
             const std::vector<base::FilePath>& paths_to_share) {
            run_loop->Quit();
            ASSERT_EQ(4U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
            EXPECT_EQ(
                GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
                urls[1]);
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5"
                           "c569%2Fa%2Fb%2Fc"),
                      urls[2]);
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "external_files/a/b/c"),
                      urls[3]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilTest, GetDisplayablePathTest) {
  auto* volume_manager = VolumeManager::Get(profile_.get());
  volume_manager->RegisterDownloadsDirectoryForTesting(
      base::FilePath("/mount_path/my_files"));
  volume_manager->RegisterCrostiniDirectoryForTesting(
      base::FilePath("/mount_path/crostini"));
  volume_manager->RegisterAndroidFilesDirectoryForTesting(
      base::FilePath("/mount_path/android"));

  volume_manager->RegisterMediaViewForTesting(arc::kAudioRootDocumentId);
  volume_manager->RegisterMediaViewForTesting(arc::kImagesRootDocumentId);
  volume_manager->RegisterMediaViewForTesting(arc::kVideosRootDocumentId);
  volume_manager->RegisterMediaViewForTesting(arc::kDocumentsRootDocumentId);

  volume_manager->AddVolumeForTesting(
      Volume::CreateForDrive(base::FilePath("/mount_path/drive")));

  auto removable_disk =
      ash::disks::Disk::Builder().SetDeviceLabel("removable_label").Build();
  volume_manager->AddVolumeForTesting(Volume::CreateForRemovable(
      {"/source_path/removable", "/mount_path/removable",
       ash::MountType::kDevice, ash::MountError::kSuccess},
      removable_disk.get()));

  // The source path for archives need to be inside an already mounted volume,
  // so add it under the My Files volume.
  volume_manager->AddVolumeForTesting(Volume::CreateForRemovable(
      {"/mount_path/my_files/archive", "/mount_path/archive.zip",
       ash::MountType::kArchive, ash::MountError::kSuccess},
      nullptr));

  volume_manager->AddVolumeForTesting(Volume::CreateForProvidedFileSystem(
      {ash::file_system_provider::ProviderId::FromString("provider_id"),
       {"file_system_id", "provided_label"},
       base::FilePath("/mount_path/provided"),
       false,
       false,
       extensions::SOURCE_FILE,
       {}},
      MOUNT_CONTEXT_USER));

  volume_manager->AddVolumeForTesting(Volume::CreateForDocumentsProvider(
      "authority", "root_id", "document_id", "documents_provider_label",
      "summary", {}, false, /*optional_fusebox_subdir=*/std::string()));

  volume_manager->AddVolumeForTesting(Volume::CreateForSftpGuestOs(
      "guest_os_label", base::FilePath("/mount_path/guest_os"),
      base::FilePath("/remote_mount_path/guest_os"),
      guest_os::VmType::TERMINA));

  volume_manager->AddVolumeForTesting(Volume::CreateForMTP(
      base::FilePath("/mount_path/mtp"), "mtp_label", false));

  volume_manager->AddVolumeForTesting(
      Volume::CreateForSmb(base::FilePath("/mount_path/smb"), "smb_label"));

  volume_manager->AddVolumeForTesting(
      Volume::CreateForShareCache(base::FilePath("/mount_path/share_cache")));

  volume_manager->AddVolumeForTesting(
      base::FilePath("/mount_path/testing"), VOLUME_TYPE_TESTING,
      ash::DeviceType::kUnknown, false /* read_only */);

  struct Test {
    std::string path;
    std::string expected;
  };
  Test tests[] = {
      {
          "/mount_path/drive/root",
          "Google Drive/My Drive",
      },
      {
          "/mount_path/drive/root/foo",
          "Google Drive/My Drive/foo",
      },
      {
          "/mount_path/drive/team_drives/foo",
          "Google Drive/Shared drives/foo",
      },
      {
          "/mount_path/drive/Computers/foo",
          "Google Drive/Computers/foo",
      },
      {
          "/mount_path/drive/.files-by-id/id/foo",
          "Google Drive/Shared with me/foo",
      },
      {
          "/mount_path/drive/.shortcut-targets-by-id/id/foo",
          "Google Drive/Shared with me/foo",
      },
      {
          "/mount_path/my_files",
          "My files",
      },
      {
          "/mount_path/my_files/foo",
          "My files/foo",
      },
      {
          "/mount_path/my_files/Downloads/foo",
          "My files/Downloads/foo",
      },
      {
          "/mount_path/my_files/PvmDefault",
          "My files/Windows files",
      },
      {
          "/mount_path/my_files/Camera/foo",
          "My files/Camera/foo",
      },
      {
          "/mount_path/my_files/foo/bar",
          "My files/foo/bar",
      },
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kAudioRootDocumentId)
           .value(),
       "Audio"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kImagesRootDocumentId)
           .Append("foo")
           .value(),
       "Images/foo"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kVideosRootDocumentId)
           .Append("foo/bar")
           .value(),
       "Videos/foo/bar"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kDocumentsRootDocumentId)
           .Append("bar")
           .value(),
       "Documents/bar"},
      {
          "/mount_path/android",
          "My files/Play files",
      },
      {
          "/mount_path/crostini/foo",
          "My files/Linux files/foo",
      },
      {
          "/mount_path/guest_os/foo",
          "My files/guest_os_label/foo",
      },
      {
          "/mount_path/provided/foo",
          "provided_label/foo",
      },
      {
          "/mount_path/removable",
          "removable_label",
      },
      {
          "/mount_path/archive.zip/foo",
          "archive.zip/foo",
      },
      {
          "/mount_path/removable/foo",
          "removable_label/foo",
      },
      {
          arc::GetDocumentsProviderMountPath("authority", "document_id")
              .value(),
          "documents_provider_label",
      },
      {
          "/mount_path/mtp",
          "mtp_label",
      },
      {
          "/mount_path/smb/foo",
          "smb_label/foo",
      },
  };
  for (auto& test : tests) {
    EXPECT_EQ(base::FilePath(test.expected),
              *GetDisplayablePath(profile_.get(), base::FilePath(test.path)));
  }
  EXPECT_EQ(absl::nullopt,
            GetDisplayablePath(profile_.get(),
                               base::FilePath("/non_existent/mount")));
  EXPECT_EQ(absl::nullopt,
            GetDisplayablePath(profile_.get(),
                               base::FilePath("/mount_path/share_cache")));
  EXPECT_EQ(absl::nullopt,
            GetDisplayablePath(profile_.get(),
                               base::FilePath("/mount_path/testing")));
}

}  // namespace
}  // namespace util
}  // namespace file_manager
