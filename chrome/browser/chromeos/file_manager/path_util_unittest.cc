// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

TEST(FileManagerPathUtilTest, GetDownloadsFolderForProfile) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile(base::FilePath("/home/chronos/u-0123456789abcdef"));
  std::string mount_point_name = GetDownloadsMountPointName(&profile);
  EXPECT_EQ("Downloads", mount_point_name);
}

TEST(FileManagerPathUtilTest, GetMyFilesFolderForProfile) {
  content::TestBrowserThreadBundle thread_bundle;

  base::FilePath profile_path =
      base::FilePath("/home/chronos/u-0123456789abcdef");
  TestingProfile profile(profile_path);

  // When running outside ChromeOS, it should return $HOME/Downloads for both
  // MyFiles and Downloads.
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetMyFilesFolderForProfile(&profile));
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetDownloadsFolderForProfile(&profile));

  // When running inside ChromeOS, it should return /home/u-{hash}/MyFiles.
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chromeos::features::kMyFilesVolume);
    // When MyFilesVolume feature is disabled it will return the same as
    // Downloads.
    EXPECT_EQ(GetDownloadsFolderForProfile(&profile),
              GetMyFilesFolderForProfile(&profile));
    EXPECT_EQ("/home/chronos/u-0123456789abcdef/Downloads",
              GetDownloadsFolderForProfile(&profile).value());
  }
  {
    // When MyFilesVolume feature is enabled Downloads path is inside MyFiles.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kMyFilesVolume);

    base::FilePath myfiles_path = profile_path.AppendASCII("MyFiles");
    base::FilePath myfiles_downloads_path =
        myfiles_path.AppendASCII("Downloads");

    EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
              GetMyFilesFolderForProfile(&profile).value());
    EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles/Downloads",
              GetDownloadsFolderForProfile(&profile).value());

    // Mount the volume to test the return from mount_points.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        GetDownloadsMountPointName(&profile),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        profile_path.Append("MyFiles"));

    // Still the same: /home/u-{hash}/MyFiles.
    EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
              GetMyFilesFolderForProfile(&profile).value());
  }
}

