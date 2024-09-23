// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/api/file_system/consent_provider_impl.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/file_system.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

// TODO(michaelpg): Port these tests to app_shell: crbug.com/505926.

using file_manager::VolumeManager;

namespace extensions {
namespace {

// Mount point names for chrome.fileSystem.requestFileSystem() tests.
const char kWritableMountPointName[] = "writable";
const char kReadOnlyMountPointName[] = "read-only";

// Child directory created in each of the mount points.
const char kChildDirectory[] = "child-dir";

// ID of a testing extension.
const char kTestingExtensionId[] = "pkplfbidichfdicaijlchgnapepdginl";

}  // namespace

// Skips the user consent dialog for chrome.fileSystem.requestFileSystem() and
// simulates clicking of the specified dialog button.
class ScopedSkipRequestFileSystemDialog {
 public:
  explicit ScopedSkipRequestFileSystemDialog(ui::mojom::DialogButton button) {
    file_system_api::ConsentProviderDelegate::SetAutoDialogButtonForTest(
        button);
  }

  ScopedSkipRequestFileSystemDialog(const ScopedSkipRequestFileSystemDialog&) =
      delete;
  ScopedSkipRequestFileSystemDialog& operator=(
      const ScopedSkipRequestFileSystemDialog&) = delete;

  ~ScopedSkipRequestFileSystemDialog() {
    file_system_api::ConsentProviderDelegate::SetAutoDialogButtonForTest(
        ui::mojom::DialogButton::kNone);
  }
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

  ScopedAddListenerObserver(const ScopedAddListenerObserver&) = delete;
  ScopedAddListenerObserver& operator=(const ScopedAddListenerObserver&) =
      delete;

  ~ScopedAddListenerObserver() override {
    event_router_->UnregisterObserver(this);
  }

  // EventRouter::Observer overrides.
  void OnListenerAdded(const EventListenerInfo& details) override {
    // Call the callback only once, as the listener may be added multiple times.
    if (details.extension_id != extension_id_ || !callback_)
      return;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
  }

 private:
  const ExtensionId extension_id_;
  base::OnceClosure callback_;
  const raw_ptr<EventRouter> event_router_;
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

    create_drive_integration_service_ = base::BindRepeating(
        &FileSystemApiTestForDrive::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  // Ensure the fake service's data is fetch in the local file system. This is
  // necessary because the fetch starts lazily upon the first read operation.
  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override { PlatformAppBrowserTest::TearDown(); }

  base::FilePath GetDriveMountPoint() { return drivefs_mount_point_; }

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    // FileSystemApiTestForDrive doesn't expect that several user profiles could
    // exist simultaneously.
    CHECK(drivefs_root_.CreateUniqueTempDir());
    drivefs_mount_point_ = drivefs_root_.GetPath().Append("drive-user");
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point_);

    SetUpTestFileHierarchy();

    integration_service_ = new drive::DriveIntegrationService(
        profile, "", test_cache_root_.GetPath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
    return integration_service_;
  }

  void SetUpTestFileHierarchy() {
    const base::FilePath root = drivefs_mount_point_.Append("root");
    CHECK(base::CreateDirectory(root));

    std::string data = "Can you see me?";
    CHECK(base::WriteFile(root.Append("open_existing.txt"), data));
    CHECK(base::WriteFile(root.Append("open_existing1.txt"), data));
    CHECK(base::WriteFile(root.Append("open_existing2.txt"), data));
    CHECK(base::WriteFile(root.Append("save_existing.txt"), data));

    CHECK(base::CreateDirectory(root.Append("subdir")));
    CHECK(base::WriteFile(root.Append("subdir").Append("open_existing.txt"),
                          data));
  }

