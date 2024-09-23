// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/mount_test_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/common/test_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

// Tests for access to external file systems (as defined in
// storage/common/file_system/file_system_types.h) from extensions with
// fileManagerPrivate and fileBrowserHandler extension permissions.
// The tests cover following external file system types:
// - local (kFileSystemTypeLocalNative): a local file system on which files are
//   accessed using native local path.
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

// Default file content for the test files.
constexpr char kTestFileContent[] = "This is some test content.";

// User account email and directory hash for secondary account for multi-profile
// sensitive test cases.
constexpr char kSecondProfileAccount[] = "profile2@test.com";
constexpr char kSecondProfileGiaId[] = "9876543210";
constexpr char kSecondProfileHash[] = "fileBrowserApiTestProfile2";

// Waits for a WebContents of the background page of the extension under test
// to load, then injects some javascript into it to trigger a particular test.
class JSTestStarter : public content::TestNavigationObserver {
 public:
  explicit JSTestStarter(const std::string& test_name)
      : TestNavigationObserver(GetUrlToWatch()), test_name_(test_name) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  static GURL GetUrlToWatch() {
    // Use the chrome-extension:// ID corresponding to the key used in the app
    // manifests for tests in this file. An improvement to this would use the ID
    // of the extension from LoadExtensionAsComponentWithManifest(), but that's
    // potentially racy.
    return GURL(
        "chrome-extension://pkplfbidichfdicaijlchgnapepdginl/"
        "_generated_background_page.html");
  }

  // TestNavigationObserver:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // If the background page scripts have run, the test will exist, so just run
    // it. Otherwise, schedule the test to be run at the end of the background
    // page script.
    constexpr char kScript[] = R"(
        if (self.$1) {
          chrome.test.runTests([$1])
        } else {
          self.testNameToRun = '$1';
        }
    )";
    ASSERT_TRUE(content::ExecJs(
        navigation_handle->GetRenderFrameHost(),
        base::ReplaceStringPlaceholders(kScript, {test_name_}, nullptr)));

    TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

 private:
  const std::string test_name_;
};

bool TouchFile(const base::FilePath& path,
               std::string_view mtime_string,
               std::string_view atime_string) {
  base::Time mtime, atime;
  auto result =
      base::Time::FromString(std::string(mtime_string).c_str(), &mtime) &&
      base::Time::FromString(std::string(atime_string).c_str(), &atime) &&
      base::TouchFile(path, atime, mtime);
  return result;
}

// Configuration of a file residing in a "test_dir" of a created volume.
// If contents is null, creates a subdirectory.
struct TestDirConfig {
  const char* mtime;
  const char* atime;
  const char* name;
  const char* contents = kTestFileContent;
};

// An arbitrary time, for tests that don't care.
constexpr const char kArbitraryTime[] = "2011-04-03T11:11:10.000Z";

// The default configuration of entries in "test_dir" used in test harnesses.
constexpr const TestDirConfig kDefaultDirConfig[] = {
    {"2011-11-02T04:00:00.000Z", "2011-11-02T04:00:00.000Z", "empty_dir",
     nullptr},
    {"2011-04-01T18:34:08.234Z", "2012-01-02T00:00:01.000Z", "subdir", nullptr},
    {"2011-12-14T00:40:47.330Z", "2012-01-02T00:00:00.000Z", "test_file.xul"},
    {"2012-01-01T10:00:30.000Z", "2012-01-01T00:00:00.000Z",
     "test_file.xul.foo"},
    {"2011-04-03T11:11:10.000Z", "2012-01-02T00:00:00.000Z", "test_file.tiff"},
    {"2011-12-14T00:40:47.330Z", "2010-01-02T00:00:00.000Z",
     "test_file.tiff.foo"},
    {"2011-12-14T00:40:47.330Z", "2011-12-14T00:40:47.330Z", "empty_file.foo",
     ""},
};

