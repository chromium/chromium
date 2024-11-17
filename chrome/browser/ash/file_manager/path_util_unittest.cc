// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/path_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
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
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace file_manager::util {
namespace {

using base::FilePath;
using base::test::TestFuture;
using storage::FileSystemURL;

class FileManagerPathUtilTest : public testing::Test {
 public:
  FileManagerPathUtilTest() = default;

  FileManagerPathUtilTest(const FileManagerPathUtilTest&) = delete;
  FileManagerPathUtilTest& operator=(const FileManagerPathUtilTest&) = delete;

  ~FileManagerPathUtilTest() override = default;

  void SetUp() override {
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);
    profile_ = std::make_unique<TestingProfile>(
        FilePath("/home/chronos/u-0123456789abcdef"));
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
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FileManagerPathUtilTest, GetDownloadsFolderForProfile) {
  std::string mount_point_name = GetDownloadsMountPointName(profile_.get());
  EXPECT_EQ("Downloads", mount_point_name);
}

TEST_F(FileManagerPathUtilTest, GetMyFilesFolderForProfile) {
  FilePath profile_path = FilePath("/home/chronos/u-0123456789abcdef");

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
  EXPECT_EQ("My files", GetPathDisplayTextForSettings(
                            profile_.get(), "/home/chronos/user/MyFiles"));

  EXPECT_EQ("My files",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/u-0123456789abcdef/MyFiles"));

  EXPECT_EQ("My files › a › b › c",
            GetPathDisplayTextForSettings(profile_.get(),
                                          "/home/chronos/user/MyFiles/a/b/c"));
  EXPECT_EQ(
      "My files › a › b › c",
      GetPathDisplayTextForSettings(
          profile_.get(), "/home/chronos/u-0123456789abcdef/MyFiles/a/b/c"));

  EXPECT_EQ("My files › Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/Downloads"));

  EXPECT_EQ("My files › Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/Downloads"));

  EXPECT_EQ("My files › Downloads › a › b › c",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/Downloads/a/b/c"));

  EXPECT_EQ("My files › Downloads › a › b › c",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/Downloads/a/b/c"));

  EXPECT_NE("My files2", GetPathDisplayTextForSettings(
                             profile_.get(), "/home/chronos/user/MyFiles2"));

  EXPECT_EQ("Play files",
            GetPathDisplayTextForSettings(profile_.get(),
                                          "/run/arc/sdcard/write/emulated/0"));

  EXPECT_EQ("Play files › a › b › c",
            GetPathDisplayTextForSettings(
                profile_.get(), "/run/arc/sdcard/write/emulated/0/a/b/c"));

  EXPECT_EQ("Linux files",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/media/fuse/crostini_0123456789abcdef_termina_penguin"));

  EXPECT_EQ("Linux files › a › b › c",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/media/fuse/crostini_0123456789abcdef_termina_penguin/a/b/c"));

  EXPECT_EQ("foo", GetPathDisplayTextForSettings(profile_.get(),
                                                 "/media/removable/foo"));

  EXPECT_EQ("foo › a › b › c",
            GetPathDisplayTextForSettings(profile_.get(),
                                          "/media/removable/foo/a/b/c"));

  EXPECT_EQ("foo", GetPathDisplayTextForSettings(profile_.get(),
                                                 "/media/archive/foo"));

  EXPECT_EQ("foo › a › b › c", GetPathDisplayTextForSettings(
                                   profile_.get(), "/media/archive/foo/a/b/c"));

  TestingProfile profile2(FilePath("/home/chronos/u-0123456789abcdef"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  profile2.GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);

  EXPECT_EQ("Google Drive › My Drive",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root"));

  EXPECT_EQ(
      "Google Drive › My Drive › foo",
      GetPathDisplayTextForSettings(
          &profile2,
          "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root/foo"));

  EXPECT_EQ("Google Drive › Shared drives › A Team Drive",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "team_drives/A Team Drive"));

  EXPECT_EQ("Google Drive › Shared drives › A Team Drive › foo",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "team_drives/A Team Drive/foo"));

  EXPECT_EQ("Google Drive › Computers › My Other Computer",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "Computers/My Other Computer"));

  EXPECT_EQ("Google Drive › Computers › My Other Computer › bar",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "Computers/My Other Computer/bar"));

  EXPECT_EQ("Google Drive › Shared with me › 1234 › shared",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                ".files-by-id/1234/shared"));

  EXPECT_EQ("Google Drive › Shared with me › 1-abc-xyz › shortcut",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                ".shortcut-targets-by-id/1-abc-xyz/shortcut"));

  EXPECT_EQ("Google Drive › My Drive › foo",
            GetPathDisplayTextForSettings(&profile2, "${google_drive}/foo"));

  TestingProfile guest_profile(FilePath("/home/chronos/guest"));
  guest_profile.SetGuestSession(true);
  guest_profile.set_profile_name("$guest");
  ASSERT_TRUE(
      drive::DriveIntegrationServiceFactory::GetForProfile(&guest_profile));

  EXPECT_EQ("My files", GetPathDisplayTextForSettings(
                            &guest_profile, "/home/chronos/user/MyFiles"));

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
  FilePath home("/home/chronos/u-0123456789abcdef");
  FilePath other("/some/other/path");
  FilePath old_drive("/special/drive-0123456789abcdef");
  FilePath my_drive = old_drive.Append("root");
  FilePath file_in_my_drive = old_drive.Append("root").Append("file.txt");

  // Migrate paths under old drive mount.
  TestingProfile profile2(FilePath("/home/chronos/u-0123456789abcdef"));
  user_manager_->AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  PrefService* prefs = profile2.GetPrefs();
  prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");
  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);

  FilePath result;
  EXPECT_FALSE(MigrateToDriveFs(&profile2, other, &result));
  EXPECT_TRUE(MigrateToDriveFs(&profile2, my_drive, &result));
  EXPECT_EQ(
      FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root"),
      result);
  EXPECT_TRUE(MigrateToDriveFs(&profile2, file_in_my_drive, &result));
  EXPECT_EQ(FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                     "root/file.txt"),
            result);
}