  base::ScopedTempDir test_cache_root_;
  base::ScopedTempDir drivefs_root_;
  base::FilePath drivefs_mount_point_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  raw_ptr<drive::DriveIntegrationService, DanglingUntriaged>
      integration_service_ = nullptr;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

// This class contains chrome.filesystem.requestFileSystem API tests.
class FileSystemApiTestForRequestFileSystem : public PlatformAppBrowserTest {
 public:
  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  // Sets up fake Drive service for tests (this has to be injected before the
  // real DriveIntegrationService instance is created.)
  void SetUpInProcessBrowserTestFixture() override {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    create_drive_integration_service_ = base::BindRepeating(
        &FileSystemApiTestForRequestFileSystem::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateTestingFileSystem(kWritableMountPointName, false /* read_only */);
    CreateTestingFileSystem(kReadOnlyMountPointName, true /* read_only */);
    PlatformAppBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    PlatformAppBrowserTest::TearDownOnMainThread();
    user_manager_.Reset();
  }

  // Simulates mounting a removable volume.
  void MountFakeVolume() {
    VolumeManager* const volume_manager =
        VolumeManager::Get(browser()->profile());
    ASSERT_TRUE(volume_manager);
    volume_manager->AddVolumeForTesting(
        base::FilePath("/a/b/c"), file_manager::VOLUME_TYPE_TESTING,
        ash::DeviceType::kUnknown, false /* read_only */);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;

  // Creates a testing file system in a testing directory.
  void CreateTestingFileSystem(const std::string& mount_point_name,
                               bool read_only) {
    const base::FilePath mount_point_path =
        temp_dir_.GetPath().Append(mount_point_name);
    ASSERT_TRUE(base::CreateDirectory(mount_point_path));
    ASSERT_TRUE(
        base::CreateDirectory(mount_point_path.Append(kChildDirectory)));
    ASSERT_TRUE(browser()->profile()->GetMountPoints()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), mount_point_path));
    VolumeManager* const volume_manager =
        VolumeManager::Get(browser()->profile());
    ASSERT_TRUE(volume_manager);
    volume_manager->AddVolumeForTesting(mount_point_path,
                                        file_manager::VOLUME_TYPE_TESTING,
                                        ash::DeviceType::kUnknown, read_only);
  }

  // Simulates entering the kiosk session.
  void EnterKioskSession() {
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    chromeos::SetUpFakeKioskSession();
  }

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    CHECK(drivefs_root_.CreateUniqueTempDir());
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_root_.GetPath().Append("drive-user"));