// Sets up the initial file system state for native local file systems. The
// hierarchy is the same as for the drive file system.
// The directory is created at unique_temp_dir/|mount_point_name| path.
bool InitializeLocalFileSystem(std::string mount_point_name,
                               base::ScopedTempDir* tmp_dir,
                               base::FilePath* mount_point_dir,
                               const std::vector<TestDirConfig>& dir_contents) {
  if (!tmp_dir->CreateUniqueTempDir()) {
    return false;
  }

  *mount_point_dir = tmp_dir->GetPath().AppendASCII(mount_point_name);
  // Create the mount point.
  if (!base::CreateDirectory(*mount_point_dir)) {
    return false;
  }

  constexpr TestDirConfig kTestDir = {"2012-01-02T00:00:00.000Z",
                                      "2012-01-02T00:00:01.000Z", "test_dir",
                                      nullptr};

  const base::FilePath test_dir = mount_point_dir->AppendASCII(kTestDir.name);
  if (!base::CreateDirectory(test_dir)) {
    return false;
  }

  for (const auto& file : dir_contents) {
    const base::FilePath test_path = test_dir.AppendASCII(file.name);
    if (file.contents) {
      if (!google_apis::test_util::WriteStringToFile(test_path,
                                                     file.contents)) {
        return false;
      }
    } else {
      if (!base::CreateDirectory(test_path)) {
        return false;
      }
    }
  }

  for (const auto& file : dir_contents) {
    if (!TouchFile(test_dir.Append(file.name), file.mtime, file.atime)) {
      return false;
    }
  }

  // Touch the directory holding all the contents last.
  return TouchFile(test_dir, kTestDir.mtime, kTestDir.atime);
}

// Base class for FileSystemExtensionApi tests.
class FileSystemExtensionApiTestBase : public extensions::ExtensionApiTest {
 public:
  enum Flags {
    FLAGS_NONE = 0,
    FLAGS_USE_FILE_HANDLER = 1 << 1,
    FLAGS_LAZY_FILE_HANDLER = 1 << 2
  };

  FileSystemExtensionApiTestBase() = default;

  FileSystemExtensionApiTestBase(const FileSystemExtensionApiTestBase&) =
      delete;
  FileSystemExtensionApiTestBase& operator=(
      const FileSystemExtensionApiTestBase&) = delete;

  ~FileSystemExtensionApiTestBase() override = default;

  virtual std::vector<TestDirConfig> GetTestDirContents() {
    return {std::begin(kDefaultDirConfig), std::end(kDefaultDirConfig)};
  }

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

      if (flags & FLAGS_LAZY_FILE_HANDLER) {
        // Ensures the file handler extension's background page closes in a
        // timely manner to avoid test timeouts.
        extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
      }

      extensions::ExtensionHostTestHelper host_helper(profile());
      host_helper.RestrictToType(
          extensions::mojom::ViewType::kExtensionBackgroundPage);
      const Extension* file_handler =
          LoadExtension(test_data_dir_.AppendASCII(filehandler_path));
      if (!file_handler) {
        message_ = "Error loading file handler extension";
        return false;
      }

      if (flags & FLAGS_LAZY_FILE_HANDLER) {
        host_helper.WaitForHostDestroyed();
      } else {
        host_helper.WaitForDocumentElementAvailable();
      }
    }

    extensions::ResultCatcher catcher;

    const Extension* file_browser = LoadExtensionAsComponentWithManifest(
        test_data_dir_.AppendASCII(filebrowser_path), filebrowser_manifest);
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

  // Starts the app in |app_folder| then triggers the |test_name| on the
  // background page using JSTestStarter. Does not configure a file handler app.
  bool RunBackgroundPageTestCase(const std::string& app_folder,
                                 const std::string& test_name) {
    JSTestStarter starter(test_name);
    return RunFileSystemExtensionApiTest("file_browser/" + app_folder,
                                         FILE_PATH_LITERAL("manifest.json"),
                                         std::string(), FLAGS_NONE);
  }

 protected:
  // Sets up initial test file system hierarchy.
  virtual void InitTestFileSystem() = 0;
  // Registers mount point used in the test.
  virtual void AddTestMountPoint() = 0;

 private:
  std::unique_ptr<media_router::MockMediaRouter> media_router_;
};