TEST(FileManagerPathUtilTest, GetPathDisplayTextForSettings) {
  content::TestBrowserThreadBundle thread_bundle;
  content::TestServiceManagerContext service_manager_context;

  TestingProfile profile(base::FilePath("/home/chronos/u-0123456789abcdef"));

  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             &profile, "/home/chronos/user/Downloads"));
  EXPECT_EQ("Downloads",
            GetPathDisplayTextForSettings(
                &profile, "/home/chronos/u-0123456789abcdef/Downloads"));

  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             &profile, "/home/chronos/user/MyFiles/Downloads"));
  EXPECT_EQ(
      "Downloads",
      GetPathDisplayTextForSettings(
          &profile, "/home/chronos/u-0123456789abcdef/MyFiles/Downloads"));

  EXPECT_EQ("Play files \u203a foo \u203a bar",
            GetPathDisplayTextForSettings(
                &profile, "/run/arc/sdcard/write/emulated/0/foo/bar"));
  EXPECT_EQ("Linux files \u203a foo",
            GetPathDisplayTextForSettings(
                &profile,
                "/media/fuse/crostini_0123456789abcdef_termina_penguin/foo"));

  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(chromeos::features::kDriveFs);
    drive::DriveIntegrationServiceFactory::GetForProfile(&profile);
    EXPECT_EQ("Google Drive \u203a My Drive \u203a foo",
              GetPathDisplayTextForSettings(
                  &profile, "/special/drive-0123456789abcdef/root/foo"));
    EXPECT_EQ(
        "Google Drive \u203a Team Drives \u203a A Team Drive \u203a foo",
        GetPathDisplayTextForSettings(
            &profile,
            "/special/drive-0123456789abcdef/team_drives/A Team Drive/foo"));

    EXPECT_EQ(
        "Google Drive \u203a Computers \u203a My Other Computer \u203a bar",
        GetPathDisplayTextForSettings(
            &profile,
            "/special/drive-0123456789abcdef/Computers/My Other Computer/bar"));
  }
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(chromeos::features::kDriveFs);
    chromeos::disks::DiskMountManager::InitializeForTesting(
        new FakeDiskMountManager);
    TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
    chromeos::FakeChromeUserManager user_manager;
    user_manager.AddUser(
        AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
    PrefService* prefs = profile2.GetPrefs();
    prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");

    drive::DriveIntegrationServiceFactory::GetForProfile(&profile2);
    EXPECT_EQ(
        "Google Drive \u203a My Drive \u203a foo",
        GetPathDisplayTextForSettings(
            &profile2,
            "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root/foo"));
    EXPECT_EQ("Google Drive \u203a Team Drives \u203a A Team Drive \u203a foo",
              GetPathDisplayTextForSettings(
                  &profile2,
                  "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                  "team_drives/A Team Drive/foo"));
    EXPECT_EQ(
        "Google Drive \u203a Computers \u203a My Other Computer \u203a bar",
        GetPathDisplayTextForSettings(
            &profile2,
            "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
            "Computers/My Other Computer/bar"));
  }
  chromeos::disks::DiskMountManager::Shutdown();
}

TEST(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  // MigratePathFromOldFormat is explicitly disabled on Linux build.
  // So we need to fake that this is real ChromeOS system.
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());

  // This looks like "/home/chronos/u-hash/Downloads" in the production
  // environment.
  const base::FilePath kDownloads = GetDownloadsFolderForProfile(&profile);
  const base::FilePath kOldDownloads =
      DownloadPrefs::GetDefaultDownloadDirectory();

  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(&profile, kOldDownloads, &path));
  EXPECT_EQ(kDownloads, path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      kOldDownloads.AppendASCII("a/b"),
      &path));
  EXPECT_EQ(kDownloads.AppendASCII("a/b"), path);

  // Path already in the new format is not converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      kDownloads.AppendASCII("a/b"),
      &path));

  // Only the "Downloads" path is converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/home/chronos/user/dl"),
      &path));
}

TEST(FileManagerPathUtilTest, ConvertFileSystemURLToPathInsideCrostini) {
  content::TestBrowserThreadBundle thread_bundle;
  content::TestServiceManagerContext service_manager_context;

  // Setup for DriveFS.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(chromeos::features::kDriveFs);
  TestingProfile profile(base::FilePath("/home/chronos/u-0123456789abcdef"));
  chromeos::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile.GetProfileUserName(), "12345"));
  profile.GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  // Register crostini, downloads, drive.
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      GetCrostiniMountPointName(&profile), storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), GetCrostiniMountDirectory(&profile));
  mount_points->RegisterFileSystem(
      GetDownloadsMountPointName(&profile), storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), GetDownloadsFolderForProfile(&profile));
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(&profile);
  base::FilePath mount_point_drive = integration_service->GetMountPointPath();
  mount_points->RegisterFileSystem(
      mount_point_drive.BaseName().value(), storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), mount_point_drive);

  base::FilePath inside;
  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "crostini_0123456789abcdef_termina_penguin",
          base::FilePath("path/in/crostini")),
      &inside));
  EXPECT_EQ("/home/testing_profile/path/in/crostini", inside.value());

  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "Downloads-testing_profile-hash",
          base::FilePath("path/in/downloads")),
      &inside));
  EXPECT_EQ("/ChromeOS/Downloads/path/in/downloads", inside.value());

  EXPECT_FALSE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "unknown", base::FilePath("path/in/unknown")),
      &inside));

  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "drivefs-84675c855b63e12f384d45f033826980",
          base::FilePath("root/path/in/mydrive")),
      &inside));
  EXPECT_EQ("/ChromeOS/Google Drive/My Drive/path/in/mydrive", inside.value());

  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "drivefs-84675c855b63e12f384d45f033826980",
          base::FilePath("team_drives/path/in/teamdrives")),
      &inside));
  EXPECT_EQ("/ChromeOS/Google Drive/Team Drives/path/in/teamdrives",
            inside.value());

  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      &profile,
      mount_points->CreateExternalFileSystemURL(
          GURL(), "drivefs-84675c855b63e12f384d45f033826980",
          base::FilePath("Computers/path/in/computers")),
      &inside));
  EXPECT_EQ("/ChromeOS/Google Drive/Computers/path/in/computers",
            inside.value());
}

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