    return new drive::DriveIntegrationService(
        profile, "", {},
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
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
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_existing",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenExistingFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/open_existing.txt");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_existing_with_write",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenMultipleSuggested) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/open_existing.txt");
  ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DOCUMENTS, test_file.DirName(), true, false));
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .use_suggested_path = true};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/open_multiple_with_suggested_name",
                       {.launch_as_platform_app = true}))
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
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .paths_to_be_picked = &test_files};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_multiple_existing",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_directory};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_directory",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithWriteTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_directory};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_directory_with_write",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithoutPermissionTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_directory};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/open_directory_without_permission",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiOpenDirectoryWithOnlyWritePermissionTest) {
  base::FilePath test_directory =
      GetDriveMountPoint().AppendASCII("root/subdir");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_directory};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/open_directory_with_only_write",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveNewFileTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_new.txt");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/save_new",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveExistingFileTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_existing.txt");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/save_existing",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveNewFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_new.txt");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/save_new_with_write",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForDrive,
                       FileSystemApiSaveExistingFileWithWriteTest) {
  base::FilePath test_file =
      GetDriveMountPoint().AppendASCII("root/save_existing.txt");
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/save_existing_with_write",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, Background) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kOk);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/request_file_system_background",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, ReadOnly) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kOk);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/request_file_system_read_only",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, Writable) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kOk);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/request_file_system_writable",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, UserReject) {
  EnterKioskSession();
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kCancel);
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/request_file_system_user_reject",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, NotKioskSession) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kOk);
  ASSERT_TRUE(RunExtensionTest(
      "api_test/file_system/request_file_system_not_kiosk_session",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       AllowlistedComponent) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kCancel);
  ASSERT_TRUE(RunExtensionTest(
      "api_test/file_system/request_file_system_allowed_component",
      {.launch_as_platform_app = true}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       NotAllowlistedComponent) {
  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kOk);
  ASSERT_TRUE(RunExtensionTest(
      "api_test/file_system/request_file_system_not_allowed_component",
      {.launch_as_platform_app = true}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, GetVolumeList) {
  EnterKioskSession();
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/get_volume_list",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       GetVolumeList_NotKioskSession) {
  ASSERT_TRUE(
      RunExtensionTest("api_test/file_system/get_volume_list_not_kiosk_session",
                       {.launch_as_platform_app = true}))
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

  ASSERT_TRUE(RunExtensionTest("api_test/file_system/on_volume_list_changed",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       DlpOpenFileAllow) {
  file_access::MockScopedFileAccessDelegate file_access;
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      create;
  EXPECT_CALL(
      file_access,
      CreateFileAccessCallback(testing::Property(
          &GURL::spec,
          testing::MatchesRegex(
              "chrome-extension://.*/_generated_background_page\\.html"))))
      .WillOnce(testing::Return(create.Get()));
  EXPECT_CALL(create, Run)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Allowed()));

  base::FilePath test_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.GetPath());

    test_file = temp_dir_.GetPath().AppendASCII("open_existing.txt");
    base::WriteFile(test_file, "Can you see me?");
  }

  ASSERT_FALSE(test_file.empty());
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("api_test/file_system/open_existing",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem, DlpOpenFileDeny) {
  file_access::MockScopedFileAccessDelegate file_access;
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      create;
  EXPECT_CALL(
      file_access,
      CreateFileAccessCallback(testing::Property(
          &GURL::spec,
          testing::MatchesRegex(
              "chrome-extension://.*/_generated_background_page\\.html"))))
      .WillOnce(testing::Return(base::BindPostTask(
          content::GetIOThreadTaskRunner({}), create.Get())));
  EXPECT_CALL(create, Run)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess(false, base::ScopedFD())));

  base::FilePath test_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.GetPath());

    test_file = temp_dir_.GetPath().AppendASCII("open_existing.txt");
    base::WriteFile(test_file, "Can you see me?");
  }

  ASSERT_FALSE(test_file.empty());
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_FALSE(RunExtensionTest("api_test/file_system/open_existing",
                                {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       AllowlistedComponentBackgroundDlpAllow) {
  file_access::MockScopedFileAccessDelegate file_access;
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      create;
  EXPECT_CALL(
      file_access,
      CreateFileAccessCallback(testing::Property(
          &GURL::spec,
          testing::MatchesRegex(
              "chrome-extension://.*/_generated_background_page\\.html"))))
      .WillOnce(testing::Return(base::BindPostTask(
          content::GetIOThreadTaskRunner({}), create.Get())));
  EXPECT_CALL(create, Run)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess(true, base::ScopedFD())));

  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kCancel);
  const base::FilePath test_file = temp_dir_.GetPath()
                                       .Append(kReadOnlyMountPointName)
                                       .AppendASCII("open_existing.txt");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(test_file, "Can you see me?");
  }
  EXPECT_TRUE(RunExtensionTest(
      "api_test/file_system/request_file_system_allowed_component_background",
      {.launch_as_platform_app = true}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTestForRequestFileSystem,
                       AllowlistedComponentBackgroundDlpDeny) {
  file_access::MockScopedFileAccessDelegate file_access;
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      create;
  EXPECT_CALL(
      file_access,
      CreateFileAccessCallback(testing::Property(
          &GURL::spec,
          testing::MatchesRegex(
              "chrome-extension://.*/_generated_background_page\\.html"))))
      .WillOnce(testing::Return(base::BindPostTask(
          content::GetIOThreadTaskRunner({}), create.Get())));
  EXPECT_CALL(create, Run)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess(false, base::ScopedFD())));

  ScopedSkipRequestFileSystemDialog dialog_skipper(
      ui::mojom::DialogButton::kCancel);
  const base::FilePath test_file = temp_dir_.GetPath()
                                       .Append(kReadOnlyMountPointName)
                                       .AppendASCII("open_existing.txt");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(test_file, "Can you see me?");
  }
  EXPECT_FALSE(RunExtensionTest(
      "api_test/file_system/request_file_system_allowed_component_background",
      {.launch_as_platform_app = true}, {.load_as_component = true}))
      << message_;
}

}  // namespace extensions