// Tests for a native local file system.
class LocalFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  LocalFileSystemExtensionApiTest() = default;
  ~LocalFileSystemExtensionApiTest() override = default;

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(kLocalMountPointName, &tmp_dir_,
                                          &mount_point_dir_,
                                          GetTestDirContents()))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
        kLocalMountPointName, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), mount_point_dir_));
    VolumeManager::Get(profile())->AddVolumeForTesting(
        mount_point_dir_, VOLUME_TYPE_TESTING, ash::DeviceType::kUnknown,
        /*read_only*/ false, /*device_path*/ {}, /*drive_label*/ {},
        /*file_system_type*/ {}, /*hidden*/ false, /*watchable*/ true);
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
    create_drive_integration_service_ = base::BindRepeating(
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
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    // DriveFileSystemExtensionApiTest doesn't expect that several user profiles
    // could exist simultaneously.
    base::FilePath drivefs_mount_point;
    InitializeLocalFileSystem("drive-user/root", &drivefs_root_,
                              &drivefs_mount_point, GetTestDirContents());
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point.DirName());
    return new drive::DriveIntegrationService(
        profile, "", test_cache_root_.GetPath(),
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
class MultiProfileDriveFileSystemExtensionApiTest
    : public FileSystemExtensionApiTestBase {
 public:
  MultiProfileDriveFileSystemExtensionApiTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemExtensionApiTestBase::SetUpCommandLine(command_line);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(ash::switches::kProfileRequiresPolicy,
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
    base::FilePath profile_dir = user_data_directory.AppendASCII(
        ash::BrowserContextHelper::GetUserBrowserContextDirName(
            kSecondProfileHash));
    second_profile_ =
        g_browser_process->profile_manager()->GetProfile(profile_dir);

    FileSystemExtensionApiTestBase::SetUpOnMainThread();
  }

  void InitTestFileSystem() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ =
        base::BindRepeating(&MultiProfileDriveFileSystemExtensionApiTest::
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
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    base::FilePath cache_dir;
    base::CreateTemporaryDirInDir(tmp_dir_.GetPath(),
                                  base::FilePath::StringType(), &cache_dir);

    base::FilePath drivefs_dir;
    base::CreateTemporaryDirInDir(tmp_dir_.GetPath(),
                                  base::FilePath::StringType(), &drivefs_dir);
    auto profile_name_storage = profile->GetBaseName().value();
    std::string_view profile_name = profile_name_storage;
    if (base::StartsWith(profile_name, "u-")) {
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
        profile, std::string(), cache_dir,
        drivefs_helper->CreateFakeDriveFsListenerFactory());
  }

  base::ScopedTempDir tmp_dir_;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  raw_ptr<Profile, DanglingUntriaged> second_profile_ = nullptr;
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
    ASSERT_TRUE(InitializeLocalFileSystem(kLocalMountPointName, &local_tmp_dir_,
                                          &local_mount_point_dir_,
                                          GetTestDirContents()))
        << "Failed to initialize file system.";

    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::BindRepeating(
        &LocalAndDriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ =
        std::make_unique<DriveIntegrationServiceFactory::ScopedFactoryForTest>(
            &create_drive_integration_service_);
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
        kLocalMountPointName, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), local_mount_point_dir_));
    VolumeManager::Get(profile())->AddVolumeForTesting(
        local_mount_point_dir_, VOLUME_TYPE_TESTING, ash::DeviceType::kUnknown,
        false /* read_only */);
    test_util::WaitUntilDriveMountPointIsAdded(profile());
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (ash::IsSigninBrowserContext(profile) ||
        ash::IsLockScreenAppBrowserContext(profile)) {
      return nullptr;
    }

    // LocalAndDriveFileSystemExtensionApiTest doesn't expect that several user
    // profiles could exist simultaneously.
    base::FilePath drivefs_mount_point;
    InitializeLocalFileSystem("drive-user/root", &drivefs_root_,
                              &drivefs_mount_point, GetTestDirContents());
    fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
        profile, drivefs_mount_point.DirName());
    return new drive::DriveIntegrationService(
        profile, "", test_cache_root_.GetPath(),
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

// Mixin for starting one of the FileSystem test fixures with a specific app
// configuration, which may include default-installed apps. Currently set up
// to run with the chrome://media-app.
class FileSystemExtensionApiTestWithApps
    : public LocalFileSystemExtensionApiTest {
 public:
  FileSystemExtensionApiTestWithApps() {}

  // FileManagerPrivateApiTest:
  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile);
    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();
    LocalFileSystemExtensionApiTest::SetUpOnMainThread();
  }

  std::vector<TestDirConfig> GetTestDirContents() override {
    return {{kArbitraryTime, kArbitraryTime, "test_file.png"},
            {kArbitraryTime, kArbitraryTime, "test_file.arw"}};
  }
};

