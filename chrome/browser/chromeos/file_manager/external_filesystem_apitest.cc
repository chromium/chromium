// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/drive/test_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

// Tests for access to external file systems (as defined in
// storage/common/file_system/file_system_types.h) from extensions with
// fileManagerPrivate and fileBrowserHandler extension permissions.
// The tests cover following external file system types:
// - local (kFileSystemTypeLocalNative): a local file system on which files are
//   accessed using native local path.
// - restricted (kFileSystemTypeRestrictedLocalNative): a *read-only* local file
//   system which can only be accessed by extensions that have full access to
//   external file systems (i.e. extensions with fileManagerPrivate permission).
//
// The tests cover following scenarios:
// - Performing file system operations on external file systems from an
//   app with fileManagerPrivate permission (i.e. The Files app).
// - Performing read/write operations from file handler extensions. These
//   extensions need a file browser extension to give them permissions to access
//   files. This also includes file handler extensions in filesystem API.
// - Observing directory changes from a file browser extension (using
//   fileManagerPrivate API).
// - Doing searches on drive file system from file browser extension (using
//   fileManagerPrivate API).

using drive::DriveIntegrationServiceFactory;
using extensions::Extension;

namespace file_manager {
namespace {

// Root dirs for file systems expected by the test extensions.
// NOTE: Root dir for drive file system is set by Chrome's drive implementation,
// but the test will have to make sure the mount point is added before
// starting a test extension using WaitUntilDriveMountPointIsAdded().
constexpr char kLocalMountPointName[] = "local";
constexpr char kRestrictedMountPointName[] = "restricted";

// Default file content for the test files.
constexpr char kTestFileContent[] = "This is some test content.";

// User account email and directory hash for secondary account for multi-profile
// sensitive test cases.
constexpr char kSecondProfileAccount[] = "profile2@test.com";
constexpr char kSecondProfileGiaId[] = "9876543210";
constexpr char kSecondProfileHash[] = "fileBrowserApiTestProfile2";

class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(ui::SelectFileDialog::Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       const base::FilePath& drivefs_root)
      : ui::SelectFileDialog(listener, std::move(policy)),
        drivefs_root_(drivefs_root) {}

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(drivefs_root_.Append("root/test_dir"), 0, nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override {}

  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;

  const base::FilePath drivefs_root_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(const base::FilePath& drivefs_root)
      : drivefs_root_(drivefs_root) {}

 private:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(listener, std::move(policy), drivefs_root_);
  }

  const base::FilePath drivefs_root_;
};

bool TouchFile(const base::FilePath& path,
               base::StringPiece mtime_string,
               base::StringPiece atime_string) {
  base::Time mtime, atime;
  auto result = base::Time::FromString(mtime_string.data(), &mtime) &&
                base::Time::FromString(atime_string.data(), &atime) &&
                base::TouchFile(path, atime, mtime);
  return result;
}

// Sets up the initial file system state for native local and restricted native
// local file systems. The hierarchy is the same as for the drive file system.
// The directory is created at unique_temp_dir/|mount_point_name| path.
bool InitializeLocalFileSystem(std::string mount_point_name,
                               base::ScopedTempDir* tmp_dir,
                               base::FilePath* mount_point_dir) {
  if (!tmp_dir->CreateUniqueTempDir())
    return false;

  *mount_point_dir = tmp_dir->GetPath().AppendASCII(mount_point_name);
  // Create the mount point.
  if (!base::CreateDirectory(*mount_point_dir))
    return false;

  base::FilePath test_dir = mount_point_dir->AppendASCII("test_dir");
  if (!base::CreateDirectory(test_dir))
    return false;

  base::FilePath test_subdir = test_dir.AppendASCII("empty_dir");
  if (!base::CreateDirectory(test_subdir))
    return false;

  test_subdir = test_dir.AppendASCII("subdir");
  if (!base::CreateDirectory(test_subdir))
    return false;

  base::FilePath test_file = test_dir.AppendASCII("test_file.xul");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.xul.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.tiff");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.tiff.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("empty_file.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, ""))
    return false;

  if (!TouchFile(test_dir.Append("empty_dir"), "2011-11-02T04:00:00.000Z",
                 "2011-11-02T04:00:00.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("subdir"), "2011-04-01T18:34:08.234Z",
                 "2012-01-02T00:00:01.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("test_file.xul"), "2011-12-14T00:40:47.330Z",
                 "2012-01-02T00:00:00.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("test_file.xul.foo"),
                 "2012-01-01T10:00:30.000Z", "2012-01-01T00:00:00.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("test_file.tiff"), "2011-04-03T11:11:10.000Z",
                 "2012-01-02T00:00:00.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("test_file.tiff.foo"),
                 "2011-12-14T00:40:47.330Z", "2010-01-02T00:00:00.000Z")) {
    return false;
  }
  if (!TouchFile(test_dir.Append("empty_file.foo"), "2011-12-14T00:40:47.330Z",
                 "2011-12-14T00:40:47.330Z")) {
    return false;
  }
  if (!TouchFile(test_dir, "2012-01-02T00:00:00.000Z",
                 "2012-01-02T00:00:01.000Z")) {
    return false;
  }
  return true;
}

