// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/file_system/consent_provider.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_features.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/switches.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "ui/base/ui_base_types.h"

// TODO(michaelpg): Port these tests to app_shell: crbug.com/505926.

using file_manager::VolumeManager;
using file_manager::VolumeType;

namespace extensions {
namespace {

// Mount point names for chrome.fileSystem.requestFileSystem() tests.
const char kWritableMountPointName[] = "writable";
const char kReadOnlyMountPointName[] = "read-only";
const char kDownloadsMountPointName[] = "downloads";

// Child directory created in each of the mount points.
const char kChildDirectory[] = "child-dir";

// ID of a testing extension.
const char kTestingExtensionId[] = "pkplfbidichfdicaijlchgnapepdginl";

void IgnoreDriveEntryResult(google_apis::DriveApiErrorCode error,
                            std::unique_ptr<google_apis::FileResource> entry) {}

}  // namespace

// Skips the user consent dialog for chrome.fileSystem.requestFileSystem() and
// simulates clicking of the specified dialog button.
class ScopedSkipRequestFileSystemDialog {
 public:
  explicit ScopedSkipRequestFileSystemDialog(ui::DialogButton button) {
    file_system_api::ConsentProviderDelegate::SetAutoDialogButtonForTest(
        button);
  }
  ~ScopedSkipRequestFileSystemDialog() {
    file_system_api::ConsentProviderDelegate::SetAutoDialogButtonForTest(
        ui::DIALOG_BUTTON_NONE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSkipRequestFileSystemDialog);
};

// Observers adding a listener to the |event_name| event by |extension|, and
// then fires the |callback|.
class ScopedAddListenerObserver : public EventRouter::Observer {
 public:
  ScopedAddListenerObserver(Profile* profile,
                            const std::string& event_name,
                            const std::string& extension_id,
                            base::OnceClosure callback)
      : extension_id_(extension_id),
        callback_(std::move(callback)),
        event_router_(EventRouter::EventRouter::Get(profile)) {
    DCHECK(profile);
    event_router_->RegisterObserver(this, event_name);
  }

  ~ScopedAddListenerObserver() override {
    event_router_->UnregisterObserver(this);
  }

  // EventRouter::Observer overrides.
  void OnListenerAdded(const EventListenerInfo& details) override {
    // Call the callback only once, as the listener may be added multiple times.
    if (details.extension_id != extension_id_ || !callback_)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback_));
  }

 private:
  const std::string extension_id_;
  base::OnceClosure callback_;
  EventRouter* const event_router_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAddListenerObserver);
};

// This class contains chrome.filesystem API test specific to Chrome OS, namely,
// the integrated Google Drive support.
class FileSystemApiTestForDrive : public PlatformAppBrowserTest {
 public:
  FileSystemApiTestForDrive() {}

  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  // Sets up fake Drive service for tests (this has to be injected before the
  // real DriveIntegrationService instance is created.)
  void SetUpInProcessBrowserTestFixture() override {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    create_drive_integration_service_ =
        base::Bind(&FileSystemApiTestForDrive::CreateDriveIntegrationService,
                   base::Unretained(this));
    service_factory_for_test_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  // Ensure the fake service's data is fetch in the local file system. This is
  // necessary because the fetch starts lazily upon the first read operation.
  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();

    if (!base::FeatureList::IsEnabled(chromeos::features::kDriveFs)) {
      std::unique_ptr<drive::ResourceEntry> entry;
      drive::FileError error = drive::FILE_ERROR_FAILED;
      integration_service_->file_system()->GetResourceEntry(
          base::FilePath::FromUTF8Unsafe("drive/root"),  // whatever
          google_apis::test_util::CreateCopyResultCallback(&error, &entry));
      content::RunAllTasksUntilIdle();
      ASSERT_EQ(drive::FILE_ERROR_OK, error);
    }
  }

  void TearDown() override {
    FileSystemChooseEntryFunction::StopSkippingPickerForTest();
    PlatformAppBrowserTest::TearDown();
  };

  base::FilePath GetDriveMountPoint() {
    if (base::FeatureList::IsEnabled(chromeos::features::kDriveFs)) {
      return drivefs_mount_point_;
    } else {
      return drive::util::GetDriveMountPointPath(browser()->profile());
    }
  }

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    // FileSystemApiTestForDrive doesn't expect that several user profiles could
    // exist simultaneously.
    DCHECK(!fake_drive_service_);
    fake_drive_service_ = new drive::FakeDriveService;