namespace {

// Constants from app_service_metrics.cc.
constexpr int kGalleryUmaBucket = 13;
constexpr int kMediaAppUmaBucket = 19;

// Metric recorded as the result of the call to apps::RecordAppLaunch().
constexpr char kAppLaunchMetric[] = "Apps.DefaultAppLaunch.FromFileManager";

}  // namespace

// Check the interception of ExecuteTask calls to replace Gallery for PNGs. The
// Media App should always be used in this case.
IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTestWithApps, OpenGalleryForPng) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(RunBackgroundPageTestCase("open_gallery",
                                        "testPngOpensGalleryReturnsOpened"))
      << message_;
  histogram_tester.ExpectBucketCount(kAppLaunchMetric, kGalleryUmaBucket, 0);
  histogram_tester.ExpectBucketCount(kAppLaunchMetric, kMediaAppUmaBucket, 1);
}

//
// LocalFileSystemExtensionApiTests.
//

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"), "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/file_watcher_test",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner", FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler", FLAGS_USE_FILE_HANDLER))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest,
                       FileBrowserHandlersLazy) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner", FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler_lazy",
      FLAGS_USE_FILE_HANDLER | FLAGS_LAZY_FILE_HANDLER))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner", FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler", FLAGS_USE_FILE_HANDLER))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, DefaultFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/default_file_handler",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

//
// DriveFileSystemExtensionApiTests.
//
// This test is flaky. See https://crbug.com/1008880.
IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest,
                       DISABLED_FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"), "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, DISABLED_FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/file_watcher_test",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner", FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler", FLAGS_USE_FILE_HANDLER))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, Search) {
  // Configure the drive service to return only one search result at a time
  // to simulate paginated searches.
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/drive_search_test",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner", FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler", FLAGS_USE_FILE_HANDLER))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, RetainEntry) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{
              drivefs_root_.GetPath().Append("drive-user/root/test_dir")}));
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/retain_entry",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MultiProfileDriveFileSystemExtensionApiTest,
                       CrossProfileCopy) {
  // IOTask only sends progress update to JS/renderer when there is a Files app
  // window open, so we need to force it to send progress in the test.
  auto* event_router =
      file_manager::EventRouterFactory::GetForProfile(profile());
  event_router->ForceBroadcastingForTesting(true);
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/multi_profile_copy",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "", FLAGS_NONE))
      << message_;
}

//
// LocalAndDriveFileSystemExtensionApiTests.
IN_PROC_BROWSER_TEST_F(LocalAndDriveFileSystemExtensionApiTest,
                       AppFileHandlerMulti) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/app_file_handler_multi", FILE_PATH_LITERAL("manifest.json"),
      "", FLAGS_NONE))
      << message_;
}
}  // namespace
}  // namespace file_manager