TEST_F(FileManagerPathUtilTest, GetGuestOsMountPointName) {
  guest_os::GuestId arcvm(guest_os::VmType::ARCVM, "arcvm", "");
  EXPECT_EQ(GetGuestOsMountPointName(profile_.get(), arcvm), "android_files");

  guest_os::GuestId penguin(guest_os::VmType::TERMINA, "termina", "penguin");
  EXPECT_EQ(GetGuestOsMountPointName(profile_.get(), penguin),
            "crostini_0123456789abcdef_termina_penguin");

  guest_os::GuestId other(guest_os::VmType::TERMINA, "termina", "other");
  EXPECT_EQ(GetGuestOsMountPointName(profile_.get(), other),
            "guestos+0123456789abcdef+termina+other");

  guest_os::GuestId bru(guest_os::VmType::BRUSCHETTA, "bru", "penguin");
  EXPECT_EQ(GetGuestOsMountPointName(profile_.get(), bru),
            "guestos+0123456789abcdef+bru+penguin");
}

TEST_F(FileManagerPathUtilTest, ConvertBetweenFileSystemURLAndPathInsideVM) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // Setup for DriveFS.
  user_manager_->AddUser(
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
      storage::FileSystemMountOption(), FilePath(kAndroidFilesPath));
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());
  FilePath mount_point_drive = integration_service->GetMountPointPath();
  mount_points->RegisterFileSystem(
      mount_point_drive.BaseName().value(), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), mount_point_drive);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), FilePath(kRemovableMediaPath));
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), FilePath(kArchiveMountPath));
  // SmbFsShare comes up with a unique stable ID for the share, it can
  // just be faked here, the mount ID is expected to appear in the mount
  // path, it's faked too.
  mount_points->RegisterFileSystem(
      "smbfsmountid" /* fake mount ID for mount name */,
      storage::kFileSystemTypeSmbFs, storage::FileSystemMountOption(),
      FilePath("/media/fuse/smbfs-smbfsmountid"));
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      base::StrCat({util::kFuseBoxMountNamePrefix, "id"}),
      storage::kFileSystemTypeFuseBox, storage::FileSystemMountOption(),
      base::FilePath::FromUTF8Unsafe(util::kFuseBoxMediaPath)
          .AppendASCII("subdir"));

  FilePath inside;
  FileSystemURL url;
  FilePath vm_mount("/mnt/chromeos");

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
      {
          "fubomona:id",
          "path/in/fusebox",
          "/mnt/chromeos/Fusebox/subdir/path/in/fusebox",
      },
  };

  for (const auto& test : tests) {
    // ConvertFileSystemURLToPathInsideVM.
    EXPECT_TRUE(ConvertFileSystemURLToPathInsideVM(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            blink::StorageKey(), test.mount_name, FilePath(test.relative_path)),
        vm_mount, /*map_crostini_home=*/false, &inside));
    EXPECT_EQ(test.inside, inside.value());

    // ConvertPathInsideVMToFileSystemURL.
    EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
        profile_.get(), FilePath(test.inside), vm_mount,
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
          FilePath("path/in/crostini")),
      vm_mount, /*map_crostini_home=*/true, &inside));
  EXPECT_EQ("/home/testuser/path/in/crostini", inside.value());
  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(
          blink::StorageKey(), "crostini_0123456789abcdef_termina_penguin",
          FilePath("path/in/crostini")),
      &inside));
  EXPECT_EQ("/home/testuser/path/in/crostini", inside.value());

  EXPECT_FALSE(ConvertFileSystemURLToPathInsideVM(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(blink::StorageKey(), "unknown",
                                                FilePath("path/in/unknown")),
      vm_mount, /*map_crostini_home=*/false, &inside));

  // Special case for Crostini $HOME ConvertPathInsideVMToFileSystemURL.
  EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), FilePath("/home/testuser/path/in/crostini"), vm_mount,
      /*map_crostini_home=*/true, &url));
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(file_manager::util::GetFileManagerURL(), url.origin().GetURL());
  EXPECT_EQ("crostini_0123456789abcdef_termina_penguin/path/in/crostini",
            url.virtual_path().value());
  EXPECT_FALSE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), FilePath("/home/testuser/path/in/crostini"), vm_mount,
      /*map_crostini_home=*/false, &url));

  EXPECT_FALSE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), FilePath("/path/not/under/mount"), vm_mount,
      /*map_crostini_home=*/false, &url));

  // Special case for PluginVM case-insensitive hostname matching.
  EXPECT_TRUE(ConvertPathInsideVMToFileSystemURL(
      profile_.get(), FilePath("//chromeos/MyFiles/path/in/pluginvm"),
      FilePath("//ChromeOS"), /*map_crostini_home=*/false, &url));
  EXPECT_EQ("Downloads-testing_profile%40test-hash/path/in/pluginvm",
            url.virtual_path().value());

  profile_.reset();
  ash::SeneschalClient::Shutdown();
  ash::ConciergeClient::Shutdown();
  ash::ChunneldClient::Shutdown();
}

