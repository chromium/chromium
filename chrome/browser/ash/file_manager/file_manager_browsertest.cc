// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/immediate_crash.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_utils.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

using file_manager::test::TestCase;

namespace file_manager {
namespace {
constexpr char kOwnerEmail[] = "owner@example.com";

}  // namespace

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

  FilesAppBrowserTest(const FilesAppBrowserTest&) = delete;
  FilesAppBrowserTest& operator=(const FilesAppBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Default mode is clamshell: force Ash into tablet mode if requested,
    // and enable the Ash virtual keyboard sub-system therein.
    if (GetOptions().tablet_mode) {
      command_line->AppendSwitchASCII("force-tablet-mode", "touch_view");
      command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
    }
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  Options GetOptions() const override { return GetParam().options; }
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// `FilesAppBrowserTest` with `LoggedInUserMixin` and `DeviceStateMixin`. This
// test provides additional two options from `FilesAppBrowserTest`. Both options
// must be explicitly set for this test.
//
// - test_account_type: Account type used for a test.
// - device_mode: Status of a device, e.g. a device is enrolled.
class LoggedInUserFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  LoggedInUserFilesAppBrowserTest() {
    // ChromeOS user will be set by `LoggedInUserMixin`.
    set_chromeos_user_ = false;

    device_state_mixin_ = std::make_unique<ash::DeviceStateMixin>(
        &mixin_host_, DeviceStateFor(GetOptions().device_mode));

    logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
        &mixin_host_, LogInTypeFor(GetOptions().test_account_type),
        embedded_test_server(), this, /*should_launch_browser=*/false,
        AccountIdFor(GetOptions().test_account_type));

    // Set up owner email of a device. We set up owner email only if a device is
    // kConsumerOwned. If a device is enrolled, an account cannot be an owner of
    // a device.
    if (GetOptions().device_mode == kConsumerOwned) {
      std::string owner_email;

      switch (GetOptions().test_account_type) {
        case kTestAccountTypeNotSet:
        case kEnterprise:
        case kChild:
        case kNonManaged:
        case kGoogler:
          owner_email = logged_in_user_mixin_->GetAccountId().GetUserEmail();
          break;
        case kNonManagedNonOwner:
          owner_email = kOwnerEmail;
          break;
      }

      scoped_testing_cros_settings_.device_settings()->Set(
          ash::kDeviceOwner, base::Value(owner_email));
    }
  }

  void SetUpOnMainThread() override {
    logged_in_user_mixin_->LogInUser();
    FilesAppBrowserTest::SetUpOnMainThread();
  }

  AccountId GetAccountId() override {
    return logged_in_user_mixin_->GetAccountId();
  }

 private:
  ash::DeviceStateMixin::State DeviceStateFor(DeviceMode device_mode) {
    switch (device_mode) {
      case kDeviceModeNotSet:
        CHECK(false) << "device_mode option must be set for "
                        "LoggedInUserFilesAppBrowserTest";
        // TODO(crbug.com/1061742): `base::ImmediateCrash` is necessary.
        base::ImmediateCrash();
      case kConsumerOwned:
        return ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED;
      case kEnrolled:
        return ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED;
    }
  }

  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
  std::unique_ptr<ash::DeviceStateMixin> device_state_mixin_;

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(LoggedInUserFilesAppBrowserTest, Test) {
  StartTest();
}

// A version of the FilesAppBrowserTest that supports spanning browser restart
// to allow testing prefs and other things.
class ExtendedFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  ExtendedFilesAppBrowserTest() = default;

  ExtendedFilesAppBrowserTest(const ExtendedFilesAppBrowserTest&) = delete;
  ExtendedFilesAppBrowserTest& operator=(const ExtendedFilesAppBrowserTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, PRE_Test) {
  profile()->GetPrefs()->SetBoolean(prefs::kNetworkFileSharesAllowed,
                                    GetOptions().native_smb);
}

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, Test) {
  StartTest();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
class QuickOfficeBrowserTestBase : public InProcessBrowserTest {
 public:
  QuickOfficeBrowserTestBase() = default;
  ~QuickOfficeBrowserTestBase() override = default;

  QuickOfficeBrowserTestBase(const QuickOfficeBrowserTestBase&) = delete;
  QuickOfficeBrowserTestBase& operator=(const QuickOfficeBrowserTestBase&) =
      delete;

 protected:
  // extensions::ExtensionApiTest:
  void SetUpOnMainThread() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    service->component_loader()->AddDefaultComponentExtensions(false);

    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class QuickOfficeForceFileDownloadEnabledBrowserTest
    : public QuickOfficeBrowserTestBase {
 public:
  QuickOfficeForceFileDownloadEnabledBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kQuickOfficeForceFileDownload);
  }
  ~QuickOfficeForceFileDownloadEnabledBrowserTest() override = default;

  QuickOfficeForceFileDownloadEnabledBrowserTest(
      const QuickOfficeForceFileDownloadEnabledBrowserTest&) = delete;
  QuickOfficeForceFileDownloadEnabledBrowserTest& operator=(
      const QuickOfficeForceFileDownloadEnabledBrowserTest&) = delete;
};