// Helper class to wait for a background page to load or close again.
class BackgroundObserver {
 public:
  BackgroundObserver()
      : page_created_(extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        page_closed_(extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                     content::NotificationService::AllSources()) {}

  void WaitUntilLoaded() {
    page_created_.Wait();
  }

  void WaitUntilClosed() {
    page_closed_.Wait();
  }

 private:
  content::WindowedNotificationObserver page_created_;
  content::WindowedNotificationObserver page_closed_;
};

// Base class for FileSystemExtensionApi tests.
class FileSystemExtensionApiTestBase : public extensions::ExtensionApiTest {
 public:
  enum Flags {
    FLAGS_NONE = 0,
    FLAGS_USE_FILE_HANDLER = 1 << 1,
    FLAGS_LAZY_FILE_HANDLER = 1 << 2
  };

  FileSystemExtensionApiTestBase() = default;
  ~FileSystemExtensionApiTestBase() override = default;

  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUp() override {
    InitTestFileSystem();
    extensions::ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    AddTestMountPoint();

    // Mock the Media Router in extension api tests. Dispatches to the message
    // loop now try to handle mojo messages that will call back into Profile
    // creation through the media router, which then confuse the drive code.
    media_router_ = std::make_unique<media_router::MockMediaRouter>();
    ON_CALL(*media_router_, RegisterMediaSinksObserver(testing::_))
        .WillByDefault(testing::Return(true));
    CastConfigControllerMediaRouter::SetMediaRouterForTest(media_router_.get());

    extensions::ExtensionApiTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // Runs a file system extension API test.
  // It loads test component extension at |filebrowser_path| with manifest
  // at |filebrowser_manifest|. The |filebrowser_manifest| should be a path
  // relative to |filebrowser_path|. The method waits until the test extension
  // sends test succeed or fail message. It returns true if the test succeeds.
  // If |FLAGS_USE_FILE_HANDLER| flag is set, the file handler extension at path
  // |filehandler_path| will be loaded before the file browser extension.
  // If the flag FLAGS_LAZY_FILE_HANDLER is set, the file handler extension must
  // not have persistent background page. The test will wait until the file
  // handler's background page is closed after initial load before the file
  // browser extension is loaded.
  // If |RunFileSystemExtensionApiTest| fails, |message_| will contain a failure
  // message.
  bool RunFileSystemExtensionApiTest(
      const std::string& filebrowser_path,
      const base::FilePath::CharType* filebrowser_manifest,
      const std::string& filehandler_path,
      int flags) {
    if (flags & FLAGS_USE_FILE_HANDLER) {
      if (filehandler_path.empty()) {
        message_ = "Missing file handler path.";
        return false;
      }

      BackgroundObserver page_complete;
      const Extension* file_handler =
          LoadExtension(test_data_dir_.AppendASCII(filehandler_path));
      if (!file_handler) {
        message_ = "Error loading file handler extension";
        return false;
      }

      if (flags & FLAGS_LAZY_FILE_HANDLER) {
        page_complete.WaitUntilClosed();
      } else {
        page_complete.WaitUntilLoaded();
      }
    }

    extensions::ResultCatcher catcher;

    const Extension* file_browser = LoadExtensionAsComponentWithManifest(
        test_data_dir_.AppendASCII(filebrowser_path),
        filebrowser_manifest);
    if (!file_browser) {
      message_ = "Could not create file browser";
      return false;
    }

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

 protected:
  // Sets up initial test file system hierarchy.
  virtual void InitTestFileSystem() = 0;
  // Registers mount point used in the test.
  virtual void AddTestMountPoint() = 0;

 private:
  std::unique_ptr<media_router::MockMediaRouter> media_router_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemExtensionApiTestBase);
};

// Tests for a native local file system.
class LocalFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  LocalFileSystemExtensionApiTest() = default;
  ~LocalFileSystemExtensionApiTest() override = default;

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kLocalMountPointName, &tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(
        content::BrowserContext::GetMountPoints(profile())->RegisterFileSystem(
            kLocalMountPointName, storage::kFileSystemTypeNativeLocal,
            storage::FileSystemMountOption(), mount_point_dir_));
    VolumeManager::Get(profile())->AddVolumeForTesting(
        mount_point_dir_, VOLUME_TYPE_TESTING, chromeos::DEVICE_TYPE_UNKNOWN,
        false /* read_only */);
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::FilePath mount_point_dir_;
};

// Tests for restricted native local file systems.
class RestrictedFileSystemExtensionApiTest
    : public FileSystemExtensionApiTestBase {
 public:
  RestrictedFileSystemExtensionApiTest() = default;
  ~RestrictedFileSystemExtensionApiTest() override = default;

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kRestrictedMountPointName, &tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(
        content::BrowserContext::GetMountPoints(profile())->RegisterFileSystem(
            kRestrictedMountPointName,
            storage::kFileSystemTypeRestrictedNativeLocal,
            storage::FileSystemMountOption(), mount_point_dir_));
    VolumeManager::Get(profile())->AddVolumeForTesting(
        mount_point_dir_, VOLUME_TYPE_TESTING, chromeos::DEVICE_TYPE_UNKNOWN,
        true /* read_only */);
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::FilePath mount_point_dir_;
};

// Tests for a drive file system.
class DriveFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  DriveFileSystemExtensionApiTest() = default;
  ~DriveFileSystemExtensionApiTest() override = default;

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &DriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ =
        std::make_unique<DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    test_util::WaitUntilDriveMountPointIsAdded(profile());
  }

  // FileSystemExtensionApiTestBase override.
  void TearDown() override {
    FileSystemExtensionApiTestBase::TearDown();
    ui::SelectFileDialog::SetFactory(nullptr);
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    // DriveFileSystemExtensionApiTest doesn't expect that several user profiles
    // could exist simultaneously.
    base::FilePath drivefs_mount_point;
    InitializeLocalFileSystem("drive-user/root", &drivefs_root_,
                              &drivefs_mount_point);
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point.DirName());
    return new drive::DriveIntegrationService(
        profile, nullptr, "", test_cache_root_.GetPath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
  }

  base::ScopedTempDir test_cache_root_;
  base::ScopedTempDir drivefs_root_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