    CHECK(drivefs_root_.CreateUniqueTempDir());
    drivefs_mount_point_ = drivefs_root_.GetPath().Append("drive-user");
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point_);

    SetUpTestFileHierarchy();

    integration_service_ = new drive::DriveIntegrationService(
        profile, nullptr, fake_drive_service_, "", test_cache_root_.GetPath(),
        nullptr,
        fake_drivefs_helper_->CreateFakeDriveFsConnectionDelegateFactory());
    return integration_service_;
  }

  void SetUpTestFileHierarchy() {
    CHECK(base::CreateDirectory(drivefs_mount_point_.Append("root")));
    const std::string root = fake_drive_service_->GetRootResourceId();
    AddTestFile("open_existing.txt", "Can you see me?", root);
    AddTestFile("open_existing1.txt", "Can you see me?", root);
    AddTestFile("open_existing2.txt", "Can you see me?", root);
    AddTestFile("save_existing.txt", "Can you see me?", root);

    const char kSubdirResourceId[] = "subdir_resource_id";
    AddTestDirectory(kSubdirResourceId, "subdir", root);
    AddTestFile("open_existing.txt", "Can you see me?", kSubdirResourceId);
  }

  void AddTestFile(const std::string& title,
                   const std::string& data,
                   const std::string& parent_id) {
    fake_drive_service_->AddNewFile("text/plain", data, parent_id, title, false,
                                    base::Bind(&IgnoreDriveEntryResult));
    base::FilePath parent_path = drivefs_mount_point_.Append("root");
    if (parent_id == "subdir_resource_id") {
      parent_path = parent_path.Append("subdir");
    }
    CHECK(base::WriteFile(parent_path.Append(title), data.data(), data.size()));
  }

  void AddTestDirectory(const std::string& resource_id,
                        const std::string& title,
                        const std::string& parent_id) {
    fake_drive_service_->AddNewDirectoryWithResourceId(
        resource_id, parent_id, title, drive::AddNewDirectoryOptions(),
        base::Bind(&IgnoreDriveEntryResult));
    CHECK(base::CreateDirectory(
        drivefs_mount_point_.Append("root").Append(title)));
  }

  base::ScopedTempDir test_cache_root_;
  base::ScopedTempDir drivefs_root_;
  base::FilePath drivefs_mount_point_;
  drive::FakeDriveService* fake_drive_service_ = nullptr;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  drive::DriveIntegrationService* integration_service_ = nullptr;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