class QuickOfficeForceFileDownloadDisabledBrowserTest
    : public QuickOfficeBrowserTestBase {
 public:
  QuickOfficeForceFileDownloadDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kQuickOfficeForceFileDownload);
  }
  ~QuickOfficeForceFileDownloadDisabledBrowserTest() override = default;

  QuickOfficeForceFileDownloadDisabledBrowserTest(
      const QuickOfficeForceFileDownloadDisabledBrowserTest&) = delete;
  QuickOfficeForceFileDownloadDisabledBrowserTest& operator=(
      const QuickOfficeForceFileDownloadDisabledBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(QuickOfficeForceFileDownloadEnabledBrowserTest,
                       OfficeDocumentsAreDownloaded) {
  using download::DownloadItem;

  GURL download_url =
      embedded_test_server()->GetURL("/chromeos/file_manager/text.docx");

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverTerminal(
          download_manager, /*num_downloads=*/1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // This call will not wait for the download to finish.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), download_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Wait for the download itself to complete.
  download_observer->WaitForFinished();
  EXPECT_EQ(1u,
            download_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  std::vector<DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  DownloadItem* download = downloads[0];

  download->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(QuickOfficeForceFileDownloadDisabledBrowserTest,
                       OfficeDocumentsAreNotDownloaded) {
  using download::DownloadItem;

  GURL download_url =
      embedded_test_server()->GetURL("/chromeos/file_manager/text.docx");

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverTerminal(
          download_manager, /*num_downloads=*/1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // This call will block until the condition X, but will not wait for the
  // download to finish.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), download_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(0u, download_observer->NumDownloadsSeenInState(
                    DownloadItem::IN_PROGRESS));
  EXPECT_EQ(0u,
            download_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  std::vector<DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayLaunchOnDrive")
            .DontObserveFileTasks()
            .NewDirectoryTree(),
        TestCase("fileDisplayComputers").NewDirectoryTree(),
        TestCase("fileDisplayMtp")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9",
                         "screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayUsb")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        // TODO(b/301341566): enable the tests
        TestCase("fileDisplayUsbPartition").NewDirectoryTree(),
        TestCase("fileDisplayUsbPartition")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("fileDisplayUsbPartitionSort").NewDirectoryTree(),
        TestCase("fileDisplayPartitionFileTable").NewDirectoryTree(),
        // TestCase("fileDisplayWithoutVolumesThenMountDownloads")
        //     .DontMountVolumes()
        //     .NewDirectoryTree(),
        // TestCase("fileDisplayWithoutVolumesThenMountDrive")
        //     .DontMountVolumes()
        //     .NewDirectoryTree(),
        // TestCase("fileDisplayWithoutDrive")
        //     .DontMountVolumes()
        //     .NewDirectoryTree(),
        TestCase("fileDisplayWithHiddenVolume").NewDirectoryTree(),
        TestCase("fileDisplayMountWithFakeItemSelected").NewDirectoryTree(),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected")
            .NewDirectoryTree(),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory")
            .NewDirectoryTree(),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles")
            .NewDirectoryTree(),
        TestCase("fileDisplayCheckNoReadOnlyIconOnGuestOs").NewDirectoryTree(),
        TestCase("fileDisplayUnmountRemovableRoot").NewDirectoryTree(),
        TestCase("fileDisplayUnmountFirstPartition").NewDirectoryTree(),
        TestCase("fileDisplayUnmountLastPartition").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("fileDisplayDownloads")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDownloads")
            .TabletMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFile").DontObserveFileTasks(),
        TestCase("fileDisplayDrive")
            .TabletMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDrive")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayDriveOnlineNewWindow").DontObserveFileTasks(),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp")
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9",
                         "screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayUsb")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartition").EnableSinglePartitionFormat(),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes(),
        // Test is failing (crbug.com/1097013)
        // TestCase("fileDisplayWithoutDriveThenDisable").DontMountVolumes(),
        TestCase("fileDisplayWithHiddenVolume"),
        TestCase("fileDisplayMountWithFakeItemSelected"),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected"),
        TestCase("fileDisplayUnmountRemovableRoot"),
        TestCase("fileDisplayUnmountFirstPartition"),
        TestCase("fileDisplayUnmountLastPartition"),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchNotFound"),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner"),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected"),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnGuestOs")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoMediaApp, /* open_video_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioMediaApp, /* open_audio_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenMediaAppDownloads").InGuestMode(),
                      TestCase("imageOpenMediaAppDownloads"),
                      TestCase("imageOpenMediaAppDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive"),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenFilesInWebDrive, /* open_files_in_web_drive.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("hostedHasDefaultTask"),
                      TestCase("encryptedHasDefaultTask"),
                      TestCase("hostedOpenDrive"),
                      TestCase("encryptedHostedOpenDrive"),
                      TestCase("encryptedNonHostedOpenDrive").Offline(),
                      TestCase("encryptedNonHostedOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("zipFileOpenUsb").NewDirectoryTree(),
        TestCase("zipCreateFileUsb").NewDirectoryTree(),
        TestCase("zipExtractFromReadOnly").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("zipFileOpenDownloads"),
        TestCase("zipFileOpenDownloads").InGuestMode(),
        TestCase("zipFileOpenDrive"),
        TestCase("zipFileOpenUsb"),
        TestCase("zipNotifyFileTasks"),
        TestCase("zipCreateFileDownloads"),
        TestCase("zipCreateFileDownloads").InGuestMode(),
        TestCase("zipCreateFileDrive"),
        TestCase("zipCreateFileDriveOffice"),
        TestCase("zipCreateFileUsb"),
        TestCase("zipDoesntCreateFileEncrypted"),
        TestCase("zipExtractA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("zipExtractCheckContent"),
        TestCase("zipExtractCheckDuplicates"),
        TestCase("zipExtractCheckEncodings"),
        TestCase("zipExtractNotEnoughSpace"),
        TestCase("zipExtractFromReadOnly"),
        TestCase("zipExtractShowPanel"),
        TestCase("zipExtractShowMultiPanel"),
        TestCase("zipExtractSelectionMenus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("selectCreateFolderDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("selectCreateFolderDownloads")
            .InGuestMode()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("createFolderDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDownloads")
            .InGuestMode()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderNestedDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),

        TestCase("selectCreateFolderDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("selectCreateFolderDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("createFolderDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderNestedDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDrive")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(
        // TODO(b/307657529): enable the test
        // TestCase("renameRemovableWithKeyboardOnFileList").NewDirectoryTree(),
        TestCase("keyboardSelectDriveDirectoryTree").NewDirectoryTree(),
        TestCase("keyboardDeleteFolderDownloads").NewDirectoryTree(),
        TestCase("keyboardDeleteFolderDownloads")
            .InGuestMode()
            .NewDirectoryTree(),
        TestCase("keyboardDeleteFolderDrive").NewDirectoryTree(),
        TestCase("renameNewFolderDownloads").NewDirectoryTree(),
        TestCase("renameNewFolderDownloads").InGuestMode().NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("keyboardDeleteDownloads").InGuestMode(),
        TestCase("keyboardDeleteDownloads"),
        TestCase("keyboardDeleteDrive"),
        TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
        TestCase("keyboardDeleteFolderDownloads"),
        TestCase("keyboardDeleteFolderDrive"),
        TestCase("keyboardCopyDownloads").InGuestMode(),
        TestCase("keyboardCopyDownloads"),
        TestCase("keyboardCopyDownloads").EnableConflictDialog(),
        TestCase("keyboardCopyDrive"),
        TestCase("keyboardCopyDrive").EnableConflictDialog(),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("keyboardFocusOutlineVisible"),
        TestCase("keyboardFocusOutlineVisibleMouse"),
#endif
        TestCase("keyboardSelectDriveDirectoryTree"),
        TestCase("keyboardDisableCopyWhenDialogDisplayed"),
        TestCase("keyboardOpenNewWindow"),
        TestCase("keyboardOpenNewWindow").InGuestMode(),
        TestCase("noPointerActiveOnTouch"),
        TestCase("pointerActiveRemovedByTouch"),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDrive"),
        TestCase("renameNewFolderDownloads"),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameRemovableWithKeyboardOnFileList")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkNewFolderEnabledInsideReadWriteFolder")
            .NewDirectoryTree(),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder")
            .NewDirectoryTree(),
        TestCase("checkPasteEnabledInsideReadWriteFolder").NewDirectoryTree(),
        TestCase("checkPasteDisabledInsideReadOnlyFolder").NewDirectoryTree(),
        TestCase("checkDownloadsContextMenu").NewDirectoryTree(),
        TestCase("checkPlayFilesContextMenu").NewDirectoryTree(),
        TestCase("checkLinuxFilesContextMenu").NewDirectoryTree(),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("checkDeleteEnabledInRecents").NewDirectoryTree(),
        TestCase("checkGoToFileLocationEnabledInRecents").NewDirectoryTree(),
// TODO(https://crbug.com/1425820): Fix flakes and re-enable.
#if !BUILDFLAG(IS_CHROMEOS)
        TestCase("checkGoToFileLocationDisabledInMultipleSelection")
            .NewDirectoryTree(),
#endif
        TestCase("checkEncryptedCrossVolumeMoveDisabled").NewDirectoryTree(),
        TestCase("checkEncryptedMoveEnabled").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("checkDeleteEnabledForReadWriteFile"),
        TestCase("checkDeleteDisabledForReadOnlyDocument"),
        TestCase("checkDeleteDisabledForReadOnlyFile"),
        TestCase("checkDeleteDisabledForReadOnlyFolder"),
        TestCase("checkRenameEnabledForReadWriteFile"),
        TestCase("checkRenameDisabledForReadOnlyDocument"),
        TestCase("checkRenameDisabledForReadOnlyFile"),
        TestCase("checkRenameDisabledForReadOnlyFolder"),
        TestCase("checkContextMenuForRenameInput"),
        TestCase("checkShareEnabledForReadWriteFile"),
        TestCase("checkShareEnabledForReadOnlyDocument"),
        TestCase("checkShareDisabledForStrictReadOnlyDocument"),
        TestCase("checkShareEnabledForReadOnlyFile"),
        TestCase("checkShareEnabledForReadOnlyFolder"),
        TestCase("checkCopyEnabledForReadWriteFile"),
        TestCase("checkCopyEnabledForReadOnlyDocument"),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
        TestCase("checkCopyEnabledForReadOnlyFile"),
        TestCase("checkCopyEnabledForReadOnlyFolder"),
        TestCase("checkCutEnabledForReadWriteFile"),
        TestCase("checkCutDisabledForReadOnlyDocument"),
        TestCase("checkCutDisabledForReadOnlyFile"),
        TestCase("checkDlpRestrictionDetailsDisabledForNonDlpFiles"),
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        // TODO(b/189173190): Enable
        // TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkImportCrostiniImageEnabled"),
        // TODO(b/189173190): Enable
        // TestCase("checkImportCrostiniImageDisabled"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkDeleteEnabledInRecents"),
        TestCase("checkGoToFileLocationEnabledInRecents"),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection"),
        TestCase("checkDefaultTask"),
        TestCase("checkPolicyAssignedDefaultHasManagedIcon"),
        TestCase("checkEncryptedCopyDisabled"),
        TestCase("checkEncryptedCrossVolumeMoveDisabled"),
        TestCase("checkEncryptedMoveEnabled")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Share, /* share.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("checkEncryptedSharesheetOptions")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("toolbarDeleteButtonKeepFocus").NewDirectoryTree(),
        TestCase("toolbarRefreshButtonWithSelection")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("toolbarRefreshButtonHiddenInRecents").NewDirectoryTree(),
        TestCase("toolbarRefreshButtonShownForNonWatchableVolume")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("toolbarAltACommand"),
        TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
        TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
        TestCase("toolbarDeleteButtonKeepFocus"),
        TestCase("toolbarDeleteEntry"),
        TestCase("toolbarDeleteEntry").InGuestMode(),
        TestCase("toolbarMultiMenuFollowsButton"),
        TestCase("toolbarRefreshButtonHiddenInRecents"),
        TestCase("toolbarRefreshButtonHiddenForWatchableVolume"),
        TestCase("toolbarRefreshButtonShownForNonWatchableVolume")
            .EnableGenericDocumentsProvider(),
        TestCase("toolbarRefreshButtonWithSelection")
            .EnableGenericDocumentsProvider(),
        TestCase("toolbarSharesheetButtonWithSelection")
            .FeatureIds({"screenplay-195b5b1d-2f7f-45ae-be5c-18b9c5d17674",
                         "screenplay-54b29b90-e689-4745-af0d-f8d336be2d13"}),
        TestCase("toolbarSharesheetContextMenuWithSelection"),
        TestCase("toolbarSharesheetNoEntrySelected"),
        TestCase("toolbarCloudIconShouldNotShowWhenBulkPinningDisabled"),
        TestCase("toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIState"
                 "Available")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowForInProgress").EnableBulkPinning(),
        TestCase("toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconWhenPressedShouldOpenCloudPanel")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldNotShowWhenPrefDisabled")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowOnStartupEvenIfSyncing")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowWhenPausedState")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowWhenOnMeteredNetwork")
            .EnableBulkPinning()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickViewSmbfs").NewDirectoryTree(),
        TestCase("openQuickViewRemovablePartitions").NewDirectoryTree(),
        TestCase("openQuickViewTrash")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("openQuickViewMtp").NewDirectoryTree(),
        TestCase("openQuickViewAndroid").NewDirectoryTree(),
        TestCase("openQuickViewAndroidGuestOs")
            .EnableArcVm()
            .NewDirectoryTree(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("openQuickViewFromDirectoryTree").NewDirectoryTree(),
        TestCase("openQuickViewUsb").NewDirectoryTree(),
        TestCase("openQuickViewTabIndexDeleteDialog")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewGuestOs").NewDirectoryTree(),
        TestCase("openQuickViewCrostini").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("openQuickView"),
        TestCase("openQuickViewDialog"),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickViewViaContextMenuSingleSelection"),
        TestCase("openQuickViewViaContextMenuCheckSelections"),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewAudioOnDrive"),
        TestCase("openQuickViewAudioWithImageMetadata"),
        TestCase("openQuickViewImageJpg"),
        TestCase("openQuickViewImageJpeg"),
        TestCase("openQuickViewImageJpeg").InGuestMode(),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewImageRawWithOrientation"),
        TestCase("openQuickViewImageWebp"),
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdfPopup"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdfPreviewsDisabled"),
#endif
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewTextFileWithUnknownMimeType"),
        TestCase("openQuickViewUtf8Text"),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewMhtml"),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewDrive"),
        TestCase("openQuickViewSmbfs"),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewAndroidGuestOs").EnableArcVm(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewGuestOs"),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewTrash")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewTabIndexImage"),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexHtml"),
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexVideo"),
        TestCase("openQuickViewTabIndexDeleteDialog")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewAndDeleteCheckSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewDeleteEntireCheckSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewClickDeleteButton")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("openQuickViewEncryptedFile"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory").NewDirectoryTree(),
        TestCase("directoryTreeSelectedDirectory").NewDirectoryTree(),
        TestCase("directoryTreeHorizontalScroll").NewDirectoryTree(),
        TestCase("directoryTreeExpandHorizontalScroll").NewDirectoryTree(),
        TestCase("directoryTreeExpandHorizontalScrollRTL").NewDirectoryTree(),
        TestCase("directoryTreeVerticalScroll").NewDirectoryTree(),
        TestCase("directoryTreeExpandFolder").NewDirectoryTree(),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff")
            .NewDirectoryTree(),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn")
            .NewDirectoryTree(),
        TestCase("directoryTreeExpandFolderOnNonDelayExpansionVolume")
            .NewDirectoryTree(),
        TestCase("directoryTreeExpandFolderOnDelayExpansionVolume")
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("directoryTreeActiveDirectory"),
        TestCase("directoryTreeSelectedDirectory"),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeExpandFolder"),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff"),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn"),
        TestCase("directoryTreeExpandFolderOnNonDelayExpansionVolume"),
        TestCase("directoryTreeExpandFolderOnDelayExpansionVolume")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode().NewDirectoryTree(),
        TestCase("dirCopyWithContextMenu").NewDirectoryTree(),
        TestCase("dirCopyWithKeyboard").InGuestMode().NewDirectoryTree(),
        TestCase("dirCopyWithKeyboard").NewDirectoryTree(),
        TestCase("dirCopyWithoutChangingCurrent").NewDirectoryTree(),
        TestCase("dirPasteWithContextMenu").NewDirectoryTree(),
        TestCase("dirPasteWithContextMenu").InGuestMode().NewDirectoryTree(),
        TestCase("dirPasteWithoutChangingCurrent").NewDirectoryTree(),
        TestCase("dirRenameWithContextMenu").NewDirectoryTree(),
        TestCase("dirRenameWithContextMenu").InGuestMode().NewDirectoryTree(),
        TestCase("dirRenameUpdateChildrenBreadcrumbs").NewDirectoryTree(),
        TestCase("dirRenameWithKeyboard").NewDirectoryTree(),
        TestCase("dirRenameWithKeyboard").InGuestMode().NewDirectoryTree(),
        TestCase("dirRenameWithoutChangingCurrent").NewDirectoryTree(),
        TestCase("dirRenameToEmptyString").NewDirectoryTree(),
        TestCase("dirRenameToEmptyString").InGuestMode().NewDirectoryTree(),
        TestCase("dirRenameToExisting").NewDirectoryTree(),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode().NewDirectoryTree(),
#endif
        TestCase("dirRenameRemovableWithKeyboard").NewDirectoryTree(),
        TestCase("dirRenameRemovableWithKeyboard")
            .InGuestMode()
            .NewDirectoryTree(),
        TestCase("dirRenameRemovableWithContentMenu").NewDirectoryTree(),
        TestCase("dirRenameRemovableWithContentMenu")
            .InGuestMode()
            .NewDirectoryTree(),
        TestCase("dirContextMenuForRenameInput").NewDirectoryTree(),
        TestCase("dirCreateWithContextMenu").NewDirectoryTree(),
        TestCase("dirCreateWithKeyboard").NewDirectoryTree(),
        TestCase("dirCreateWithoutChangingCurrent").NewDirectoryTree(),
        TestCase("dirContextMenuZip").NewDirectoryTree(),
        TestCase("dirContextMenuZipEject").NewDirectoryTree(),
        TestCase("dirContextMenuRecent").NewDirectoryTree(),
        TestCase("dirContextMenuMyFiles").NewDirectoryTree(),
        TestCase("dirContextMenuMyFilesWithPaste").NewDirectoryTree(),
        TestCase("dirContextMenuCrostini").NewDirectoryTree(),
        TestCase("dirContextMenuPlayFiles").NewDirectoryTree(),
        TestCase("dirContextMenuUsbs").NewDirectoryTree(),
        TestCase("dirContextMenuUsbs")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("dirContextMenuFsp").NewDirectoryTree(),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("dirContextMenuUsbDcim").NewDirectoryTree(),
        TestCase("dirContextMenuUsbDcim")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("dirContextMenuMtp").NewDirectoryTree(),
        TestCase("dirContextMenuMyDrive").NewDirectoryTree(),
        TestCase("dirContextMenuSharedDrive").NewDirectoryTree(),
        TestCase("dirContextMenuSharedWithMe").NewDirectoryTree(),
        TestCase("dirContextMenuOffline").NewDirectoryTree(),
        // TODO(b/301340154): should Computers allow rename?
        // TestCase("dirContextMenuComputers").NewDirectoryTree(),
        TestCase("dirContextMenuTrash").NewDirectoryTree(),
        TestCase("dirContextMenuShortcut").NewDirectoryTree(),
        TestCase("dirContextMenuFocus").NewDirectoryTree(),
        TestCase("dirContextMenuKeyboardNavigation").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu").InGuestMode(),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToExisting"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode(),
#endif
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
        // TODO(http://crbug.com/1480973): Enable
        // TestCase("dirCreateMultipleFolders"),
        TestCase("dirContextMenuZip"),
        TestCase("dirContextMenuZipEject"),
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuUsbDcim").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuTrash"),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus"),
        TestCase("dirContextMenuKeyboardNavigation")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("driveOpenSidebarSharedWithMe").NewDirectoryTree(),
        TestCase("drivePinToggleUpdatesInFakeEntries").NewDirectoryTree(),
        TestCase("drivePinToggleUpdatesInFakeEntries")
            .EnableCrosComponents()
            .NewDirectoryTree(),
        TestCase("drivePressClearSearch").NewDirectoryTree(),
        TestCase("driveAvailableOfflineActionBar").NewDirectoryTree(),
        TestCase("driveAvailableOfflineActionBar")
            .EnableCrosComponents()
            .NewDirectoryTree(),
        TestCase("driveWelcomeBanner").NewDirectoryTree(),
        TestCase("driveWelcomeBanner")
            .EnableCrosComponents()
            .NewDirectoryTree(),
        TestCase("driveOfflineInfoBanner").NewDirectoryTree(),
        TestCase("driveInlineSyncStatusParentFolderProgressEvents")
            .EnableInlineSyncStatusProgressEvents()
            .NewDirectoryTree(),
        TestCase("driveFoldersRetainPinnedPropertyWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .NewDirectoryTree(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .NewDirectoryTree(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .EnableCrosComponents()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("driveOpenSidebarOffline").EnableGenericDocumentsProvider(),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("drivePinMultiple"),
        TestCase("drivePinHosted"),
        // TODO(b/296960734): Enable
        // TestCase("drivePinFileMobileNetwork"),
        TestCase("drivePinToggleUpdatesInFakeEntries"),
        TestCase("drivePinToggleUpdatesInFakeEntries").EnableCrosComponents(),
        TestCase("drivePinToggleIsDisabledAndHiddenWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsDisabledAndHiddenWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .EnableCrosComponents(),
        TestCase("drivePressEnterToSearch"),
        TestCase("drivePressClearSearch"),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveAvailableOfflineActionBar").EnableCrosComponents(),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkToDirectory").EnableDriveShortcuts(),
        TestCase("driveLinkOpenFileThroughLinkedDirectory"),
        TestCase("driveLinkOpenFileThroughTransitiveLink"),
        TestCase("driveWelcomeBanner"),
        TestCase("driveWelcomeBanner").EnableCrosComponents(),
        TestCase("driveOfflineInfoBanner"),
        TestCase("driveEncryptionBadge"),
        TestCase("driveDeleteDialogDoesntMentionPermanentDelete"),
        TestCase("driveInlineSyncStatusSingleFile").EnableInlineSyncStatus(),
        TestCase("driveInlineSyncStatusParentFolder").EnableInlineSyncStatus(),
        TestCase("driveInlineSyncStatusSingleFileProgressEvents")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveInlineSyncStatusParentFolderProgressEvents")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveFoldersRetainPinnedPropertyWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .EnableCrosComponents(),
        TestCase("driveCantPinItemsShouldHaveClassNameAndGetUpdatedWhenCanPin")
            .EnableBulkPinning(),
        TestCase("driveItemsOutOfViewportShouldUpdateTheirSyncStatus")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveAllItemsShouldBeQueuedIfTrackedByPinningManager")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveDirtyItemsShouldBeDisplayedAsQueued")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("openDriveDocWhenOffline")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("completedSyncStatusDismissesAfter300Ms")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveOutOfOrganizationSpaceBanner")
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialog"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBanner"),
        TestCase("holdingSpaceWelcomeBanner").EnableCrosComponents(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferDragDropActiveLeave")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragDropActiveDrop")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragDropTreeItemDenies")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragAndHoverTreeItemEntryList")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragAndDrop")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndDropFolder")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHover")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDriveToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferOfficeFileFromDriveToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFilesMove")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFiles")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolder")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolderMove")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedFolderToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDriveToTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferHostedFileFromTeamDriveToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferBetweenTeamDrives")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDownloads")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        // Section end - browser tests for new directory tree
        TestCase("transferFromDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferOfficeFileFromDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFiles")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFilesMove")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolder")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolderMove")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedFolderToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDriveToTeamDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferHostedFileFromTeamDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToTeamDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferBetweenTeamDrives")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragDropActiveLeave")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragDropActiveDrop")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragDropTreeItemDenies")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragAndHoverTreeItemEntryList")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragFileListItemSelects")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndDrop")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndDropFolder")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHover")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDropBrowserFile")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDeletedFile")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        // TODO(b/189173190): Enable
        // TestCase("transferInfoIsRemembered"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferToUsbHasDestinationText"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferDismissedErrorIsRemembered"),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferUpdateSamePanelItem")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferShowPreparingMessageForZeroRemainingTime")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    LoggedInUserFilesAppBrowserTest,
    ::testing::Values(
        // Google One offer banner checks device state. Device state is NOT set
        // to `policy::DeviceMode::DEVICE_MODE_CONSUMER` in
        // `FilesAppBrowserTest`.
        TestCase("driveGoogleOneOfferBannerEnabled")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner(),
        // Google One offer banner is disabled by default.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged),
        TestCase("driveGoogleOneOfferBannerDismiss")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner(),
        TestCase("driveGoogleOneOfferBannerDismiss")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner()
            .EnableCrosComponents(),
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kEnterprise),
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kChild),
        // Google One offer is for a device. The banner will not
        // be shown for an enrolled device.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kNonManaged),
        // We do not show a banner if a profile is not an owner profile.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(kConsumerOwned)
            .SetTestAccountType(kNonManagedNonOwner),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kGoogler)
            .FeatureIds({"screenplay-e9165a4d-39d6-406c-9027-f2ad39bb4aeb"}),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kGoogler)
            .EnableCrosComponents()
            .FeatureIds({"screenplay-e9165a4d-39d6-406c-9027-f2ad39bb4aeb"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("shareDirectoryTeamDrive").NewDirectoryTree(),
        TestCase("shareFileDrive").NewDirectoryTree(),
        TestCase("shareDirectoryDrive").NewDirectoryTree(),
        TestCase("shareHostedFileDrive").NewDirectoryTree(),
        TestCase("shareFileTeamDrive").NewDirectoryTree(),
        TestCase("shareHostedFileTeamDrive").NewDirectoryTree(),
        TestCase("shareTeamDrive").NewDirectoryTree(),
        TestCase("manageFileDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageHostedFileDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageFileTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageHostedFileTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageTeamDrive")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        // Section end - browser tests for new directory tree
        TestCase("shareFileDrive"),
        TestCase("shareDirectoryDrive"),
        TestCase("shareHostedFileDrive"),
        TestCase("manageHostedFileDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageFileDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("shareFileTeamDrive"),
        TestCase("shareDirectoryTeamDrive"),
        TestCase("shareHostedFileTeamDrive"),
        TestCase("shareTeamDrive"),
        TestCase("manageHostedFileTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageFileTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeViaDblClick").NewDirectoryTree(),
                      // Section end - browser tests for new directory tree
                      TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive"),
                      TestCase("defaultTaskForPdf"),
                      TestCase("defaultTaskForTextPlain"),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive"),
                      TestCase("changeDefaultDialogScrollList"),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask"),
                      TestCase("executeViaDblClick"),
                      TestCase("noActionBarOpenForDirectories")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("traverseFolderShortcuts")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-42c556fb-303c-45b2-910b-3ecc5ec71b92"}),
        // TODO(b/307656817): enable the test
        // TestCase("addRemoveFolderShortcuts")
        //     .NewDirectoryTree()
        //     .FeatureIds({"screenplay-1ae94bd0-60a7-4bb9-925d-78312d7c045d"}),
        // Section end - browser tests for new directory tree
        TestCase("traverseFolderShortcuts")
            .FeatureIds({"screenplay-42c556fb-303c-45b2-910b-3ecc5ec71b92"}),
        TestCase("addRemoveFolderShortcuts")
            .FeatureIds({"screenplay-1ae94bd0-60a7-4bb9-925d-78312d7c045d"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus").NewDirectoryTree(),
        // TODO(b/307657533): enable the tests
        // TestCase("tabindexFocus").NewDirectoryTree(),
        // TestCase("tabindexFocus").EnableCrosComponents().NewDirectoryTree(),
        // TestCase("tabindexFocusDownloads").NewDirectoryTree(),
        // TestCase("tabindexFocusDownloads")
        //     .EnableCrosComponents()
        //     .NewDirectoryTree(),
        // TestCase("tabindexFocusDownloads").InGuestMode().NewDirectoryTree(),
        // TestCase("tabindexFocusDirectorySelected").NewDirectoryTree(),
        // TestCase("tabindexFocusDirectorySelected")
        //     .EnableCrosComponents()
        //     .NewDirectoryTree(),
        // TestCase("tabindexOpenDialogDownloads")
        //     .WithBrowser()
        //     .NewDirectoryTree(),
        // TestCase("tabindexOpenDialogDownloads")
        //     .WithBrowser()
        //     .EnableCrosComponents()
        //     .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocus").EnableCrosComponents(),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").EnableCrosComponents(),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusDirectorySelected"),
        TestCase("tabindexFocusDirectorySelected").EnableCrosComponents(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .EnableCrosComponents()
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        //      ,
        //      TestCase("tabindexSaveFileDialogDrive").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser().InGuestMode()
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openMultiFileDialogDriveOfficeFile")
            .WithBrowser()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-3337ab4d-3c77-4908-a9ec-e43d2f52cd1f"}),
        TestCase("openFileDialogFileListShowContextMenu")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogGuestOs").WithBrowser().NewDirectoryTree(),
// TODO(crbug.com/1425820): Re-enable this test.
#if !defined(LEAK_SANITIZER) || !BUILDFLAG(IS_CHROMEOS) || \
    !defined(ADDRESS_SANITIZER)
        TestCase("openFileDialogGuestOs")
            .WithBrowser()
            .InIncognito()
            .NewDirectoryTree(),
#endif
        TestCase("saveFileDialogGuestOs").WithBrowser().NewDirectoryTree(),
// TODO(crbug.com/1425820): Re-enable this test.
#if !BUILDFLAG(IS_CHROMEOS)
        TestCase("saveFileDialogGuestOs")
            .WithBrowser()
            .InIncognito()
            .NewDirectoryTree(),
#endif
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .InIncognito()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveFromBrowser")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveHostedDoc")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveEncryptedFile")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveOfficeFile")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .InIncognito()
            .NewDirectoryTree(),
        TestCase("saveFileDialogDrive")
            .WithBrowser()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDrive")
            .WithBrowser()
            .InIncognito()
            .NewDirectoryTree(),
        TestCase("saveFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .NewDirectoryTree(),
        TestCase("saveFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveHostedNeedsFile")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("saveFileDialogDriveHostedNeedsFile")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveCSENeedsFile")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogDriveCSEGrey").WithBrowser().NewDirectoryTree(),
        TestCase("openFileDialogCancelDownloads")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogCancelDrive").WithBrowser().NewDirectoryTree(),
        TestCase("openFileDialogEscapeDownloads")
            .WithBrowser()
            .NewDirectoryTree(),
        TestCase("openFileDialogEscapeDrive").WithBrowser().NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        // TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        // TestCase("openFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogAriaMultipleSelect")
            .WithBrowser()
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("saveFileDialogAriaSingleSelect")
            .WithBrowser()
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDownloads").WithBrowser().InIncognito(),
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        // TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDrive")
            .WithBrowser()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        TestCase("openFileDialogDriveEncryptedFile").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveCSEGrey").WithBrowser(),
        TestCase("openFileDialogDriveCSENeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveOfficeFile").WithBrowser(),
        TestCase("openMultiFileDialogDriveOfficeFile")
            .WithBrowser()
            .FeatureIds({"screenplay-3337ab4d-3c77-4908-a9ec-e43d2f52cd1f"}),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter")
            .WithBrowser()
            .FeatureIds({"screenplay-29790711-ea7b-4ad2-9596-83b98edbca8d"}),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilterKeyNavigation").WithBrowser(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled")
            .WithBrowser()
            .FeatureIds({"screenplay-3337ab4d-3c77-4908-a9ec-e43d2f52cd1f"}),
        TestCase("saveFileDialogGuestOs").WithBrowser(),
        TestCase("saveFileDialogGuestOs").WithBrowser().InIncognito(),
        TestCase("openFileDialogGuestOs").WithBrowser(),
        TestCase("openFileDialogGuestOs").WithBrowser().InIncognito()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("copyBetweenWindowsLocalToUsb").NewDirectoryTree(),
        TestCase("copyBetweenWindowsUsbToLocal").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("copyBetweenWindowsLocalToDrive"),
        TestCase("copyBetweenWindowsLocalToUsb"),
        // TODO(b/189173190): Enable
        // TestCase("copyBetweenWindowsUsbToDrive"),
        TestCase("copyBetweenWindowsDriveToLocal"),
        // TODO(b/189173190): Enable
        // TestCase("copyBetweenWindowsDriveToUsb"),
        TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showGridViewDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("showGridViewDownloads").InGuestMode(),
        TestCase("showGridViewDownloads"),
        TestCase("showGridViewButtonSwitches"),
        TestCase("showGridViewKeyboardSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("showGridViewTitles"),
        TestCase("showGridViewMouseSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("showGridViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("showGridViewEncryptedFile")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(
        TestCase("providerEject").NewDirectoryTree(),
        TestCase("providerEject").DisableNativeSmb().NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("requestMount"),
        TestCase("requestMount").DisableNativeSmb(),
        TestCase("requestMountMultipleMounts"),
        TestCase("requestMountMultipleMounts").DisableNativeSmb(),
        TestCase("requestMountSourceDevice"),
        TestCase("requestMountSourceDevice").DisableNativeSmb(),
        TestCase("requestMountSourceFile"),
        TestCase("requestMountSourceFile").DisableNativeSmb(),
        TestCase("providerEject"),
        TestCase("providerEject").DisableNativeSmb()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles")
            .NewDirectoryTree(),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders")
            .NewDirectoryTree(),
        TestCase("newFolderInDownloads").NewDirectoryTree(),
        TestCase("enableDisableStorageSettingsLink").NewDirectoryTree(),
        TestCase("showAvailableStorageSmbfs")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"})
            .NewDirectoryTree(),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot").NewDirectoryTree(),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot")
            .EnableMirrorSync()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("showHiddenFilesDownloads")
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showHiddenFilesDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showHiddenFilesDrive")
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles")
            .FeatureIds({"screenplay-09c32d6b-36d3-494b-bb83-e19655880471"}),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showFilesSettingsButton"),
        TestCase("showSendFeedbackAction")
            .FeatureIds({"screenplay-3bd7bbba-a25a-4386-93cf-933266df22a7"}),
        TestCase("enableDisableStorageSettingsLink"),
        TestCase("showAvailableStorageMyFiles")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageDrive")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageSmbfs")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("openHelpPageFromDownloadsVolume"),
        TestCase("openHelpPageFromDriveVolume"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot")
            .EnableMirrorSync()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesCardTooltipClickHides").NewDirectoryTree(),
                      // Section end - browser tests for new directory tree
                      TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipLabelChange"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipMouseOverStaysOpen"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides"),
                      TestCase("filesTooltipHidesOnDeleteDialogClosed")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileListAriaAttributes")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListFocusFirstItem"),
        TestCase("fileListSelectLastFocusedItem"),
        TestCase("fileListSortWithKeyboard"),
        TestCase("fileListKeyboardSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListMouseSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListDeleteMultipleFiles"),
        TestCase("fileListRenameSelectedItem"),
        TestCase("fileListRenameFromSelectAll")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("mountCrostini").NewDirectoryTree(),
        TestCase("enableDisableCrostini").NewDirectoryTree(),
        TestCase("sharePathWithCrostini")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-122c00f8-9842-4666-8ca0-b6bf47454551"}),
// Section end - browser tests for new directory tree
        TestCase("mountCrostini"),
        TestCase("enableDisableCrostini"),
        TestCase("sharePathWithCrostini")
            .FeatureIds({"screenplay-122c00f8-9842-4666-8ca0-b6bf47454551"}),
        TestCase("pluginVmDirectoryNotSharedErrorDialog"),
        TestCase("pluginVmFileOnExternalDriveErrorDialog"),
        TestCase("pluginVmFileDropFailErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        // TODO(b/307656687): enable the test
        // TestCase("showMyFiles").NewDirectoryTree(),
        TestCase("directoryTreeRefresh")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-02521fe6-a9c5-4cd1-ac9b-cc46df33c1a0"}),
        TestCase("myFilesDisplaysAndOpensEntries").NewDirectoryTree(),
        TestCase("myFilesUpdatesChildren").NewDirectoryTree(),
        TestCase("myFilesAutoExpandOnce").NewDirectoryTree(),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts")
            .DontMountVolumes()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9"}),
        TestCase("myFilesFolderRename").NewDirectoryTree(),
        TestCase("myFilesToolbarDelete").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("directoryTreeRefresh")
            .FeatureIds({"screenplay-02521fe6-a9c5-4cd1-ac9b-cc46df33c1a0"}),
        TestCase("showMyFiles"),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts")
            .DontMountVolumes()
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9"}),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Navigation, /* navigation.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("navigateToParent")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog").NewDirectoryTree(),
                      // Section end - browser tests for new directory tree
                      TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        // TODO(b/307657164): enable the test
        // TestCase("recentsNested").NewDirectoryTree(),
        TestCase("recentsFilterResetToAll").NewDirectoryTree(),
        TestCase("recentsA11yMessages")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("recentsReadOnlyHidden").NewDirectoryTree(),
        // TestCase("recentsAllowDeletion").EnableArc().NewDirectoryTree(),
        // TestCase("recentsAllowMultipleFilesDeletion")
        //     .EnableArc()
        //     .NewDirectoryTree(),
        TestCase("recentsAllowRename")
            .EnableArc()
            .NewDirectoryTree()
            .FeatureIds({"screenplay-788b6d1f-0752-41e9-826e-bba324a19ef9"}),
        TestCase("recentsNoRenameForPlayFiles").EnableArc().NewDirectoryTree(),
        TestCase("recentsAllowCutForDownloads").NewDirectoryTree(),
        TestCase("recentsAllowCutForDrive").NewDirectoryTree(),
        // TestCase("recentsAllowCutForPlayFiles").EnableArc().NewDirectoryTree(),
        TestCase("recentsTimePeriodHeadings").NewDirectoryTree(),
        TestCase("recentsEmptyFolderMessage").NewDirectoryTree(),
        TestCase("recentsEmptyFolderMessageAfterDeletion").NewDirectoryTree(),
        TestCase("recentsRespondToTimezoneChangeForListView")
            .NewDirectoryTree(),
        TestCase("recentsRespondToTimezoneChangeForGridView")
            .NewDirectoryTree(),
        TestCase("recentsRespectSearchWhenSwitchingFilter").NewDirectoryTree(),
        TestCase("recentFileSystemProviderFiles")
            .FakeFileSystemProvider()
            .EnableFSPsInRecents()
            .NewDirectoryTree(),
        TestCase("recentsCrostiniMounted").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("recentsA11yMessages")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("recentsAllowCutForDownloads"),
        TestCase("recentsAllowCutForDrive"),
        TestCase("recentsAllowCutForPlayFiles").EnableArc(),
        TestCase("recentsAllowDeletion").EnableArc(),
        TestCase("recentsAllowMultipleFilesDeletion").EnableArc(),
        TestCase("recentsAllowRename")
            .EnableArc()
            .FeatureIds({"screenplay-788b6d1f-0752-41e9-826e-bba324a19ef9"}),
        TestCase("recentsEmptyFolderMessage"),
        TestCase("recentsEmptyFolderMessageAfterDeletion"),
        TestCase("recentsDownloads"),
        TestCase("recentsDrive"),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentsFilterResetToAll"),
        TestCase("recentsNested"),
        TestCase("recentsNoRenameForPlayFiles").EnableArc(),
        TestCase("recentsPlayFiles").EnableArc(),
        TestCase("recentsReadOnlyHidden"),
        TestCase("recentsRespectSearchWhenSwitchingFilter"),
        TestCase("recentsRespondToTimezoneChangeForGridView"),
        TestCase("recentsRespondToTimezoneChangeForListView"),
        TestCase("recentsTimePeriodHeadings"),
        TestCase("recentAudioDownloads"),
        TestCase("recentAudioDownloadsAndDrive"),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentDocumentsDownloads"),
        TestCase("recentDocumentsDownloadsAndDrive"),
        TestCase("recentDocumentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentImagesDownloads"),
        TestCase("recentImagesDownloadsAndDrive"),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentVideosDownloads"),
        TestCase("recentVideosDownloadsAndDrive"),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentFileSystemProviderFiles")
            .FakeFileSystemProvider()
            .EnableFSPsInRecents()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        // TODO(b/301342772): enable the test
        // TestCase("metadataDrive").NewDirectoryTree(),
        TestCase("metadataDownloads").NewDirectoryTree(),
        TestCase("metadataLargeDrive").NewDirectoryTree(),
        TestCase("metadataTeamDrives").NewDirectoryTree(),
        TestCase("metadataDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive"),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(
        // TODO(b/307656688): enable the tests
        // TestCase("searchQueryLaunchParam").NewDirectoryTree(),
        TestCase("searchWithLocationOptions").NewDirectoryTree(),
        TestCase("searchDriveWithRecencyOptions").NewDirectoryTree(),
        TestCase("searchDriveWithTypeOptions").NewDirectoryTree(),
        TestCase("searchRemovableDevice").NewDirectoryTree(),
        TestCase("searchPartitionedRemovableDevice").NewDirectoryTree(),
        TestCase("resetSearchOptionsOnFolderChange").NewDirectoryTree(),
        TestCase("searchFromMyFiles").NewDirectoryTree(),
        TestCase("searchHierarchy").NewDirectoryTree(),
        TestCase("hideSearchInTrash").NewDirectoryTree(),
        TestCase("searchSharedWithMe").NewDirectoryTree(),
        TestCase("searchDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("searchDocumentsProviderWithTypeOptions")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("searchDocumentsProviderWithRecencyOptions")
            .EnableGenericDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("searchFileSystemProvider").NewDirectoryTree(),
        TestCase("changingDirectoryClosesSearch").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("searchDownloadsWithResults"),
        TestCase("searchDownloadsWithNoResults"),
        TestCase("searchDownloadsClearSearchKeyDown"),
        TestCase("searchDownloadsClearSearch"),
        TestCase("searchHidingViaTab"),
        TestCase("searchHidingTextEntryField"),
        TestCase("searchButtonToggles"),
        TestCase("searchWithLocationOptions"),
        TestCase("searchLocalWithTypeOptions"),
        TestCase("searchDriveWithTypeOptions"),
        TestCase("searchWithRecencyOptions"),
        TestCase("searchDriveWithRecencyOptions"),
        TestCase("searchRemovableDevice"),
        TestCase("searchPartitionedRemovableDevice"),
        TestCase("resetSearchOptionsOnFolderChange"),
        TestCase("showSearchResultMessageWhenSearching"),
        TestCase("searchFromMyFiles"),
        TestCase("selectionPath"),
        TestCase("searchHierarchy"),
        TestCase("hideSearchInTrash"),
// TODO(b/287169303): test is flaky on ChromiumOS MSan
// TODO(crbug.com/1493224): Test is flaky on ChromiumOS Asan / Lsan.
#if !defined(ADDRESS_SANITIZER) && !defined(LEAK_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
        TestCase("searchTrashedFiles"),
#endif
        TestCase("matchDriveFilesByName"),
        TestCase("searchSharedWithMe"),
        TestCase("searchDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("searchDocumentsProviderWithTypeOptions")
            .EnableGenericDocumentsProvider(),
        TestCase("searchDocumentsProviderWithRecencyOptions")
            .EnableGenericDocumentsProvider(),
        TestCase("searchFileSystemProvider"),
        TestCase("searchImageByContent").EnableLocalImageSearch(),
        TestCase("changingDirectoryClosesSearch"),
        TestCase("searchQueryLaunchParam")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum"),
// TODO(https://crbug.com/1303472): Fix flakes and re-enable.
#if !BUILDFLAG(IS_CHROMEOS)
                      TestCase("metricsRecordDirectoryListLoad"),
                      TestCase("metricsRecordUpdateAvailableApps"),
#endif
                      TestCase("metricsOpenSwa")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("breadcrumbsNavigate").NewDirectoryTree(),
        TestCase("breadcrumbsDownloadsTranslation").NewDirectoryTree(),
        TestCase("breadcrumbsRenderShortPath").NewDirectoryTree(),
        TestCase("breadcrumbsEliderButtonNotExist").NewDirectoryTree(),
        TestCase("breadcrumbsRenderLongPath").NewDirectoryTree(),
        TestCase("breadcrumbsMainButtonClick").NewDirectoryTree(),
        TestCase("breadcrumbsMainButtonEnterKey").NewDirectoryTree(),
        TestCase("breadcrumbsEliderButtonClick").NewDirectoryTree(),
        TestCase("breadcrumbsEliderButtonKeyboard").NewDirectoryTree(),
        TestCase("breadcrumbsEliderMenuClickOutside").NewDirectoryTree(),
        TestCase("breadcrumbsEliderMenuItemClick").NewDirectoryTree(),
        TestCase("breadcrumbsEliderMenuItemTabLeft").NewDirectoryTree(),
        TestCase("breadcrumbNavigateBackToSharedWithMe").NewDirectoryTree(),
        TestCase("breadcrumbsEliderMenuItemTabRight").NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("breadcrumbsNavigate"),
        TestCase("breadcrumbsDownloadsTranslation"),
        TestCase("breadcrumbsRenderShortPath"),
        TestCase("breadcrumbsEliderButtonNotExist"),
        TestCase("breadcrumbsRenderLongPath"),
        TestCase("breadcrumbsMainButtonClick"),
        TestCase("breadcrumbsMainButtonEnterKey"),
        TestCase("breadcrumbsEliderButtonClick"),
        TestCase("breadcrumbsEliderButtonKeyboard"),
        TestCase("breadcrumbsEliderMenuClickOutside"),
        TestCase("breadcrumbsEliderMenuItemClick"),
        TestCase("breadcrumbsEliderMenuItemTabLeft"),
        TestCase("breadcrumbNavigateBackToSharedWithMe"),
        TestCase("breadcrumbsEliderMenuItemTabRight")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("formatDialogGearMenu").NewDirectoryTree(),
        TestCase("formatDialogGearMenu")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialog").NewDirectoryTree(),
        TestCase("formatDialog")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialogIsModal").NewDirectoryTree(),
        TestCase("formatDialogIsModal")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialogEmpty").NewDirectoryTree(),
        TestCase("formatDialogEmpty")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialogCancel").NewDirectoryTree(),
        TestCase("formatDialogCancel")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialogNameLength").NewDirectoryTree(),
        TestCase("formatDialogNameLength")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        TestCase("formatDialogNameInvalid").NewDirectoryTree(),
        TestCase("formatDialogNameInvalid")
            .EnableSinglePartitionFormat()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("formatDialog"),
        TestCase("formatDialogIsModal"),
        TestCase("formatDialogEmpty"),
        TestCase("formatDialogCancel"),
        TestCase("formatDialogNameLength"),
        TestCase("formatDialogNameInvalid"),
        TestCase("formatDialogGearMenu"),
        TestCase("formatDialog").EnableSinglePartitionFormat(),
        TestCase("formatDialogIsModal").EnableSinglePartitionFormat(),
        TestCase("formatDialogEmpty").EnableSinglePartitionFormat(),
        TestCase("formatDialogCancel").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameLength").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameInvalid").EnableSinglePartitionFormat(),
        TestCase("formatDialogGearMenu").EnableSinglePartitionFormat()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Trash, /* trash.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("trashMoveToTrash")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-a06f961a-17f5-4fbd-8285-49abb000dee1"}),
        TestCase("trashDeleteFromTrashOriginallyFromMyFiles")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashRestoreFromTrash").NewDirectoryTree(),
        TestCase("trashRestoreFromTrashShortcut").NewDirectoryTree(),
        TestCase("trashEmptyTrash")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashEmptyTrashShortcut")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrash")
            .NewDirectoryTree()
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        // TODO(b/301344220): enable the tests
        // TestCase("trashDeleteFromTrashOriginallyFromDrive")
        //     .NewDirectoryTree()
        //     .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashNoTasksInTrashRoot").NewDirectoryTree(),
        TestCase("trashDoubleClickOnFileInTrashRootShowsDialog")
            .NewDirectoryTree(),
        TestCase(
            "trashPressingEnterOnFileInTrashRootShowsDialogWithRestoreButton")
            .NewDirectoryTree(),
        TestCase("trashTraversingFolderShowsDisallowedDialog")
            .NewDirectoryTree(),
        TestCase("trashDragDropRootAcceptsEntries").NewDirectoryTree(),
        TestCase("trashDragDropFromDisallowedRootsFails").NewDirectoryTree(),
        TestCase("trashDragDropRootPerformsTrashAction").NewDirectoryTree(),
        TestCase("trashDragDropNonModifiableEntriesCantBeTrashed")
            .NewDirectoryTree(),
        // TestCase("trashDontShowTrashRootOnSelectFileDialog").NewDirectoryTree(),
        TestCase("trashDontShowTrashRootWhenOpeningAsAndroidFilePicker")
            .NewDirectoryTree(),
        TestCase("trashEnsureOldEntriesArePeriodicallyRemoved")
            .NewDirectoryTree(),
        TestCase("trashDragDropOutOfTrashPerformsRestoration")
            .NewDirectoryTree(),
        // TestCase("trashTogglingTrashEnabledPrefUpdatesDirectoryTree")
        //     .NewDirectoryTree(),
        // TestCase("trashTogglingTrashEnabledNavigatesAwayFromTrashRoot")
        //     .NewDirectoryTree(),
        TestCase("trashCantRestoreWhenParentDoesntExist").NewDirectoryTree(),
        TestCase("trashInfeasibleActionsForFileDisabledAndHiddenInTrashRoot")
            .NewDirectoryTree(),
        TestCase("trashInfeasibleActionsForFolderDisabledAndHiddenInTrashRoot")
            .NewDirectoryTree(),
        TestCase("trashExtractAllForZipHiddenAndDisabledInTrashRoot")
            .NewDirectoryTree(),
        TestCase("trashAllActionsDisabledForBlankSpaceInTrashRoot")
            .NewDirectoryTree(),
        // TestCase("trashNudgeShownOnFirstTrashOperation").NewDirectoryTree(),
        TestCase("trashStaleTrashInfoFilesAreRemovedAfterOneHour")
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("trashMoveToTrash")
            .FeatureIds({"screenplay-a06f961a-17f5-4fbd-8285-49abb000dee1"}),
        TestCase("trashPermanentlyDelete"),
        TestCase("trashRestoreFromToast"),
// TODO(crbug.com/1425820): Re-enable this test on ChromiumOS MSAN.
#if !defined(MEMORY_SANITIZER)
        TestCase("trashRestoreFromToast").EnableCrosComponents(),
#endif
        TestCase("trashRestoreFromTrash"),
        TestCase("trashRestoreFromTrashShortcut"),
        TestCase("trashEmptyTrash")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashEmptyTrashShortcut")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrash")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrashOriginallyFromMyFiles")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrashOriginallyFromDrive")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"})
            .EnableDriveTrash(),
        TestCase("trashNoTasksInTrashRoot"),
        TestCase("trashDoubleClickOnFileInTrashRootShowsDialog"),
        TestCase("trashDragDropRootAcceptsEntries"),
        TestCase("trashDragDropFromDisallowedRootsFails"),
        TestCase("trashDragDropNonModifiableEntriesCantBeTrashed"),
        TestCase("trashDragDropRootPerformsTrashAction"),
        TestCase("trashTraversingFolderShowsDisallowedDialog"),
        TestCase("trashDontShowTrashRootOnSelectFileDialog"),
        TestCase("trashDontShowTrashRootWhenOpeningAsAndroidFilePicker"),
        TestCase("trashEnsureOldEntriesArePeriodicallyRemoved"),
        TestCase("trashDragDropOutOfTrashPerformsRestoration"),
        TestCase("trashRestorationDialogInProgressDoesntShowUndo"),
        TestCase("trashRestorationDialogInProgressDoesntShowUndo")
            .EnableCrosComponents(),
        TestCase("trashTogglingTrashEnabledNavigatesAwayFromTrashRoot"),
        TestCase("trashTogglingTrashEnabledPrefUpdatesDirectoryTree"),
        TestCase("trashCantRestoreWhenParentDoesntExist"),
        TestCase(
            "trashPressingEnterOnFileInTrashRootShowsDialogWithRestoreButton"),
        TestCase("trashInfeasibleActionsForFileDisabledAndHiddenInTrashRoot"),
        TestCase("trashInfeasibleActionsForFolderDisabledAndHiddenInTrashRoot"),
        TestCase("trashExtractAllForZipHiddenAndDisabledInTrashRoot"),
        TestCase("trashAllActionsDisabledForBlankSpaceInTrashRoot"),
        TestCase("trashNudgeShownOnFirstTrashOperation"),
        TestCase("trashStaleTrashInfoFilesAreRemovedAfterOneHour")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    AndroidPhotos, /* android_photos.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .NewDirectoryTree(),
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .EnableCrosComponents()
            .NewDirectoryTree(),
        // Section end - browser tests for new directory tree
        TestCase("androidPhotosBanner").EnablePhotosDocumentsProvider(),
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .EnableCrosComponents()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Office, /* office.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openOfficeWordFile").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromMyFiles").EnableUploadOfficeToCloud(),
        TestCase("uploadToDriveRequiresUploadOfficeToCloudEnabled"),
        TestCase("openMultipleOfficeWordFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeExcelFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficePowerPointFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromDriveNotSynced")
            .EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromMyFilesOffline")
            .EnableUploadOfficeToCloud()
            .Offline(),
        TestCase("openOfficeWordFromDriveOffline")
            .EnableUploadOfficeToCloud()
            .Offline(),
        TestCase("officeShowNudgeGoogleDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GuestOs, /* guest_os.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fakesListed").NewDirectoryTree(),
        TestCase("listUpdatedWhenGuestsChanged").NewDirectoryTree(),
// TODO(http://crbug.com/1486453): Flaky on ASan.
#if !defined(ADDRESS_SANITIZER) && !defined(LEAK_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
        TestCase("mountGuestSuccess").NewDirectoryTree(),
        TestCase("mountAndroidVolumeSuccess").EnableArcVm().NewDirectoryTree(),
#endif
// Section end - browser tests for new directory tree
        TestCase("fakesListed"),
        TestCase("listUpdatedWhenGuestsChanged")
// TODO(http://crbug.com/1486453): Flaky on ASan.
#if !defined(ADDRESS_SANITIZER) && !defined(LEAK_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
            ,
        TestCase("mountGuestSuccess"),
        TestCase("mountAndroidVolumeSuccess").EnableArcVm()
#endif
            ));

}  // namespace file_manager