// Tests for Drive file systems in multi-profile setting.
class MultiProfileDriveFileSystemExtensionApiTest :
    public FileSystemExtensionApiTestBase {
 public:
  MultiProfileDriveFileSystemExtensionApiTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemExtensionApiTestBase::SetUpCommandLine(command_line);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                    "false");
  }

  void SetUpOnMainThread() override {
    base::FilePath user_data_directory;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    session_manager::SessionManager::Get()->CreateSession(
        AccountId::FromUserEmailGaiaId(kSecondProfileAccount,
                                       kSecondProfileGiaId),
        kSecondProfileHash, false);
    // Set up the secondary profile.
    base::FilePath profile_dir =
        user_data_directory.Append(
            chromeos::ProfileHelper::GetUserProfileDir(
                kSecondProfileHash).BaseName());
    second_profile_ =
        g_browser_process->profile_manager()->GetProfile(profile_dir);

    FileSystemExtensionApiTestBase::SetUpOnMainThread();
  }

  void InitTestFileSystem() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &MultiProfileDriveFileSystemExtensionApiTest::
            CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ =
        std::make_unique<DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
  }

  void AddTestMountPoint() override {
    test_util::WaitUntilDriveMountPointIsAdded(profile());
    test_util::WaitUntilDriveMountPointIsAdded(second_profile_);
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    base::FilePath cache_dir;
    base::CreateTemporaryDirInDir(tmp_dir_.GetPath(),
                                  base::FilePath::StringType(), &cache_dir);

    base::FilePath drivefs_dir;
    base::CreateTemporaryDirInDir(tmp_dir_.GetPath(),
                                  base::FilePath::StringType(), &drivefs_dir);
    auto profile_name_storage = profile->GetPath().BaseName().value();
    base::StringPiece profile_name = profile_name_storage;
    if (profile_name.starts_with("u-")) {
      profile_name = profile_name.substr(2);
    }
    drivefs_dir = drivefs_dir.Append(base::StrCat({"drive-", profile_name}));
    auto test_dir = drivefs_dir.Append("root/test_dir");
    CHECK(base::CreateDirectory(test_dir));
    CHECK(google_apis::test_util::WriteStringToFile(
        test_dir.AppendASCII("test_file.tiff"), kTestFileContent));
    CHECK(google_apis::test_util::WriteStringToFile(
        test_dir.AppendASCII("hosted_doc.gdoc"), kTestFileContent));

    const auto& drivefs_helper = fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, drivefs_dir);
    return new drive::DriveIntegrationService(
        profile, nullptr, std::string(), cache_dir,
        drivefs_helper->CreateFakeDriveFsListenerFactory());
  }

  base::ScopedTempDir tmp_dir_;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  Profile* second_profile_ = nullptr;
  std::unordered_map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