class FileManagerPathUtilConvertUrlTest : public testing::Test {
 public:
  FileManagerPathUtilConvertUrlTest() = default;
  ~FileManagerPathUtilConvertUrlTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up fake user manager.
    chromeos::FakeChromeUserManager* fake_user_manager =
        new chromeos::FakeChromeUserManager();
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

    Profile* primary_profile =
        profile_manager_->CreateTestingProfile("user@gmail.com");
    ASSERT_TRUE(primary_profile);
    ASSERT_TRUE(profile_manager_->CreateTestingProfile("user2@gmail.com"));

    // Set up an Arc service manager with a fake file system.
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_service_manager_->set_browser_context(primary_profile);
    arc::ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        primary_profile,
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    ASSERT_TRUE(fake_file_system_.InitCalled());

    // Add a drive mount point for the primary profile.
    drive_mount_point_ = drive::util::GetDriveMountPointPath(primary_profile);
    const std::string mount_name = drive_mount_point_.BaseName().AsUTF8Unsafe();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_name, storage::kFileSystemTypeDrive,
        storage::FileSystemMountOption(), drive_mount_point_);
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    user_manager_enabler_.reset();
    profile_manager_.reset();

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  arc::FakeFileSystemInstance fake_file_system_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  base::FilePath drive_mount_point_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileManagerPathUtilConvertUrlTest);
};

FileSystemURL CreateExternalURL(const base::FilePath& path) {
  return FileSystemURL::CreateForTest(GURL(), storage::kFileSystemTypeExternal,
                                      path);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Removable) {
  GURL url;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.removablemediaprovider/a/b/c"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidRemovable) {
  GURL url;
  EXPECT_FALSE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable_foobar"), &url));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Downloads) {
  // Conversion of paths under the primary profile's downloads folder.
  GURL url;
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  EXPECT_TRUE(ConvertPathToArcUrl(downloads.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.intent_helper.fileprovider/"
                 "download/a/b/c"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidDownloads) {
  // Non-primary profile's downloads folder is not supported for ARC yet.
  GURL url;
  const base::FilePath downloads2 = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user2@gmail.com-hash"));
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Special) {
  GURL url;
  EXPECT_TRUE(
      ConvertPathToArcUrl(drive_mount_point_.AppendASCII("a/b/c"), &url));
  // "@" appears escaped 3 times here because escaping happens when:
  // - creating drive mount point name for user
  // - creating externalfile: URL from the path
  // - encoding the URL to Chrome content provider URL
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3Adrive-user%252540gmail.com-hash%2Fa%2Fb%2Fc"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidMountType) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
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
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.removablemediaprovider/a/b/c"),
                urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidRemovable) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable_foobar"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Downloads) {
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.intent_helper.fileprovider/"
                     "download/a/b/c"),
                urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidDownloads) {
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user2@gmail.com-hash"));
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
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
      std::vector<FileSystemURL>{
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrive-user%252540gmail.com-hash%2Fa%"
                           "2Fb%2Fc"),
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
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
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
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          GURL(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
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
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.intent_helper.fileprovider/"
                     "external_files/Pictures/a/b.jpg"),
                urls[0]);
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/read/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MultipleUrls) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe("/invalid")),
          CreateExternalURL(
              base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c")),
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c")),
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(4U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
            EXPECT_EQ(
                GURL("content://org.chromium.arc.removablemediaprovider/a/b/c"),
                urls[1]);
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrive-user%252540gmail.com-hash%2Fa%"
                           "2Fb%2Fc"),
                      urls[2]);
            EXPECT_EQ(
                GURL("content://org.chromium.arc.intent_helper.fileprovider/"
                     "external_files/a/b/c"),
                urls[3]);
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace
}  // namespace util
}  // namespace file_manager