TEST_F(FileManagerPathUtilTest, ConvertFuseMonikerPathToPathInsideVM) {
  base::FilePath inside("/random/path");
  EXPECT_FALSE(ConvertFuseboxMonikerPathToPathInsideVM(
      base::FilePath("/media/fuse/fusebox/subdir/path"),
      base::FilePath("/mnt/chromeos"), &inside));
  EXPECT_EQ(inside.value(), "/random/path");

  EXPECT_TRUE(ConvertFuseboxMonikerPathToPathInsideVM(
      base::FilePath("/media/fuse/fusebox/moniker/token"),
      base::FilePath("/mnt/chromeos"), &inside));
  EXPECT_EQ(inside.value(), "/mnt/chromeos/Fusebox/moniker/token");
}

TEST_F(FileManagerPathUtilTest, ExtractMountNameFileSystemNameFullPath) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  std::string downloads_mount_name = GetDownloadsMountPointName(profile_.get());
  FilePath downloads = GetDownloadsFolderForProfile(profile_.get());
  mount_points->RegisterFileSystem(downloads_mount_name,
                                   storage::kFileSystemTypeLocal,
                                   storage::FileSystemMountOption(), downloads);
  FilePath removable = FilePath(kRemovableMediaPath);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), FilePath(kRemovableMediaPath));
  FilePath archive = FilePath(kArchiveMountPath);
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), FilePath(kArchiveMountPath));
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
      FilePath("/unknown/path"), &mount_name, &file_system_name, &full_path));
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
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up fake user manager.
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
    const AccountId account_id_2(
        AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222"));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    fake_user_manager_->AddUser(account_id_2);
    fake_user_manager_->LoginUser(account_id_2);

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
        new ash::disks::FakeDiskMountManager);

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
    arc_service_manager_->set_browser_context(nullptr);
    profile_manager_.reset();
    fake_user_manager_.Reset();

    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();

    ash::disks::DiskMountManager::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  arc::FakeFileSystemInstance fake_file_system_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> primary_profile_;
  raw_ptr<TestingProfile, DanglingUntriaged> secondary_profile_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  FilePath drive_mount_point_;
  FilePath crostini_mount_point_;
};

