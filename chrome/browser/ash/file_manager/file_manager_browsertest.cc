// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"

namespace file_manager {

// FilesAppBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* const name) : name(name) {
    CHECK(name && *name) << "no test case name";
  }

  TestCase& InGuestMode() {
    options.guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    options.guest_mode = IN_INCOGNITO;
    return *this;
  }

  TestCase& TabletMode() {
    options.tablet_mode = true;
    return *this;
  }

  TestCase& EnableGenericDocumentsProvider() {
    options.arc = true;
    options.generic_documents_provider = true;
    return *this;
  }

  TestCase& DisableGenericDocumentsProvider() {
    options.generic_documents_provider = false;
    return *this;
  }

  TestCase& EnablePhotosDocumentsProvider() {
    options.arc = true;
    options.photos_documents_provider = true;
    return *this;
  }

  TestCase& DisablePhotosDocumentsProvider() {
    options.photos_documents_provider = false;
    return *this;
  }

  TestCase& EnableArc() {
    options.arc = true;
    return *this;
  }

  TestCase& ExtractArchive() {
    options.extract_archive = true;
    return *this;
  }

  TestCase& Offline() {
    options.offline = true;
    return *this;
  }

  TestCase& FilesSwa() {
    options.files_swa = true;
    return *this;
  }

  TestCase& MediaSwa() {
    options.media_swa = true;
    return *this;
  }

  TestCase& DisableNativeSmb() {
    options.native_smb = false;
    return *this;
  }

  TestCase& DontMountVolumes() {
    options.mount_volumes = false;
    return *this;
  }

  TestCase& DontObserveFileTasks() {
    options.observe_file_tasks = false;
    return *this;
  }

  TestCase& EnableSinglePartitionFormat() {
    options.single_partition_format = true;
    return *this;
  }

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser() {
    options.browser = true;
    return *this;
  }

  TestCase& EnableDriveDssPin() {
    options.drive_dss_pin = true;
    return *this;
  }

  TestCase& EnableFiltersInRecents() {
    options.enable_filters_in_recents = true;
    return *this;
  }

  TestCase& EnableFiltersInRecentsV2() {
    options.enable_filters_in_recents_v2 = true;
    return *this;
  }

  TestCase& EnableTrash() {
    options.enable_trash = true;
    return *this;
  }

  TestCase& EnableDlp() {
    options.enable_dlp_files_restriction = true;
    return *this;
  }

  TestCase& EnableWebDriveOffice() {
    options.enable_web_drive_office = true;
    return *this;
  }

  TestCase& EnableGuestOsFiles() {
    options.enable_guest_os_files = true;
    return *this;
  }

  std::string GetFullName() const {
    std::string full_name = name;

    if (options.guest_mode == IN_GUEST_MODE)
      full_name += "_GuestMode";

    if (options.guest_mode == IN_INCOGNITO)
      full_name += "_Incognito";

    if (options.tablet_mode)
      full_name += "_TabletMode";

    if (options.files_swa)
      full_name += "_FilesSwa";

    if (!options.native_smb)
      full_name += "_DisableNativeSmb";

    if (options.generic_documents_provider)
      full_name += "_GenericDocumentsProvider";

    if (options.photos_documents_provider)
      full_name += "_PhotosDocumentsProvider";

    if (options.drive_dss_pin)
      full_name += "_DriveDssPin";

    if (options.single_partition_format)
      full_name += "_SinglePartitionFormat";

    if (options.enable_trash)
      full_name += "_Trash";

    if (options.enable_filters_in_recents)
      full_name += "_FiltersInRecents";

    if (options.enable_filters_in_recents_v2)
      full_name += "_FiltersInRecentsV2";

    return full_name;
  }

  const char* const name;
  FileManagerBrowserTestBase::Options options;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  return out << test_case.options;
}

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

// A version of FilesAppBrowserTest that supports DLP files restrictions.
class DlpFilesAppBrowserTest : public FilesAppBrowserTest {
 protected:
  DlpFilesAppBrowserTest() = default;

  DlpFilesAppBrowserTest(const DlpFilesAppBrowserTest&) = delete;
  DlpFilesAppBrowserTest& operator=(const DlpFilesAppBrowserTest&) = delete;

  ~DlpFilesAppBrowserTest() override = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetUpOnMainThread() override {
    FilesAppBrowserTest::SetUpOnMainThread();
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpFilesAppBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
  }

  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  policy::MockDlpRulesManager* mock_rules_manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(DlpFilesAppBrowserTest, Test) {
  chromeos::DlpClient::Get()->GetTestInterface()->SetFakeSource("example1.com");

  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsRestricted)
      .WillByDefault(::testing::Return(policy::DlpRulesManager::Level::kAllow));
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(nullptr));
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedDestination)
      .WillRepeatedly(
          ::testing::Return(policy::DlpRulesManager::Level::kBlock));

  StartTest();
}