// This class contains chrome.filesystem.requestFileSystem API tests.
class FileSystemApiTestForRequestFileSystem : public PlatformAppBrowserTest {
 public:
  FileSystemApiTestForRequestFileSystem() : fake_user_manager_(nullptr) {}

  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestingExtensionId);
  }

  // Sets up fake Drive service for tests (this has to be injected before the
  // real DriveIntegrationService instance is created.)
  void SetUpInProcessBrowserTestFixture() override {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    create_drive_integration_service_ = base::BindRepeating(
        &FileSystemApiTestForRequestFileSystem::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateTestingFileSystem(kWritableMountPointName,
                            file_manager::VOLUME_TYPE_TESTING,
                            false /* read_only */);
    CreateTestingFileSystem(kReadOnlyMountPointName,
                            file_manager::VOLUME_TYPE_TESTING,
                            true /* read_only */);
    CreateTestingFileSystem(kDownloadsMountPointName,
                            file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY,
                            false /* read_only */);
    PlatformAppBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    PlatformAppBrowserTest::TearDownOnMainThread();
    user_manager_enabler_.reset();
    fake_user_manager_ = nullptr;
  }

  // Simulates mounting a removable volume.
  void MountFakeVolume() {
    VolumeManager* const volume_manager =
        VolumeManager::Get(browser()->profile());
    ASSERT_TRUE(volume_manager);
    volume_manager->AddVolumeForTesting(
        base::FilePath("/a/b/c"), file_manager::VOLUME_TYPE_TESTING,
        chromeos::DEVICE_TYPE_UNKNOWN, false /* read_only */);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  // Creates a testing file system in a testing directory.
  void CreateTestingFileSystem(const std::string& mount_point_name,
                               VolumeType volume_type,
                               bool read_only) {
    const base::FilePath mount_point_path =
        temp_dir_.GetPath().Append(mount_point_name);
    ASSERT_TRUE(base::CreateDirectory(mount_point_path));
    ASSERT_TRUE(
        base::CreateDirectory(mount_point_path.Append(kChildDirectory)));
    ASSERT_TRUE(content::BrowserContext::GetMountPoints(browser()->profile())
                    ->RegisterFileSystem(
                        mount_point_name, storage::kFileSystemTypeNativeLocal,
                        storage::FileSystemMountOption(), mount_point_path));
    VolumeManager* const volume_manager =
        VolumeManager::Get(browser()->profile());
    ASSERT_TRUE(volume_manager);
    volume_manager->AddVolumeForTesting(mount_point_path, volume_type,
                                        chromeos::DEVICE_TYPE_UNKNOWN,
                                        read_only);
  }

  // Simulates entering the kiosk session.
  void EnterKioskSession() {
    fake_user_manager_ = new chromeos::FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    const AccountId kiosk_app_account_id =
        AccountId::FromUserEmail("kiosk@foobar.com");
    fake_user_manager_->AddKioskAppUser(kiosk_app_account_id);
    fake_user_manager_->LoginUser(kiosk_app_account_id);
  }

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    CHECK(drivefs_root_.CreateUniqueTempDir());
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_root_.GetPath().Append("drive-user"));

    return new drive::DriveIntegrationService(
        profile, nullptr, nullptr, "", {}, nullptr,
        fake_drivefs_helper_->CreateFakeDriveFsConnectionDelegateFactory());
  }

  base::ScopedTempDir drivefs_root_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenExistingFileTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/open_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenExistingFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/open_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenMultipleSuggested) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/open_existing.txt");
  ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DOCUMENTS, test_file.DirName(), true, false));
  FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest();
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_multiple_with_suggested_name"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenMultipleExistingFilesTest) {
  base::FilePath test_file1 =
      GetDriveMountPoint().AppendASCII("root/open_existing1.txt");
  base::FilePath test_file2 =
      GetDriveMountPoint().AppendASCII("root/open_existing2.txt");
  std::vector<base::FilePath> test_files;
  test_files.push_back(test_file1);
  test_files.push_back(test_file2);
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_multiple_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
}

#if defined(ADDRESS_SANITIZER)
// Flaky when run under ASan: crbug.com/499233.
#define MAYBE_FileSystemApiOpenDirectoryWithWriteTest \
  DISABLED_FileSystemApiOpenDirectoryWithWriteTest
#else
#define MAYBE_FileSystemApiOpenDirectoryWithWriteTest \
  FileSystemApiOpenDirectoryWithWriteTest
#endif
IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       MAYBE_FileSystemApiOpenDirectoryWithWriteTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/open_directory_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithoutPermissionTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_without_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithOnlyWritePermissionTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_with_only_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveNewFileTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_new.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveExistingFileTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
    FileSystemApiSaveNewFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_new.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
    FileSystemApiSaveExistingFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_existing.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/save_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, Background) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_OK);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/request_file_system_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, ReadOnly) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_OK);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/request_file_system_read_only"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, Writable) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_OK);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/request_file_system_writable"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, UserReject) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_CANCEL);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/request_file_system_user_reject"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, NotKioskSession) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_OK);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/request_file_system_not_kiosk_session"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       WhitelistedComponent) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_CANCEL);
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "api_test/file_system/request_file_system_whitelisted_component",
      kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       NotWhitelistedComponent) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_OK);
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "api_test/file_system/request_file_system_not_whitelisted_component",
      kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, GetVolumeList) {
  EnterKioskSession();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/get_volume_list"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       GetVolumeList_NotKioskSession) {
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_volume_list_not_kiosk_session"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       OnVolumeListChanged) {
  EnterKioskSession();

  ScopedAddListenerObserver observer(
      profile(), extensions::api::file_system::OnVolumeListChanged::kEventName,
      kTestingExtensionId,
      base::BindOnce(&FileSystemApiTestForRequestFileSystem::MountFakeVolume,
                     base::Unretained(this)));

  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/on_volume_list_changed"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       WhitelistedExtensionForDownloads) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(ui::DIALOG_BUTTON_CANCEL);
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "api_test/file_system/request_downloads_whitelisted_extension",
      kFlagLaunchPlatformApp))
      << message_;
}

}  // namespace extensions