FileSystemURL CreateExternalURL(const FilePath& path) {
  return FileSystemURL::CreateForTest(blink::StorageKey(),
                                      storage::kFileSystemTypeExternal, path);
}

storage::FileSystemURL CreateExternalURL(
    const blink::StorageKey& storage_key,
    storage::FileSystemType file_system_type,
    const std::string& file_system_id,
    const std::string& virtual_path,
    const std::string& cracked_path) {
  return storage::FileSystemURL::CreateForTest(
      storage_key, storage::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(virtual_path), file_system_id,
      file_system_type, base::FilePath::FromUTF8Unsafe(cracked_path),
      file_system_id, storage::FileSystemMountOption());
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Archive) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"", "--enable-arcvm"});
  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      FilePath::FromUTF8Unsafe("/media/archive/Smile 🙂.zip/"
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
  EXPECT_TRUE(
      ConvertPathToArcUrl(FilePath::FromUTF8Unsafe("/media/removable/a/b/c"),
                          &url, &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
            url);
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_MyFiles) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  GURL url;
  bool requires_sharing = false;
  const FilePath myfiles = GetMyFilesFolderForProfile(primary_profile_);
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
  EXPECT_FALSE(
      ConvertPathToArcUrl(FilePath::FromUTF8Unsafe("/media/removable_foobar"),
                          &url, &requires_sharing));
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidDownloads) {
  // Non-primary profile's downloads folder is not supported for ARC yet.
  GURL url;
  bool requires_sharing = false;
  const FilePath downloads2 = GetDownloadsFolderForProfile(secondary_profile_);
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url,
                                   &requires_sharing));
  EXPECT_FALSE(requires_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_CrostiniOnArcContainer) {
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

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_CrostiniOnArcVm) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());

  GURL url;
  bool requires_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(crostini_mount_point_.AppendASCII("a/b/c"),
                                  &url, &requires_sharing));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/crostini/a/b/c"),
            url);
  EXPECT_TRUE(requires_sharing);
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
       ConvertPathToArcUrl_FuseboxOnArcContainer) {
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      base::StrCat({util::kFuseBoxMountNamePrefix, "subdir"}),
      storage::kFileSystemTypeFuseBox, storage::FileSystemMountOption(),
      base::FilePath::FromUTF8Unsafe(util::kFuseBoxMediaPath)
          .AppendASCII("subdir"));

  // On ARC++ container, Fusebox paths are converted into
  // ChromeContentProvider URIs which do not need to be shared via seneschal.
  GURL url;
  bool requires_seneschal_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe(util::kFuseBoxMediaPath)
          .AppendASCII("subdir/a/b/c"),
      &url, &requires_seneschal_sharing));
  EXPECT_EQ(url, GURL("content://org.chromium.arc.chromecontentprovider/"
                      "externalfile%3Afubomona%253Asubdir%2Fa%2Fb%2Fc"));
  EXPECT_FALSE(requires_seneschal_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_FuseboxOnArcVm) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());

  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      base::StrCat({util::kFuseBoxMountNamePrefix, "subdir"}),
      storage::kFileSystemTypeFuseBox, storage::FileSystemMountOption(),
      base::FilePath::FromUTF8Unsafe(util::kFuseBoxMediaPath)
          .AppendASCII("subdir"));

  // On ARCVM, Fusebox paths are converted into ArcVolumeProvider URIs which
  // need to be shared via seneschal.
  GURL url;
  bool requires_seneschal_sharing = false;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe(util::kFuseBoxMediaPath)
          .AppendASCII("subdir/a/b/c"),
      &url, &requires_seneschal_sharing));
  EXPECT_EQ(
      url,
      GURL("content://org.chromium.arc.volumeprovider/fusebox/subdir/a/b/c"));
  EXPECT_TRUE(requires_seneschal_sharing);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertFileSystemURLToPathForSharingWithArc) {
  const std::string file_manager_origin =
      url::Origin::Create(GetFileManagerURL()).Serialize();
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(file_manager_origin);

  // Register volumes for FSP and MTP in the Fusebox server.
  fusebox::Server fusebox_server(/*delegate=*/nullptr);
  fusebox_server.RegisterFSURLPrefix(
      "fsp.subdir",
      base::StrCat({"filesystem:", file_manager_origin,
                    "/external/extensionid%3Afilesystemid%3Auserhash"}),
      /*read_only=*/true);
  fusebox_server.RegisterFSURLPrefix(
      "mtp.subdir",
      base::StrCat({"filesystem:", file_manager_origin,
                    "/external/fileman-mtp-mtp%3Aserialnumber%3Astorageid"}),
      /*read_only=*/true);

  // FileSystemURLs for FSP and MTP are converted into Fusebox VFS paths.
  EXPECT_EQ(
      ConvertFileSystemURLToPathForSharingWithArc(CreateExternalURL(
          storage_key, storage::kFileSystemTypeProvided,
          "extensionid:filesystemid:userhash",
          "extensionid:filesystemid:userhash/a/b/c",
          "/provided/extensionid:filesystemid:userhash/a/b/c")),
      base::FilePath::FromUTF8Unsafe("/media/fuse/fusebox/fsp.subdir/a/b/c"));
  EXPECT_EQ(
      ConvertFileSystemURLToPathForSharingWithArc(CreateExternalURL(
          storage_key, storage::kFileSystemTypeDeviceMediaAsFileStorage,
          "fileman-mtp-mtp:serialnumber:storageid",
          "fileman-mtp-mtp:serialnumber:storageid/a/b/c",
          "/usb:x,y:storageid/a/b/c")),
      base::FilePath::FromUTF8Unsafe("/media/fuse/fusebox/mtp.subdir/a/b/c"));

  // For the other FileSystemURLs, the path associated with the original URL is
  // returned as-is.
  EXPECT_EQ(ConvertFileSystemURLToPathForSharingWithArc(
                CreateExternalURL(storage_key, storage::kFileSystemTypeLocal,
                                  "Downloads-testing_profile%40test-hash",
                                  "Downloads-testing_profile%40test-hash/a/b/c",
                                  GetMyFilesFolderForProfile(primary_profile_)
                                      .AppendASCII("a/b/c")
                                      .value())),
            GetMyFilesFolderForProfile(primary_profile_).AppendASCII("a/b/c"));
  EXPECT_EQ(ConvertFileSystemURLToPathForSharingWithArc(CreateExternalURL(
                storage_key, storage::kFileSystemTypeDriveFs,
                "drivefs-84675c855b63e12f384d45f033826980",
                "drivefs-84675c855b63e12f384d45f033826980/a/b/c",
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/a/b/c")),
            base::FilePath::FromUTF8Unsafe(
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/a/b/c"));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidMountType) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
                           blink::StorageKey(), storage::kFileSystemTypeTest,
                           FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Removable) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{CreateExternalURL(
                           FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MyFiles) {
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  const FilePath myfiles = GetMyFilesFolderForProfile(primary_profile_);
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{
                           CreateExternalURL(myfiles.AppendASCII("a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "0000000000000000000000000000CAFEF00D2019/a/b/c"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidRemovable) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(
          FilePath::FromUTF8Unsafe("/media/removable_foobar"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Downloads) {
  const FilePath downloads = GetDownloadsFolderForProfile(primary_profile_);
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{
                           CreateExternalURL(downloads.AppendASCII("a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/download/a/b/c"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidDownloads) {
  const FilePath downloads = GetDownloadsFolderForProfile(secondary_profile_);
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{
                           CreateExternalURL(downloads.AppendASCII("a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Special) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(ProfileManager::GetPrimaryUserProfile(),
                       std::vector<FileSystemURL>{CreateExternalURL(
                           drive_mount_point_.AppendASCII("a/b/c"))},
                       future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/externalfile"
                 "%3Adrivefs-b1f44746e7144c3caafeacaa8bb5c569%2Fa%2Fb%2Fc"),
            urls[0]);
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

  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeArcDocumentsProvider,
          FilePath::FromUTF8Unsafe("/special/arc-documents-provider/"
                                   "com.android.providers.media.documents/"
                                   "images_root/Download/photo.jpg"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://com.android.providers.media.documents/"
                 "document/photo-id"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_ArcDocumentsProviderFileNotFound) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeArcDocumentsProvider,
          FilePath::FromUTF8Unsafe("/special/arc-documents-provider/"
                                   "com.android.providers.media.documents/"
                                   "images_root/Download/photo.jpg"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(""), urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_AndroidFiles) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(FilePath::FromUTF8Unsafe(
          "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "external_files/Pictures/a/b.jpg"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(
          FilePath::FromUTF8Unsafe("/run/arc/sdcard/read/emulated/0/a/b/c"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_AndroidFiles_GuestOs) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(FilePath::FromUTF8Unsafe(
          "/media/fuse/android_files/Pictures/a/b.jpg"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "external_files/Pictures/a/b.jpg"),
            urls[0]);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles_GuestOs) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  EXPECT_TRUE(arc::IsArcVmEnabled());
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{CreateExternalURL(FilePath::FromUTF8Unsafe(
          "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MultipleUrls) {
  TestFuture<const std::vector<GURL>&, const std::vector<FilePath>&> future;
  ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(),
      std::vector<FileSystemURL>{
          CreateExternalURL(FilePath::FromUTF8Unsafe("/invalid")),
          CreateExternalURL(FilePath::FromUTF8Unsafe("/media/removable/a/b/c")),
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c")),
          CreateExternalURL(FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/a/b/c"))},
      future.GetCallback());
  const auto& urls = future.Get<0>();
  ASSERT_EQ(4U, urls.size());
  EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/0123-abcd/b/c"),
            urls[1]);
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/externalfile"
                 "%3Adrivefs-b1f44746e7144c3caafeacaa8bb5c569%2Fa%2Fb%2Fc"),
            urls[2]);
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                 "external_files/a/b/c"),
            urls[3]);
}

TEST_F(FileManagerPathUtilTest, GetDisplayablePathTest) {
  auto* volume_manager = VolumeManager::Get(profile_.get());
  volume_manager->RegisterDownloadsDirectoryForTesting(
      FilePath("/mount_path/my_files"));
  volume_manager->RegisterCrostiniDirectoryForTesting(
      FilePath("/mount_path/crostini"));
  volume_manager->RegisterAndroidFilesDirectoryForTesting(
      FilePath("/mount_path/android"));

  volume_manager->RegisterMediaViewForTesting(arc::kAudioRootId);
  volume_manager->RegisterMediaViewForTesting(arc::kImagesRootId);
  volume_manager->RegisterMediaViewForTesting(arc::kVideosRootId);
  volume_manager->RegisterMediaViewForTesting(arc::kDocumentsRootId);

  volume_manager->AddVolumeForTesting(
      Volume::CreateForDrive(FilePath("/mount_path/drive")));

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
       FilePath("/mount_path/provided"),
       false,
       false,
       extensions::SOURCE_FILE,
       {}},
      MOUNT_CONTEXT_USER));

  volume_manager->AddVolumeForTesting(Volume::CreateForDocumentsProvider(
      "authority", "root_id", "documents_provider_label", "summary", {}, false,
      /*optional_fusebox_subdir=*/std::string()));

  volume_manager->AddVolumeForTesting(Volume::CreateForSftpGuestOs(
      "guest_os_label", FilePath("/mount_path/guest_os"),
      FilePath("/remote_mount_path/guest_os"), guest_os::VmType::TERMINA));

  volume_manager->AddVolumeForTesting(
      Volume::CreateForMTP(FilePath("/mount_path/mtp"), "mtp_label", false));

  volume_manager->AddVolumeForTesting(
      Volume::CreateForSmb(FilePath("/mount_path/smb"), "smb_label"));

  volume_manager->AddVolumeForTesting(
      Volume::CreateForShareCache(FilePath("/mount_path/share_cache")));

  volume_manager->AddVolumeForTesting(
      FilePath("/mount_path/testing"), VOLUME_TYPE_TESTING,
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
                                          arc::kAudioRootId)
           .value(),
       "Audio"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kImagesRootId)
           .Append("foo")
           .value(),
       "Images/foo"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kVideosRootId)
           .Append("foo/bar")
           .value(),
       "Videos/foo/bar"},
      {arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                          arc::kDocumentsRootId)
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
          arc::GetDocumentsProviderMountPath("authority", "root_id").value(),
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
    EXPECT_EQ(FilePath(test.expected),
              *GetDisplayablePath(profile_.get(), FilePath(test.path)));
  }
  EXPECT_EQ(std::nullopt, GetDisplayablePath(profile_.get(),
                                             FilePath("/non_existent/mount")));
  EXPECT_EQ(
      std::nullopt,
      GetDisplayablePath(profile_.get(), FilePath("/mount_path/share_cache")));
  EXPECT_EQ(std::nullopt, GetDisplayablePath(profile_.get(),
                                             FilePath("/mount_path/testing")));
}

TEST_F(FileManagerPathUtilTest, ParseFileSystemSources) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  FilePath myfiles_dir = GetMyFilesFolderForProfile(profile_.get());
  mount_points->RegisterFileSystem(
      GetDownloadsMountPointName(profile_.get()), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), myfiles_dir);
  mount_points->RegisterFileSystem(
      "fake_aa_mount_name", storage::kFileSystemTypeArcContent,
      storage::FileSystemMountOption(), FilePath("/fake/aa_dir"));
  mount_points->RegisterFileSystem(
      "fake_ad_mount_name", storage::kFileSystemTypeArcContent,
      storage::FileSystemMountOption(), FilePath("/fake/ad_dir"));

  const GURL file_manager_url = GetFileManagerURL();
  const url::Origin file_manager_origin = url::Origin::Create(file_manager_url);
  fusebox::Server fusebox_server(nullptr);
  // Accept the "allow" flavor (but reject the "deny" one).
  fusebox_server.RegisterFSURLPrefix(
      "my_subdir",
      base::StrCat({"filesystem:", file_manager_origin.Serialize(),
                    "/external/fake_aa_mount_name/"}),
      true);

  FilePath shared_path = myfiles_dir.Append("shared");
  auto* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile_.get());
  guest_os_share_path->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                          shared_path);
  // Start with four file_names but the last one is rejected. Its FileSystemURL
  // has type storage::kFileSystemTypeArcContent, so its cracked data does not
  // refer to a real (kernel visible) file path. But also, the fusebox_server
  // (configured above) has no registered mapping.
  std::vector<std::string> file_names = {
      "external/Downloads/shared/file1",
      "external/Downloads/shared/file2",
      "external/fake_aa_mount_name/a/b/c.txt",
      "external/fake_ad_mount_name/d/e/f.txt",
  };
  std::vector<std::string> file_urls =
      base::ToVector(file_names, [&file_manager_url](const std::string& name) {
        return base::StrCat({url::kFileSystemScheme, ":",
                             file_manager_url.Resolve(name).spec()});
      });
  std::u16string urls(base::ASCIIToUTF16(base::JoinString(file_urls, "\n")));
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(
      std::unordered_map<std::u16string, std::u16string>(
          {{u"fs/tag", u"exo"}, {u"fs/sources", urls}}),
      &pickle);

  ui::DataTransferEndpoint files_app(file_manager_url.Resolve("main.html"));
  std::vector<ui::FileInfo> file_info =
      ParseFileSystemSources(&files_app, pickle);
  EXPECT_EQ(3u, file_info.size());
  EXPECT_EQ(shared_path.Append("file1"), file_info[0].path);
  EXPECT_EQ(shared_path.Append("file2"), file_info[1].path);
  EXPECT_EQ(FilePath("/media/fuse/fusebox/my_subdir/a/b/c.txt"),
            file_info[2].path);
  EXPECT_EQ(FilePath(), file_info[0].display_name);
  EXPECT_EQ(FilePath(), file_info[1].display_name);
  EXPECT_EQ(FilePath(), file_info[2].display_name);

  // Should return empty if source is not FilesApp.
  ui::DataTransferEndpoint crostini(ui::EndpointType::kCrostini);
  file_info = ParseFileSystemSources(&crostini, pickle);
  EXPECT_TRUE(file_info.empty());
}

}  // namespace
}  // namespace file_manager::util