// INSTANTIATE_TEST_SUITE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_SUITE_P,
// as WRAPPED_INSTANTIATE_TEST_SUITE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#define WRAPPED_INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator, &PostTestCaseName)

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return test.param.GetFullName();
}

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayDownloads"),
        TestCase("fileDisplayDownloads").FilesSwa(),
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").InGuestMode().FilesSwa(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayDownloads").TabletMode().FilesSwa(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks().FilesSwa(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFolder")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayLaunchOnLocalFile").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFile")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive").TabletMode().FilesSwa(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDrive").FilesSwa(),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOffline").Offline().FilesSwa(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayDriveOnline").FilesSwa(),
        TestCase("fileDisplayDriveOnlineNewWindow").DontObserveFileTasks(),
        TestCase("fileDisplayDriveOnlineNewWindow")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayComputers").FilesSwa(),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayMtp").FilesSwa(),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsb").FilesSwa(),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartition").FilesSwa(),
        TestCase("fileDisplayUsbPartition").EnableSinglePartitionFormat(),
        TestCase("fileDisplayUsbPartition")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayUsbPartitionSort").FilesSwa(),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileDisplayPartitionFileTable").FilesSwa(),
        TestCase("fileSearch"),
        TestCase("fileSearch").FilesSwa(),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutDownloadsVolume")
            .DontMountVolumes()
            .FilesSwa(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes().FilesSwa(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes()
            .FilesSwa(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive")
            .DontMountVolumes()
            .FilesSwa(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes().FilesSwa(),
        // Test is failing (crbug.com/1097013)
        // TestCase("fileDisplayWithoutDriveThenDisable").DontMountVolumes(),
        // TestCase("fileDisplayWithoutDriveThenDisable")
        //     .DontMountVolumes()
        //     .FilesSwa(),
        TestCase("fileDisplayWithHiddenVolume"),
        TestCase("fileDisplayWithHiddenVolume").FilesSwa(),
        TestCase("fileDisplayMountWithFakeItemSelected"),
        TestCase("fileDisplayMountWithFakeItemSelected").FilesSwa(),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected"),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected").FilesSwa(),
        TestCase("fileDisplayUnmountRemovableRoot"),
        TestCase("fileDisplayUnmountRemovableRoot").FilesSwa(),
        TestCase("fileDisplayUnmountFirstPartition"),
        TestCase("fileDisplayUnmountFirstPartition").FilesSwa(),
        TestCase("fileDisplayUnmountLastPartition"),
        TestCase("fileDisplayUnmountLastPartition").FilesSwa(),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchCaseInsensitive").FilesSwa(),
        TestCase("fileSearchNotFound"),
        TestCase("fileSearchNotFound").FilesSwa(),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner"),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner").FilesSwa(),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected"),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected").FilesSwa(),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory"),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory").FilesSwa(),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads").FilesSwa(),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles").FilesSwa(),
        TestCase("fileDisplayStartupError")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoMediaApp, /* open_video_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("videoOpenDownloads").MediaSwa().InGuestMode(),
        TestCase("videoOpenDownloads").MediaSwa().InGuestMode().FilesSwa(),
        TestCase("videoOpenDownloads").MediaSwa().FilesSwa(),
        TestCase("videoOpenDownloads").MediaSwa(),
        TestCase("videoOpenDrive").MediaSwa(),
        TestCase("videoOpenDrive").MediaSwa().FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioMediaApp, /* open_audio_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenDownloads").MediaSwa().InGuestMode(),
                      TestCase("audioOpenDownloads").MediaSwa(),
                      TestCase("audioOpenDrive").MediaSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("imageOpenMediaAppDownloads").MediaSwa().InGuestMode(),
        TestCase("imageOpenMediaAppDownloads")
            .MediaSwa()
            .InGuestMode()
            .FilesSwa(),
        TestCase("imageOpenMediaAppDownloads").MediaSwa(),
        TestCase("imageOpenMediaAppDownloads").MediaSwa().FilesSwa(),
        TestCase("imageOpenMediaAppDrive").MediaSwa(),
        TestCase("imageOpenMediaAppDrive").MediaSwa().FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDownloads").FilesSwa(),
                      TestCase("pdfOpenDrive"),
                      TestCase("pdfOpenDrive").FilesSwa(),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDownloads").FilesSwa(),
                      TestCase("textOpenDrive"),
                      TestCase("textOpenDrive").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("zipFileOpenDownloads"),
        TestCase("zipFileOpenDownloads").FilesSwa(),
        TestCase("zipFileOpenDownloads").InGuestMode(),
        TestCase("zipFileOpenDownloads").InGuestMode().FilesSwa(),
        TestCase("zipFileOpenDrive"),
        TestCase("zipFileOpenDrive").FilesSwa(),
        TestCase("zipFileOpenUsb"),
        TestCase("zipFileOpenUsb").FilesSwa(),
        TestCase("zipNotifyFileTasks"),
        TestCase("zipNotifyFileTasks").FilesSwa(),
        TestCase("zipCreateFileDownloads"),
        TestCase("zipCreateFileDownloads").FilesSwa(),
        TestCase("zipCreateFileDownloads").InGuestMode(),
        TestCase("zipCreateFileDownloads").InGuestMode().FilesSwa(),
        TestCase("zipCreateFileDrive"),
        TestCase("zipCreateFileDrive").FilesSwa(),
        TestCase("zipCreateFileUsb"),
        TestCase("zipCreateFileUsb").FilesSwa(),
        TestCase("zipExtractA11y").ExtractArchive().FilesSwa(),
        TestCase("zipExtractCheckContent").ExtractArchive().FilesSwa(),
        TestCase("zipExtractCheckDuplicates").ExtractArchive().FilesSwa(),
        TestCase("zipExtractCheckEncodings").ExtractArchive().FilesSwa(),
        TestCase("zipExtractShowPanel").ExtractArchive().FilesSwa(),
        TestCase("zipExtractSelectionMenus").ExtractArchive().FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("selectCreateFolderDownloads"),
        TestCase("selectCreateFolderDownloads").FilesSwa(),
        TestCase("selectCreateFolderDownloads").InGuestMode(),
        TestCase("selectCreateFolderDownloads").InGuestMode().FilesSwa(),
        TestCase("createFolderDownloads"),
        TestCase("createFolderDownloads").FilesSwa(),
        TestCase("createFolderDownloads").InGuestMode(),
        TestCase("createFolderDownloads").InGuestMode().FilesSwa(),
        TestCase("createFolderNestedDownloads"),
        TestCase("createFolderNestedDownloads").FilesSwa(),
        TestCase("createFolderDrive"),
        TestCase("createFolderDrive").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("keyboardDeleteDownloads").InGuestMode(),
        TestCase("keyboardDeleteDownloads").InGuestMode().FilesSwa(),
        TestCase("keyboardDeleteDownloads"),
        TestCase("keyboardDeleteDownloads").FilesSwa(),
        TestCase("keyboardDeleteDownloads").EnableTrash(),
        TestCase("keyboardDeleteDownloads").EnableTrash().FilesSwa(),
        TestCase("keyboardDeleteDrive"),
        TestCase("keyboardDeleteDrive").FilesSwa(),
        TestCase("keyboardDeleteDrive").EnableTrash(),
        TestCase("keyboardDeleteDrive").EnableTrash().FilesSwa(),
        TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
        TestCase("keyboardDeleteFolderDownloads").InGuestMode().FilesSwa(),
        TestCase("keyboardDeleteFolderDownloads"),
        TestCase("keyboardDeleteFolderDownloads").FilesSwa(),
        TestCase("keyboardDeleteFolderDownloads").EnableTrash(),
        TestCase("keyboardDeleteFolderDownloads").EnableTrash().FilesSwa(),
        TestCase("keyboardDeleteFolderDrive"),
        TestCase("keyboardDeleteFolderDrive").FilesSwa(),
        TestCase("keyboardCopyDownloads").InGuestMode(),
        TestCase("keyboardCopyDownloads").InGuestMode().FilesSwa(),
        TestCase("keyboardCopyDownloads"),
        TestCase("keyboardCopyDownloads").FilesSwa(),
        TestCase("keyboardCopyDownloads").EnableTrash(),
        TestCase("keyboardCopyDownloads").EnableTrash().FilesSwa(),
        TestCase("keyboardCopyDrive"),
        TestCase("keyboardCopyDrive").FilesSwa(),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("keyboardFocusOutlineVisible"),
        TestCase("keyboardFocusOutlineVisible").FilesSwa(),
        TestCase("keyboardFocusOutlineVisible").EnableTrash(),
        TestCase("keyboardFocusOutlineVisible").EnableTrash().FilesSwa(),
        TestCase("keyboardFocusOutlineVisibleMouse"),
        TestCase("keyboardFocusOutlineVisibleMouse").FilesSwa(),
        TestCase("keyboardFocusOutlineVisibleMouse").EnableTrash(),
        TestCase("keyboardFocusOutlineVisibleMouse").EnableTrash().FilesSwa(),
#endif
        TestCase("keyboardSelectDriveDirectoryTree"),
        TestCase("keyboardSelectDriveDirectoryTree").FilesSwa(),
        TestCase("keyboardDisableCopyWhenDialogDisplayed"),
        TestCase("keyboardDisableCopyWhenDialogDisplayed").FilesSwa(),
        TestCase("keyboardOpenNewWindow"),
        TestCase("keyboardOpenNewWindow").FilesSwa(),
        TestCase("keyboardOpenNewWindow").InGuestMode(),
        TestCase("keyboardOpenNewWindow").InGuestMode().FilesSwa(),
        TestCase("noPointerActiveOnTouch"),
        TestCase("noPointerActiveOnTouch").FilesSwa(),
        TestCase("pointerActiveRemovedByTouch"),
        TestCase("pointerActiveRemovedByTouch").FilesSwa(),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDownloads").FilesSwa(),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDownloads").InGuestMode().FilesSwa(),
        TestCase("renameFileDrive"),
        TestCase("renameFileDrive").FilesSwa(),
        TestCase("renameNewFolderDownloads"),
        TestCase("renameNewFolderDownloads").FilesSwa(),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameNewFolderDownloads").InGuestMode().FilesSwa(),
        TestCase("renameRemovableWithKeyboardOnFileList"),
        TestCase("renameRemovableWithKeyboardOnFileList").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile"),
        TestCase("checkDeleteEnabledForReadWriteFile").FilesSwa(),
        TestCase("checkDeleteDisabledForReadOnlyDocument"),
        TestCase("checkDeleteDisabledForReadOnlyDocument").FilesSwa(),
        TestCase("checkDeleteDisabledForReadOnlyFile"),
        TestCase("checkDeleteDisabledForReadOnlyFile").FilesSwa(),
        TestCase("checkDeleteDisabledForReadOnlyFolder"),
        TestCase("checkDeleteDisabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkRenameEnabledForReadWriteFile"),
        TestCase("checkRenameEnabledForReadWriteFile").FilesSwa(),
        TestCase("checkRenameDisabledForReadOnlyDocument"),
        TestCase("checkRenameDisabledForReadOnlyDocument").FilesSwa(),
        TestCase("checkRenameDisabledForReadOnlyFile"),
        TestCase("checkRenameDisabledForReadOnlyFile").FilesSwa(),
        TestCase("checkRenameDisabledForReadOnlyFolder"),
        TestCase("checkRenameDisabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkContextMenuForRenameInput"),
        TestCase("checkContextMenuForRenameInput").FilesSwa(),
        TestCase("checkShareEnabledForReadWriteFile"),
        TestCase("checkShareEnabledForReadWriteFile").FilesSwa(),
        TestCase("checkShareEnabledForReadOnlyDocument"),
        TestCase("checkShareEnabledForReadOnlyDocument").FilesSwa(),
        TestCase("checkShareDisabledForStrictReadOnlyDocument"),
        TestCase("checkShareDisabledForStrictReadOnlyDocument").FilesSwa(),
        TestCase("checkShareEnabledForReadOnlyFile"),
        TestCase("checkShareEnabledForReadOnlyFile").FilesSwa(),
        TestCase("checkShareEnabledForReadOnlyFolder"),
        TestCase("checkShareEnabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkCopyEnabledForReadWriteFile"),
        TestCase("checkCopyEnabledForReadWriteFile").FilesSwa(),
        TestCase("checkCopyEnabledForReadOnlyDocument"),
        TestCase("checkCopyEnabledForReadOnlyDocument").FilesSwa(),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument").FilesSwa(),
        TestCase("checkCopyEnabledForReadOnlyFile"),
        TestCase("checkCopyEnabledForReadOnlyFile").FilesSwa(),
        TestCase("checkCopyEnabledForReadOnlyFolder"),
        TestCase("checkCopyEnabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkCutEnabledForReadWriteFile"),
        TestCase("checkCutEnabledForReadWriteFile").FilesSwa(),
        TestCase("checkCutDisabledForReadOnlyDocument"),
        TestCase("checkCutDisabledForReadOnlyDocument").FilesSwa(),
        TestCase("checkCutDisabledForReadOnlyFile"),
        TestCase("checkCutDisabledForReadOnlyFile").FilesSwa(),
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkCutDisabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder").FilesSwa(),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder").FilesSwa(),
        TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        // TODO(b/189173190): Enable
        // TestCase("checkInstallWithLinuxDisabledForDebianFile").FilesSwa(),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile").FilesSwa(),
        TestCase("checkImportCrostiniImageEnabled"),
        TestCase("checkImportCrostiniImageEnabled").FilesSwa(),
        TestCase("checkImportCrostiniImageDisabled"),
        // TODO(b/189173190): Enable
        // TestCase("checkImportCrostiniImageDisabled").FilesSwa(),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder").FilesSwa(),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder").FilesSwa(),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder").FilesSwa(),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder").FilesSwa(),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkDownloadsContextMenu").FilesSwa(),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkPlayFilesContextMenu").FilesSwa(),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu").FilesSwa(),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenuFocus").FilesSwa(),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkContextMenusForInputElements").FilesSwa(),
        TestCase("checkDeleteDisabledInRecents"),
        TestCase("checkDeleteDisabledInRecents").FilesSwa(),
        TestCase("checkGoToFileLocationEnabledInRecents"),
        TestCase("checkGoToFileLocationEnabledInRecents").FilesSwa(),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection"),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection")
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
        TestCase("toolbarDeleteWithMenuItemNoEntrySelected").FilesSwa(),
        TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
        TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog").FilesSwa(),
        TestCase("toolbarDeleteButtonKeepFocus"),
        TestCase("toolbarDeleteButtonKeepFocus").FilesSwa(),
        TestCase("toolbarDeleteEntry"),
        TestCase("toolbarDeleteEntry").FilesSwa(),
        TestCase("toolbarDeleteEntry").InGuestMode(),
        TestCase("toolbarDeleteEntry").InGuestMode().FilesSwa(),
        TestCase("toolbarDeleteEntry").EnableTrash(),
        TestCase("toolbarDeleteEntry").EnableTrash().FilesSwa(),
        TestCase("toolbarRefreshButtonWithSelection")
            .EnableGenericDocumentsProvider(),
        TestCase("toolbarRefreshButtonWithSelection")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("toolbarAltACommand"),
        TestCase("toolbarAltACommand").FilesSwa(),
        TestCase("toolbarRefreshButtonHiddenInRecents"),
        TestCase("toolbarRefreshButtonHiddenInRecents").FilesSwa(),
        TestCase("toolbarMultiMenuFollowsButton"),
        TestCase("toolbarMultiMenuFollowsButton").FilesSwa(),
        TestCase("toolbarSharesheetButtonWithSelection"),
        TestCase("toolbarSharesheetButtonWithSelection").FilesSwa(),
        TestCase("toolbarSharesheetContextMenuWithSelection"),
        TestCase("toolbarSharesheetContextMenuWithSelection").FilesSwa(),
        TestCase("toolbarSharesheetNoEntrySelected"),
        TestCase("toolbarSharesheetNoEntrySelected").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView"),
        TestCase("openQuickView").FilesSwa(),
        TestCase("openQuickViewDialog"),
        TestCase("openQuickViewDialog").FilesSwa(),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickViewAndEscape").FilesSwa(),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").InGuestMode().FilesSwa(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickView").TabletMode().FilesSwa(),
        TestCase("openQuickViewViaContextMenuSingleSelection"),
        TestCase("openQuickViewViaContextMenuSingleSelection").FilesSwa(),
        TestCase("openQuickViewViaContextMenuCheckSelections"),
        TestCase("openQuickViewViaContextMenuCheckSelections").FilesSwa(),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewAudio").FilesSwa(),
        TestCase("openQuickViewAudioOnDrive"),
        TestCase("openQuickViewAudioOnDrive").FilesSwa(),
        TestCase("openQuickViewAudioWithImageMetadata"),
        TestCase("openQuickViewAudioWithImageMetadata").FilesSwa(),
        TestCase("openQuickViewImageJpg"),
        TestCase("openQuickViewImageJpg").FilesSwa(),
        TestCase("openQuickViewImageJpeg"),
        TestCase("openQuickViewImageJpeg").FilesSwa(),
        TestCase("openQuickViewImageJpeg").InGuestMode(),
        TestCase("openQuickViewImageJpeg").InGuestMode().FilesSwa(),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageExif").FilesSwa(),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewImageRaw").FilesSwa(),
        TestCase("openQuickViewImageRawWithOrientation"),
        TestCase("openQuickViewImageRawWithOrientation").FilesSwa(),
        TestCase("openQuickViewImageWebp"),
        TestCase("openQuickViewImageWebp").FilesSwa(),
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewBrokenImage").FilesSwa(),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewImageClick").FilesSwa(),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideo").FilesSwa(),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewVideoOnDrive").FilesSwa(),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdf"),
#endif
        TestCase("openQuickViewPdf").FilesSwa(),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdfPopup"),
#endif
        TestCase("openQuickViewPdfPopup").FilesSwa(),
        TestCase("openQuickViewPdfPreviewsDisabled"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdfPreviewsDisabled").FilesSwa(),
#endif
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardUpDownChangesView").FilesSwa(),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView").FilesSwa(),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewSniffedText").FilesSwa(),
        TestCase("openQuickViewTextFileWithUnknownMimeType"),
        TestCase("openQuickViewTextFileWithUnknownMimeType").FilesSwa(),
        TestCase("openQuickViewUtf8Text"),
        TestCase("openQuickViewUtf8Text").FilesSwa(),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollText").FilesSwa(),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewScrollHtml").FilesSwa(),
        TestCase("openQuickViewMhtml"),
        TestCase("openQuickViewMhtml").FilesSwa(),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewBackgroundColorHtml").FilesSwa(),
        TestCase("openQuickViewDrive"),
        TestCase("openQuickViewDrive").FilesSwa(),
        TestCase("openQuickViewSmbfs"),
        TestCase("openQuickViewSmbfs").FilesSwa(),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewAndroid").FilesSwa(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewCrostini").FilesSwa(),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewUsb").FilesSwa(),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewRemovablePartitions").FilesSwa(),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewMtp").FilesSwa(),
        TestCase("openQuickViewTabIndexImage").MediaSwa(),
        TestCase("openQuickViewTabIndexImage").MediaSwa().FilesSwa(),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexText").FilesSwa(),
        TestCase("openQuickViewTabIndexHtml"),
        TestCase("openQuickViewTabIndexHtml").FilesSwa(),
        TestCase("openQuickViewTabIndexAudio").MediaSwa(),
        TestCase("openQuickViewTabIndexAudio").MediaSwa().FilesSwa(),
        TestCase("openQuickViewTabIndexVideo").MediaSwa(),
        TestCase("openQuickViewTabIndexVideo").MediaSwa().FilesSwa(),
        TestCase("openQuickViewTabIndexDeleteDialog"),
        TestCase("openQuickViewTabIndexDeleteDialog").FilesSwa(),
        TestCase("openQuickViewTabIndexDeleteDialog").EnableTrash(),
        TestCase("openQuickViewTabIndexDeleteDialog").EnableTrash().FilesSwa(),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonKeyboard").FilesSwa(),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewToggleInfoButtonClick").FilesSwa(),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFiles").FilesSwa(),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesText").FilesSwa(),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesPdf").FilesSwa(),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown").FilesSwa(),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight").FilesSwa(),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewFromDirectoryTree").FilesSwa(),
        TestCase("openQuickViewAndDeleteSingleSelection"),
        TestCase("openQuickViewAndDeleteSingleSelection").FilesSwa(),
        TestCase("openQuickViewAndDeleteSingleSelection").EnableTrash(),
        TestCase("openQuickViewAndDeleteSingleSelection")
            .EnableTrash()
            .FilesSwa(),
        TestCase("openQuickViewAndDeleteCheckSelection"),
        TestCase("openQuickViewAndDeleteCheckSelection").FilesSwa(),
        TestCase("openQuickViewAndDeleteCheckSelection").EnableTrash(),
        TestCase("openQuickViewAndDeleteCheckSelection")
            .EnableTrash()
            .FilesSwa(),
        TestCase("openQuickViewDeleteEntireCheckSelection"),
        TestCase("openQuickViewDeleteEntireCheckSelection").FilesSwa(),
        TestCase("openQuickViewDeleteEntireCheckSelection").EnableTrash(),
        TestCase("openQuickViewDeleteEntireCheckSelection")
            .EnableTrash()
            .FilesSwa(),
        TestCase("openQuickViewClickDeleteButton"),
        TestCase("openQuickViewClickDeleteButton").FilesSwa(),
        TestCase("openQuickViewClickDeleteButton").EnableTrash(),
        TestCase("openQuickViewClickDeleteButton").EnableTrash().FilesSwa(),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewDeleteButtonNotShown").FilesSwa(),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaViaContextMenu").FilesSwa(),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu").FilesSwa(),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu").FilesSwa(),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard").FilesSwa(),
        TestCase("closeQuickView"),
        TestCase("closeQuickView").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory"),
        TestCase("directoryTreeActiveDirectory").FilesSwa(),
        TestCase("directoryTreeSelectedDirectory"),
        TestCase("directoryTreeSelectedDirectory").FilesSwa(),
        TestCase("directoryTreeRecentsSubtypeScroll"),
        // TODO(b/189173190): Enable
        // TestCase("directoryTreeRecentsSubtypeScroll").FilesSwa(),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeHorizontalScroll").FilesSwa(),
        TestCase("directoryTreeExpandHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll").FilesSwa(),
        TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeExpandHorizontalScrollRTL").FilesSwa(),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeVerticalScroll").FilesSwa(),
        TestCase("directoryTreeExpandFolder"),
        TestCase("directoryTreeExpandFolder").FilesSwa(),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff"),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff")
            .FilesSwa(),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn"),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn")
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu").InGuestMode().FilesSwa(),
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithContextMenu").FilesSwa(),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard").InGuestMode().FilesSwa(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithKeyboard").FilesSwa(),
        TestCase("dirCopyWithoutChangingCurrent"),
        TestCase("dirCopyWithoutChangingCurrent").FilesSwa(),
        TestCase("dirCutWithContextMenu"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu").FilesSwa(),
        TestCase("dirCutWithContextMenu").InGuestMode(),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu").InGuestMode().FilesSwa(),
        TestCase("dirCutWithKeyboard"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard").FilesSwa(),
        TestCase("dirCutWithKeyboard").InGuestMode(),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard").InGuestMode().FilesSwa(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithContextMenu").FilesSwa(),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithContextMenu").InGuestMode().FilesSwa(),
        TestCase("dirPasteWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirPasteWithoutChangingCurrent").FilesSwa(),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameWithContextMenu").FilesSwa(),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameWithContextMenu").InGuestMode().FilesSwa(),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameUpdateChildrenBreadcrumbs").FilesSwa(),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithKeyboard").FilesSwa(),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithKeyboard").InGuestMode().FilesSwa(),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameWithoutChangingCurrent").FilesSwa(),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToEmptyString").FilesSwa(),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToEmptyString").InGuestMode().FilesSwa(),
        TestCase("dirRenameToExisting"),
        TestCase("dirRenameToExisting").FilesSwa(),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode(),
        TestCase("dirRenameToExisting").InGuestMode().FilesSwa(),
#endif
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").FilesSwa(),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode().FilesSwa(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").FilesSwa(),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode().FilesSwa(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirContextMenuForRenameInput").FilesSwa(),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithContextMenu").FilesSwa(),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithKeyboard").FilesSwa(),
        TestCase("dirCreateWithoutChangingCurrent"),
        TestCase("dirCreateWithoutChangingCurrent").FilesSwa(),
        TestCase("dirCreateMultipleFolders"),
        TestCase("dirCreateMultipleFolders").FilesSwa(),
        TestCase("dirContextMenuZip"),
        TestCase("dirContextMenuZip").FilesSwa(),
        TestCase("dirContextMenuZipEject"),
        TestCase("dirContextMenuZipEject").FilesSwa(),
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuRecent").FilesSwa(),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFiles").FilesSwa(),
        TestCase("dirContextMenuMyFiles").EnableTrash(),
        TestCase("dirContextMenuMyFiles").EnableTrash().FilesSwa(),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuMyFilesWithPaste").FilesSwa(),
        TestCase("dirContextMenuMyFilesWithPaste").EnableTrash(),
        TestCase("dirContextMenuMyFilesWithPaste").EnableTrash().FilesSwa(),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuCrostini").FilesSwa(),
        TestCase("dirContextMenuCrostini").EnableTrash(),
        TestCase("dirContextMenuCrostini").EnableTrash().FilesSwa(),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuPlayFiles").FilesSwa(),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuUsbs").FilesSwa(),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat().FilesSwa(),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuFsp").FilesSwa(),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuUsbDcim").FilesSwa(),
        TestCase("dirContextMenuUsbDcim").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuUsbDcim")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMtp").FilesSwa(),
        TestCase("dirContextMenuMediaView").EnableArc(),
        TestCase("dirContextMenuMediaView").EnableArc().FilesSwa(),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuMyDrive").FilesSwa(),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedDrive").FilesSwa(),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuSharedWithMe").FilesSwa(),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuOffline").FilesSwa(),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuComputers").FilesSwa(),
        TestCase("dirContextMenuTrash").EnableTrash(),
        TestCase("dirContextMenuTrash").EnableTrash().FilesSwa(),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuShortcut").FilesSwa(),
        TestCase("dirContextMenuFocus"),
        TestCase("dirContextMenuFocus").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline").EnableGenericDocumentsProvider(),
        TestCase("driveOpenSidebarOffline")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("driveOpenSidebarSharedWithMe").FilesSwa(),
        TestCase("driveAutoCompleteQuery"),
        TestCase("driveAutoCompleteQuery").FilesSwa(),
        TestCase("drivePinMultiple"),
        TestCase("drivePinMultiple").FilesSwa(),
        TestCase("drivePinHosted"),
        TestCase("drivePinHosted").FilesSwa(),
        TestCase("drivePinFileMobileNetwork"),
        // TODO(b/189173190): Enable
        // TestCase("drivePinFileMobileNetwork").FilesSwa(),
        TestCase("drivePinToggleUpdatesInFakeEntries"),
        TestCase("drivePinToggleUpdatesInFakeEntries").FilesSwa(),
        TestCase("driveClickFirstSearchResult"),
        TestCase("driveClickFirstSearchResult").FilesSwa(),
        TestCase("drivePressEnterToSearch"),
        TestCase("drivePressEnterToSearch").FilesSwa(),
        TestCase("drivePressClearSearch"),
        TestCase("drivePressClearSearch").FilesSwa(),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("drivePressCtrlAFromSearch").FilesSwa(),
        TestCase("driveBackupPhotos"),
        TestCase("driveBackupPhotos").EnableSinglePartitionFormat(),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineGearMenu").FilesSwa(),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu").FilesSwa(),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveAvailableOfflineActionBar").FilesSwa(),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkToDirectory").FilesSwa(),
        TestCase("driveLinkOpenFileThroughLinkedDirectory").MediaSwa(),
        TestCase("driveLinkOpenFileThroughLinkedDirectory")
            .MediaSwa()
            .FilesSwa(),
        TestCase("driveLinkOpenFileThroughTransitiveLink").MediaSwa(),
        TestCase("driveWelcomeBanner"),
        TestCase("driveOfflineInfoBanner").EnableDriveDssPin(),
        TestCase("driveOfflineInfoBannerWithoutFlag"),
        TestCase("driveLinkOpenFileThroughTransitiveLink")
            .MediaSwa()
            .FilesSwa(),
        TestCase("driveEnableDocsOfflineDialog"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialog").FilesSwa(),
        TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogWithoutWindow").FilesSwa(),
        TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogMultipleWindows").FilesSwa(),
        TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")
        //     .FilesSwa()
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBanner"),
        TestCase("holdingSpaceWelcomeBanner").FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged"),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDriveToDownloads").FilesSwa(),
        TestCase("transferFromDownloadsToMyFiles"),
        TestCase("transferFromDownloadsToMyFiles").FilesSwa(),
        TestCase("transferFromDownloadsToMyFilesMove"),
        TestCase("transferFromDownloadsToMyFilesMove").FilesSwa(),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromDownloadsToDrive").FilesSwa(),
        TestCase("transferFromSharedWithMeToDownloads"),
        TestCase("transferFromSharedWithMeToDownloads").FilesSwa(),
        TestCase("transferFromSharedWithMeToDrive"),
        TestCase("transferFromSharedWithMeToDrive").FilesSwa(),
        TestCase("transferFromDownloadsToSharedFolder"),
        TestCase("transferFromDownloadsToSharedFolder").FilesSwa(),
        TestCase("transferFromDownloadsToSharedFolderMove"),
        TestCase("transferFromDownloadsToSharedFolderMove").FilesSwa(),
        TestCase("transferFromSharedFolderToDownloads"),
        TestCase("transferFromSharedFolderToDownloads").FilesSwa(),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDownloads").FilesSwa(),
        TestCase("transferFromOfflineToDrive"),
        TestCase("transferFromOfflineToDrive").FilesSwa(),
        TestCase("transferFromTeamDriveToDrive"),
        TestCase("transferFromTeamDriveToDrive").FilesSwa(),
        TestCase("transferFromDriveToTeamDrive"),
        TestCase("transferFromDriveToTeamDrive").FilesSwa(),
        TestCase("transferFromTeamDriveToDownloads"),
        TestCase("transferFromTeamDriveToDownloads").FilesSwa(),
        TestCase("transferHostedFileFromTeamDriveToDownloads"),
        TestCase("transferHostedFileFromTeamDriveToDownloads").FilesSwa(),
        TestCase("transferFromDownloadsToTeamDrive"),
        TestCase("transferFromDownloadsToTeamDrive").FilesSwa(),
        TestCase("transferBetweenTeamDrives"),
        TestCase("transferBetweenTeamDrives").FilesSwa(),
        TestCase("transferDragDropActiveLeave"),
        TestCase("transferDragDropActiveLeave").FilesSwa(),
        TestCase("transferDragDropActiveDrop"),
        TestCase("transferDragDropActiveDrop").FilesSwa(),
        // TODO(crbug.com/1254578): Remove flakiness and enable.
        // TestCase("transferDragDropTreeItemAccepts"),
        TestCase("transferDragDropTreeItemAccepts").FilesSwa(),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragDropTreeItemDenies"),
        TestCase("transferDragDropTreeItemDenies").FilesSwa(),
#endif
        TestCase("transferDragAndHoverTreeItemEntryList"),
        TestCase("transferDragAndHoverTreeItemEntryList").FilesSwa(),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragAndHoverTreeItemFakeEntry"),
        TestCase("transferDragAndHoverTreeItemFakeEntry").FilesSwa(),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat(),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
#endif
        TestCase("transferDragFileListItemSelects"),
        TestCase("transferDragFileListItemSelects").FilesSwa(),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndDrop").FilesSwa(),
        TestCase("transferDragAndHover"),
        TestCase("transferDragAndHover").FilesSwa(),
        TestCase("transferDropBrowserFile"),
        TestCase("transferDropBrowserFile").FilesSwa(),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferFromDownloadsToDownloads").FilesSwa(),
        TestCase("transferDeletedFile"),
        TestCase("transferDeletedFile").FilesSwa(),
        TestCase("transferDeletedFile").EnableTrash(),
        TestCase("transferDeletedFile").EnableTrash().FilesSwa(),
        TestCase("transferInfoIsRemembered"),
        // TODO(b/189173190): Enable
        // TestCase("transferInfoIsRemembered").FilesSwa(),
        TestCase("transferToUsbHasDestinationText"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferToUsbHasDestinationText").FilesSwa(),
        TestCase("transferDismissedErrorIsRemembered"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferDismissedErrorIsRemembered").FilesSwa(),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText"),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText")
            .FilesSwa(),
        TestCase("transferUpdateSamePanelItem"),
        TestCase("transferUpdateSamePanelItem").FilesSwa(),
        TestCase("transferShowPendingMessageForZeroRemainingTime"),
        TestCase("transferShowPendingMessageForZeroRemainingTime").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    DlpFilesAppBrowserTest,
    ::testing::Values(TestCase("transferShowDlpToast").EnableDlp()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn").InGuestMode().FilesSwa(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreSortColumn").FilesSwa(),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView").InGuestMode().FilesSwa(),
                      TestCase("restoreCurrentView").FilesSwa(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestoreGeometry, /* restore_geometry.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreGeometry"),
                      TestCase("restoreGeometry").InGuestMode(),
                      TestCase("restoreGeometryMaximized")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("shareFileDrive"),
                      TestCase("shareFileDrive").FilesSwa(),
                      TestCase("shareDirectoryDrive"),
                      TestCase("shareDirectoryDrive").FilesSwa(),
                      TestCase("shareHostedFileDrive"),
                      TestCase("shareHostedFileDrive").FilesSwa(),
                      TestCase("manageHostedFileDrive"),
                      TestCase("manageHostedFileDrive").FilesSwa(),
                      TestCase("manageFileDrive"),
                      TestCase("manageFileDrive").FilesSwa(),
                      TestCase("manageDirectoryDrive"),
                      TestCase("manageDirectoryDrive").FilesSwa(),
                      TestCase("shareFileTeamDrive"),
                      TestCase("shareFileTeamDrive").FilesSwa(),
                      TestCase("shareDirectoryTeamDrive"),
                      TestCase("shareDirectoryTeamDrive").FilesSwa(),
                      TestCase("shareHostedFileTeamDrive"),
                      TestCase("shareHostedFileTeamDrive").FilesSwa(),
                      TestCase("shareTeamDrive"),
                      TestCase("shareTeamDrive").FilesSwa(),
                      TestCase("manageHostedFileTeamDrive"),
                      TestCase("manageHostedFileTeamDrive").FilesSwa(),
                      TestCase("manageFileTeamDrive"),
                      TestCase("manageFileTeamDrive").FilesSwa(),
                      TestCase("manageDirectoryTeamDrive"),
                      TestCase("manageDirectoryTeamDrive").FilesSwa(),
                      TestCase("manageTeamDrive").FilesSwa(),
                      TestCase("manageTeamDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads").InGuestMode().FilesSwa(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDownloads").FilesSwa(),
                      TestCase("traverseDrive").FilesSwa(),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("executeDefaultTaskDownloads"),
        TestCase("executeDefaultTaskDownloads").FilesSwa(),
        TestCase("executeDefaultTaskDownloads").InGuestMode(),
        TestCase("executeDefaultTaskDownloads").InGuestMode().FilesSwa(),
        TestCase("executeDefaultTaskDrive"),
        TestCase("executeDefaultTaskDrive").FilesSwa(),
        TestCase("defaultTaskForPdf"),
        TestCase("defaultTaskForPdf").FilesSwa(),
        TestCase("defaultTaskForTextPlain"),
        TestCase("defaultTaskForTextPlain").FilesSwa(),
        TestCase("defaultTaskDialogDownloads"),
        TestCase("defaultTaskDialogDownloads").FilesSwa(),
        TestCase("defaultTaskDialogDownloads").InGuestMode(),
        TestCase("defaultTaskDialogDownloads").InGuestMode().FilesSwa(),
        TestCase("defaultTaskDialogDrive"),
        TestCase("defaultTaskDialogDrive").FilesSwa(),
        TestCase("changeDefaultDialogScrollList"),
        TestCase("changeDefaultDialogScrollList").FilesSwa(),
        TestCase("genericTaskIsNotExecuted"),
        TestCase("genericTaskIsNotExecuted").FilesSwa(),
        TestCase("genericTaskAndNonGenericTask"),
        TestCase("genericTaskAndNonGenericTask").FilesSwa(),
        TestCase("noActionBarOpenForDirectories").FilesSwa(),
        TestCase("noActionBarOpenForDirectories")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts"),
                      TestCase("traverseFolderShortcuts").FilesSwa(),
                      TestCase("addRemoveFolderShortcuts").FilesSwa(),
                      TestCase("addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").FilesSwa(),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexSearchBoxFocus").FilesSwa(),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocus").FilesSwa(),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").FilesSwa(),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusDownloads").InGuestMode().FilesSwa(),
        TestCase("tabindexFocusDirectorySelected"),
        TestCase("tabindexFocusDirectorySelected").FilesSwa(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser().FilesSwa(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode()
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads")
        //     .WithBrowser()
        //     .InGuestMode()
        //     .FilesSwa()
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
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogUnload").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FilesSwa(),
        TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        // TestCase("openFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        //     .FilesSwa(),
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogPanelsDisabled").WithBrowser().FilesSwa(),
        TestCase("openFileDialogAriaMultipleSelect").WithBrowser(),
        TestCase("openFileDialogAriaMultipleSelect").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogAriaSingleSelect").WithBrowser(),
        TestCase("saveFileDialogAriaSingleSelect").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDownloads").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FilesSwa(),
        TestCase("saveFileDialogDownloads").WithBrowser().InIncognito(),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        //     .FilesSwa(),
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        // TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        // TestCase("saveFileDialogDownloadsNewFolderButton")
        //     .WithBrowser()
        //     .FilesSwa(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogDownloadsNewFolderButton")
            .WithBrowser()
            .FilesSwa(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser().FilesSwa(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser().FilesSwa(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDrive").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDrive").WithBrowser().InIncognito().FilesSwa(),
        TestCase("saveFileDialogDrive").WithBrowser(),
        TestCase("saveFileDialogDrive").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDrive").WithBrowser().InIncognito().FilesSwa(),
        TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveFromBrowser").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveHostedDoc").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser().FilesSwa(),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogCancelDrive").WithBrowser().FilesSwa(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser().FilesSwa(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .FilesSwa(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .FilesSwa(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .FilesSwa(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .FilesSwa(),
        TestCase("openFileDialogDefaultFilter").WithBrowser(),
        TestCase("openFileDialogDefaultFilter").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser().FilesSwa(),
        TestCase("saveFileDialogDefaultFilterKeyNavigation").WithBrowser(),
        TestCase("saveFileDialogDefaultFilterKeyNavigation")
            .WithBrowser()
            .FilesSwa(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll").WithBrowser(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll")
            .WithBrowser()
            .FilesSwa(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter")
            .WithBrowser()
            .FilesSwa(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter")
            .WithBrowser()
            .FilesSwa(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided")
            .WithBrowser()
            .FilesSwa(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu")
            .WithBrowser()
            .FilesSwa(),
        // TODO(crbug.com/1249726): Remove flakiness and enable this test.
        // TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser().FilesSwa(),
        TestCase("openMultiFileDialogSelectAllEnabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled")
            .WithBrowser()
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("copyBetweenWindowsLocalToDrive"),
                      TestCase("copyBetweenWindowsLocalToDrive").FilesSwa(),
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsLocalToUsb").FilesSwa(),
                      TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsUsbToDrive").FilesSwa(),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      TestCase("copyBetweenWindowsDriveToLocal").FilesSwa(),
                      TestCase("copyBetweenWindowsDriveToUsb"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsDriveToUsb").FilesSwa(),
                      TestCase("copyBetweenWindowsUsbToLocal").FilesSwa(),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showGridViewDownloads").InGuestMode(),
        TestCase("showGridViewDownloads").InGuestMode().FilesSwa(),
        TestCase("showGridViewDownloads"),
        TestCase("showGridViewDownloads").FilesSwa(),
        TestCase("showGridViewButtonSwitches"),
        TestCase("showGridViewButtonSwitches").FilesSwa(),
        TestCase("showGridViewKeyboardSelectionA11y"),
        TestCase("showGridViewKeyboardSelectionA11y").FilesSwa(),
        TestCase("showGridViewTitles"),
        TestCase("showGridViewTitles").FilesSwa(),
        TestCase("showGridViewMouseSelectionA11y"),
        TestCase("showGridViewMouseSelectionA11y").FilesSwa(),
        TestCase("showGridViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("showGridViewDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(
        TestCase("requestMount"),
        TestCase("requestMount").FilesSwa(),
        TestCase("requestMount").DisableNativeSmb(),
        TestCase("requestMount").DisableNativeSmb().FilesSwa(),
        TestCase("requestMountMultipleMounts"),
        TestCase("requestMountMultipleMounts").FilesSwa(),
        TestCase("requestMountMultipleMounts").DisableNativeSmb(),
        TestCase("requestMountMultipleMounts").DisableNativeSmb().FilesSwa(),
        TestCase("requestMountSourceDevice"),
        TestCase("requestMountSourceDevice").FilesSwa(),
        TestCase("requestMountSourceDevice").DisableNativeSmb(),
        TestCase("requestMountSourceDevice").DisableNativeSmb().FilesSwa(),
        TestCase("requestMountSourceFile"),
        TestCase("requestMountSourceFile").FilesSwa(),
        TestCase("requestMountSourceFile").DisableNativeSmb(),
        TestCase("requestMountSourceFile").DisableNativeSmb().FilesSwa(),
        TestCase("providerEject"),
        TestCase("providerEject").FilesSwa(),
        TestCase("providerEject").DisableNativeSmb(),
        TestCase("providerEject").DisableNativeSmb().FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads"),
        TestCase("showHiddenFilesDownloads").FilesSwa(),
        TestCase("showHiddenFilesDownloads").InGuestMode(),
        TestCase("showHiddenFilesDownloads").InGuestMode().FilesSwa(),
        TestCase("showHiddenFilesDrive"),
        TestCase("showHiddenFilesDrive").FilesSwa(),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showPasteIntoCurrentFolder").FilesSwa(),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("showSelectAllInCurrentFolder").FilesSwa(),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles")
            .FilesSwa(),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles").FilesSwa(),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders")
            .FilesSwa(),
        TestCase("newFolderInDownloads"),
        TestCase("newFolderInDownloads").FilesSwa(),
        TestCase("showSendFeedbackAction"),
        TestCase("showSendFeedbackAction").FilesSwa(),
        TestCase("enableDisableStorageSettingsLink"),
        TestCase("enableDisableStorageSettingsLink").FilesSwa(),
        TestCase("showAvailableStorageMyFiles"),
        TestCase("showAvailableStorageMyFiles").FilesSwa(),
        // Disabled until Drive quota can be properly displayed.
        // crbug.com/1177203
        // TestCase("showAvailableStorageDrive"),
        // TestCase("showAvailableStorageDrive").FilesSwa(),
        TestCase("showAvailableStorageSmbfs"),
        TestCase("showAvailableStorageSmbfs").FilesSwa(),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("openHelpPageFromDownloadsVolume"),
        TestCase("openHelpPageFromDownloadsVolume").FilesSwa(),
        TestCase("openHelpPageFromDriveVolume"),
        TestCase("openHelpPageFromDriveVolume").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("filesTooltipFocus"),
        TestCase("filesTooltipFocus").FilesSwa(),
        TestCase("filesTooltipLabelChange"),
        TestCase("filesTooltipLabelChange").FilesSwa(),
        TestCase("filesTooltipMouseOver"),
        TestCase("filesTooltipMouseOver").FilesSwa(),
        TestCase("filesTooltipClickHides"),
        TestCase("filesTooltipClickHides").FilesSwa(),
        TestCase("filesTooltipHidesOnWindowResize"),
        // TODO(b/189173190): Add SWA OnWindowResize test using window.resizeTo.
        TestCase("filesCardTooltipClickHides"),
        TestCase("filesCardTooltipClickHides").FilesSwa(),
        TestCase("filesTooltipHidesOnDeleteDialogClosed"),
        TestCase("filesTooltipHidesOnDeleteDialogClosed").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileListAriaAttributes"),
        TestCase("fileListAriaAttributes").FilesSwa(),
        TestCase("fileListFocusFirstItem"),
        TestCase("fileListFocusFirstItem").FilesSwa(),
        TestCase("fileListSelectLastFocusedItem"),
        TestCase("fileListSelectLastFocusedItem").FilesSwa(),
        TestCase("fileListKeyboardSelectionA11y"),
        TestCase("fileListKeyboardSelectionA11y").FilesSwa(),
        TestCase("fileListMouseSelectionA11y"),
        TestCase("fileListMouseSelectionA11y").FilesSwa(),
        TestCase("fileListDeleteMultipleFiles"),
        TestCase("fileListDeleteMultipleFiles").FilesSwa(),
        TestCase("fileListDeleteMultipleFiles").EnableTrash(),
        TestCase("fileListDeleteMultipleFiles").EnableTrash().FilesSwa(),
        TestCase("fileListRenameSelectedItem"),
        TestCase("fileListRenameSelectedItem").FilesSwa(),
        TestCase("fileListRenameFromSelectAll"),
        TestCase("fileListRenameFromSelectAll").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("mountCrostini"),
        TestCase("mountCrostini").FilesSwa(),
        TestCase("enableDisableCrostini"),
        TestCase("enableDisableCrostini").FilesSwa(),
        TestCase("sharePathWithCrostini"),
        TestCase("sharePathWithCrostini").FilesSwa(),
        TestCase("pluginVmDirectoryNotSharedErrorDialog"),
        TestCase("pluginVmDirectoryNotSharedErrorDialog").FilesSwa(),
        TestCase("pluginVmFileOnExternalDriveErrorDialog"),
        TestCase("pluginVmFileOnExternalDriveErrorDialog").FilesSwa(),
        TestCase("pluginVmFileDropFailErrorDialog"),
        TestCase("pluginVmFileDropFailErrorDialog").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh"),
        TestCase("directoryTreeRefresh").FilesSwa(),
        TestCase("showMyFiles"),
        TestCase("showMyFiles").FilesSwa(),
        TestCase("showMyFiles").EnableTrash(),
        TestCase("showMyFiles").EnableTrash().FilesSwa(),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesDisplaysAndOpensEntries").FilesSwa(),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesFolderRename").FilesSwa(),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts").DontMountVolumes(),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts")
            .DontMountVolumes()
            .FilesSwa(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesUpdatesChildren").FilesSwa(),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesAutoExpandOnce").FilesSwa(),
        TestCase("myFilesToolbarDelete"),
        TestCase("myFilesToolbarDelete").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog"),
                      TestCase("installLinuxPackageDialog").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("recentsA11yMessages").EnableFiltersInRecents(),
        TestCase("recentsA11yMessages").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsDownloads"),
        TestCase("recentsDownloads").FilesSwa(),
        TestCase("recentsDownloads").EnableFiltersInRecents(),
        TestCase("recentsDownloads").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsDrive"),
        TestCase("recentsDrive").FilesSwa(),
        TestCase("recentsDrive").EnableFiltersInRecents(),
        TestCase("recentsDrive").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniNotMounted").FilesSwa(),
        TestCase("recentsCrostiniNotMounted").EnableFiltersInRecents(),
        TestCase("recentsCrostiniNotMounted")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsCrostiniMounted").FilesSwa(),
        TestCase("recentsCrostiniMounted").EnableFiltersInRecents(),
        TestCase("recentsCrostiniMounted").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDrive").FilesSwa(),
        TestCase("recentsDownloadsAndDrive").EnableFiltersInRecents(),
        TestCase("recentsDownloadsAndDrive")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentsDownloadsAndDriveAndPlayFiles").EnableArc().FilesSwa(),
        TestCase("recentsDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents(),
        TestCase("recentsDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentsDownloadsAndDriveWithOverlap").FilesSwa(),
        TestCase("recentsDownloadsAndDriveWithOverlap")
            .EnableFiltersInRecents(),
        TestCase("recentsDownloadsAndDriveWithOverlap")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentsFilterResetToAll").EnableFiltersInRecents(),
        TestCase("recentsFilterResetToAll").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsNested"),
        TestCase("recentsNested").FilesSwa(),
        TestCase("recentsNested").EnableFiltersInRecents(),
        TestCase("recentsNested").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentsPlayFiles").EnableArc(),
        TestCase("recentsPlayFiles").EnableArc().FilesSwa(),
        TestCase("recentsPlayFiles").EnableArc().EnableFiltersInRecents(),
        TestCase("recentsPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentAudioDownloads"),
        TestCase("recentAudioDownloads").FilesSwa(),
        TestCase("recentAudioDownloads").EnableFiltersInRecents(),
        TestCase("recentAudioDownloads").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentAudioDownloadsAndDrive"),
        TestCase("recentAudioDownloadsAndDrive").FilesSwa(),
        TestCase("recentAudioDownloadsAndDrive").EnableFiltersInRecents(),
        TestCase("recentAudioDownloadsAndDrive")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .FilesSwa(),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents(),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentDocumentsDownloads")
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2(),
        TestCase("recentDocumentsDownloads")
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2()
            .FilesSwa(),
        TestCase("recentDocumentsDownloadsAndDrive")
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2(),
        TestCase("recentDocumentsDownloadsAndDrive")
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2()
            .FilesSwa(),
        TestCase("recentDocumentsDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2(),
        TestCase("recentDocumentsDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .EnableFiltersInRecentsV2()
            .FilesSwa(),
        TestCase("recentImagesDownloads"),
        TestCase("recentImagesDownloads").FilesSwa(),
        TestCase("recentImagesDownloads").EnableFiltersInRecents(),
        TestCase("recentImagesDownloads").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentImagesDownloadsAndDrive"),
        TestCase("recentImagesDownloadsAndDrive").FilesSwa(),
        TestCase("recentImagesDownloadsAndDrive").EnableFiltersInRecents(),
        TestCase("recentImagesDownloadsAndDrive")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .FilesSwa(),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents(),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentVideosDownloads"),
        TestCase("recentVideosDownloads").FilesSwa(),
        TestCase("recentVideosDownloads").EnableFiltersInRecents(),
        TestCase("recentVideosDownloads").EnableFiltersInRecents().FilesSwa(),
        TestCase("recentVideosDownloadsAndDrive"),
        TestCase("recentVideosDownloadsAndDrive").FilesSwa(),
        TestCase("recentVideosDownloadsAndDrive").EnableFiltersInRecents(),
        TestCase("recentVideosDownloadsAndDrive")
            .EnableFiltersInRecents()
            .FilesSwa(),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .FilesSwa(),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents(),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecents()
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("metadataDownloads"),
        TestCase("metadataDownloads").FilesSwa(),
        TestCase("metadataDrive"),
        TestCase("metadataDrive").FilesSwa(),
        TestCase("metadataTeamDrives"),
        TestCase("metadataTeamDrives").FilesSwa(),
        TestCase("metadataLargeDrive"),
        TestCase("metadataLargeDrive").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithResults").FilesSwa(),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsWithNoResults").FilesSwa(),
                      TestCase("searchDownloadsClearSearchKeyDown"),
                      TestCase("searchDownloadsClearSearchKeyDown").FilesSwa(),
                      TestCase("searchDownloadsClearSearch"),
                      TestCase("searchDownloadsClearSearch").FilesSwa(),
                      TestCase("searchHidingViaTab"),
                      TestCase("searchHidingViaTab").FilesSwa(),
                      TestCase("searchHidingTextEntryField"),
                      TestCase("searchHidingTextEntryField").FilesSwa(),
                      TestCase("searchButtonToggles"),
                      TestCase("searchButtonToggles").FilesSwa(),
                      TestCase("searchQueryLaunchParam")
                      // TODO(b/189173190): Enable
                      // TestCase("searchQueryLaunchParam").FilesSwa()
                      ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum"),
                      TestCase("metricsRecordEnum").FilesSwa(),
                      TestCase("metricsOpenSwa").FilesSwa(),
                      TestCase("metricsRecordDirectoryListLoad")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("breadcrumbsNavigate"),
        TestCase("breadcrumbsNavigate").FilesSwa(),
        TestCase("breadcrumbsDownloadsTranslation"),
        TestCase("breadcrumbsDownloadsTranslation").FilesSwa(),
        TestCase("breadcrumbsRenderShortPath"),
        TestCase("breadcrumbsRenderShortPath").FilesSwa(),
        TestCase("breadcrumbsEliderButtonHidden"),
        TestCase("breadcrumbsEliderButtonHidden").FilesSwa(),
        TestCase("breadcrumbsRenderLongPath"),
        TestCase("breadcrumbsRenderLongPath").FilesSwa(),
        TestCase("breadcrumbsMainButtonClick"),
        TestCase("breadcrumbsMainButtonClick").FilesSwa(),
        TestCase("breadcrumbsMainButtonEnterKey"),
        TestCase("breadcrumbsMainButtonEnterKey").FilesSwa(),
        TestCase("breadcrumbsEliderButtonClick"),
        TestCase("breadcrumbsEliderButtonClick").FilesSwa(),
        TestCase("breadcrumbsEliderButtonKeyboard"),
        TestCase("breadcrumbsEliderButtonKeyboard").FilesSwa(),
        TestCase("breadcrumbsEliderMenuClickOutside"),
        TestCase("breadcrumbsEliderMenuClickOutside").FilesSwa(),
        TestCase("breadcrumbsEliderMenuItemClick"),
        TestCase("breadcrumbsEliderMenuItemClick").FilesSwa(),
        TestCase("breadcrumbsEliderMenuItemTabLeft"),
        TestCase("breadcrumbsEliderMenuItemTabLeft").FilesSwa(),
        TestCase("breadcrumbsEliderMenuItemTabRight"),
        TestCase("breadcrumbsEliderMenuItemTabRight").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("formatDialog"),
        TestCase("formatDialog").FilesSwa(),
        TestCase("formatDialogIsModal"),
        TestCase("formatDialogIsModal").FilesSwa(),
        TestCase("formatDialogEmpty"),
        TestCase("formatDialogEmpty").FilesSwa(),
        TestCase("formatDialogCancel"),
        TestCase("formatDialogCancel").FilesSwa(),
        TestCase("formatDialogNameLength"),
        TestCase("formatDialogNameLength").FilesSwa(),
        TestCase("formatDialogNameInvalid"),
        TestCase("formatDialogNameInvalid").FilesSwa(),
        TestCase("formatDialogGearMenu"),
        TestCase("formatDialogGearMenu").FilesSwa(),
        TestCase("formatDialog").EnableSinglePartitionFormat(),
        TestCase("formatDialog").EnableSinglePartitionFormat().FilesSwa(),
        TestCase("formatDialogIsModal").EnableSinglePartitionFormat(),
        TestCase("formatDialogIsModal")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
        TestCase("formatDialogEmpty").EnableSinglePartitionFormat(),
        TestCase("formatDialogEmpty").EnableSinglePartitionFormat().FilesSwa(),
        TestCase("formatDialogCancel").EnableSinglePartitionFormat(),
        TestCase("formatDialogCancel").EnableSinglePartitionFormat().FilesSwa(),
        TestCase("formatDialogNameLength").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameLength")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
        TestCase("formatDialogNameInvalid").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameInvalid")
            .EnableSinglePartitionFormat()
            .FilesSwa(),
        TestCase("formatDialogGearMenu").EnableSinglePartitionFormat(),
        TestCase("formatDialogGearMenu")
            .EnableSinglePartitionFormat()
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Trash, /* trash.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("trashMoveToTrash").EnableTrash(),
        TestCase("trashMoveToTrash").EnableTrash().FilesSwa(),
        TestCase("trashPermanentlyDelete").EnableTrash(),
        TestCase("trashPermanentlyDelete").EnableTrash().FilesSwa(),
        TestCase("trashRestoreFromToast").EnableTrash(),
        TestCase("trashRestoreFromToast").EnableTrash().FilesSwa(),
        TestCase("trashRestoreFromTrash").EnableTrash(),
        TestCase("trashRestoreFromTrash").EnableTrash().FilesSwa(),
        TestCase("trashRestoreFromTrashShortcut").EnableTrash(),
        TestCase("trashRestoreFromTrashShortcut").EnableTrash().FilesSwa(),
        TestCase("trashEmptyTrash").EnableTrash(),
        // TODO(b/189173190): Enable
        // TestCase("trashEmptyTrash").EnableTrash().FilesSwa(),
        TestCase("trashEmptyTrashShortcut").EnableTrash(),
        // TODO(b/189173190): Enable
        // TestCase("trashEmptyTrashShortcut").EnableTrash().FilesSwa(),
        TestCase("trashDeleteFromTrash").EnableTrash()
        // TODO(b/189173190): Enable
        // TestCase("trashDeleteFromTrash").EnableTrash().FilesSwa()
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    AndroidPhotos, /* android_photos.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("androidPhotosBanner").EnablePhotosDocumentsProvider()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Office, /* office.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openOfficeWordFile").EnableWebDriveOffice(),
        TestCase("openOfficeWordFile").EnableWebDriveOffice().FilesSwa(),
        TestCase("openOfficeWordFromMyFiles").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromMyFiles").EnableWebDriveOffice().FilesSwa(),
        TestCase("openMultipleOfficeWordFromDrive").EnableWebDriveOffice(),
        TestCase("openMultipleOfficeWordFromDrive")
            .EnableWebDriveOffice()
            .FilesSwa(),
        TestCase("openOfficeWordFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromDrive").EnableWebDriveOffice().FilesSwa(),
        TestCase("openOfficeExcelFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficeExcelFromDrive").EnableWebDriveOffice().FilesSwa(),
        TestCase("openOfficePowerPointFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficePowerPointFromDrive")
            .EnableWebDriveOffice()
            .FilesSwa(),
        TestCase("openOfficeWordFromDriveNotSynced").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromDriveNotSynced")
            .EnableWebDriveOffice()
            .FilesSwa(),
        TestCase("openOfficeWordFromDriveOffline")
            .EnableWebDriveOffice()
            .Offline(),
        TestCase("openOfficeWordFromDriveOffline")
            .EnableWebDriveOffice()
            .Offline()
            .FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GuestOs, /* guest_os.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fakesListed").EnableGuestOsFiles(),
        TestCase("listUpdatedWhenGuestsChanged").EnableGuestOsFiles(),
        TestCase("mountGuestSuccess").EnableGuestOsFiles(),
        TestCase("notListedWithoutFlag")));

}  // namespace file_manager