class LocalAndDriveFileSystemExtensionApiTest
    : public FileSystemExtensionApiTestBase {
 public:
  LocalAndDriveFileSystemExtensionApiTest() = default;
  ~LocalAndDriveFileSystemExtensionApiTest() override = default;

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kLocalMountPointName, &local_tmp_dir_, &local_mount_point_dir_))
        << "Failed to initialize file system.";

    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &LocalAndDriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ =
        std::make_unique<DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(
        content::BrowserContext::GetMountPoints(profile())->RegisterFileSystem(
            kLocalMountPointName, storage::kFileSystemTypeNativeLocal,
            storage::FileSystemMountOption(), local_mount_point_dir_));
    VolumeManager::Get(profile())->AddVolumeForTesting(
        local_mount_point_dir_, VOLUME_TYPE_TESTING,
        chromeos::DEVICE_TYPE_UNKNOWN, false /* read_only */);
    test_util::WaitUntilDriveMountPointIsAdded(profile());
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    // LocalAndDriveFileSystemExtensionApiTest doesn't expect that several user
    // profiles could exist simultaneously.
    base::FilePath drivefs_mount_point;
    InitializeLocalFileSystem("drive-user/root", &drivefs_root_,
                              &drivefs_mount_point);
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point.DirName());
    return new drive::DriveIntegrationService(
        profile, nullptr, "", test_cache_root_.GetPath(),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
  }

 private:
  // For local volume.
  base::ScopedTempDir local_tmp_dir_;
  base::FilePath local_mount_point_dir_;

  // For drive volume.
  base::ScopedTempDir test_cache_root_;
  base::ScopedTempDir drivefs_root_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

//
// LocalFileSystemExtensionApiTests.
//

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest,
                       DISABLED_FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/file_watcher_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest,
                       FileBrowserHandlersLazy) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler_lazy",
      FLAGS_USE_FILE_HANDLER | FLAGS_LAZY_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, DefaultFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/default_file_handler",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

//
// RestrictedFileSystemExtensionApiTests.
//
IN_PROC_BROWSER_TEST_F(RestrictedFileSystemExtensionApiTest,
                       FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

//
// DriveFileSystemExtensionApiTests.
//
// This test is flaky. See https://crbug.com/1008880.
IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest,
                       DISABLED_FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/file_watcher_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, Search) {
  // Configure the drive service to return only one search result at a time
  // to simulate paginated searches.
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/drive_search_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, RetainEntry) {
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(
      drivefs_root_.GetPath().Append("drive-user")));
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/retain_entry",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "",
                                            FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MultiProfileDriveFileSystemExtensionApiTest,
                       CrossProfileCopy) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/multi_profile_copy",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

//
// LocalAndDriveFileSystemExtensionApiTests.
IN_PROC_BROWSER_TEST_F(LocalAndDriveFileSystemExtensionApiTest,
                       AppFileHandlerMulti) {
  EXPECT_TRUE(
      RunFileSystemExtensionApiTest("file_browser/app_file_handler_multi",
                                    FILE_PATH_LITERAL("manifest.json"),
                                    "",
                                    FLAGS_NONE))
      << message_;
}
}  // namespace
}  // namespace file_manager
