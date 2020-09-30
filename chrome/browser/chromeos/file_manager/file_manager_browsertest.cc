// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
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

  TestCase& EnableDocumentsProvider() {
    options.arc = true;
    options.documents_provider = true;
    return *this;
  }

  TestCase& DisableDocumentsProvider() {
    options.documents_provider = false;
    return *this;
  }

  TestCase& EnableArc() {
    options.arc = true;
    return *this;
  }

  TestCase& Offline() {
    options.offline = true;
    return *this;
  }

  TestCase& FilesNg() {
    options.files_ng = true;
    return *this;
  }

  TestCase& DisableFilesNg() {
    options.files_ng = false;
    return *this;
  }

  TestCase& DisableNativeSmb() {
    options.native_smb = false;
    return *this;
  }

  TestCase& EnableSmbfs() {
    options.smbfs = true;
    return *this;
  }

  TestCase& EnableUnifiedMediaView() {
    options.unified_media_view = true;
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

  // TODO(crbug.com/912236) Remove once transition to new ZIP system is done.
  TestCase& ZipNoNaCl() {
    options.zip_no_nacl = true;
    return *this;
  }

  TestCase& EnableSharesheet() {
    options.enable_sharesheet = true;
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

    if (!options.files_ng)
      full_name += "_DisableFilesNg";

    if (!options.native_smb)
      full_name += "_DisableNativeSmb";

    if (options.documents_provider)
      full_name += "_DocumentsProvider";

    if (options.zip_no_nacl)
      full_name += "_ZipNoNaCl";

    return full_name;
  }

  const char* const name;
  FileManagerBrowserTestBase::Options options;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  return out << test_case.options;
}

// FilesAppBrowserTest with zip/unzip support.
TestCase ZipCase(const char* const name) {
  TestCase test_case(name);
  test_case.options.zip = true;
  return test_case;
}

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(FilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// Files app tests that require SWA (System Web Apps).
class SWAsFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  SWAsFilesAppBrowserTest() = default;
  SWAsFilesAppBrowserTest(const SWAsFilesAppBrowserTest&) = delete;
  SWAsFilesAppBrowserTest& operator=(const SWAsFilesAppBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    web_app::WebAppProvider::Get(profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();

    FilesAppBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_P(SWAsFilesAppBrowserTest, Test) {
  StartTest();
}

// A version of the FilesAppBrowserTest that supports spanning browser restart
// to allow testing prefs and other things.
class ExtendedFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  ExtendedFilesAppBrowserTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtendedFilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, PRE_Test) {
  profile()->GetPrefs()->SetBoolean(prefs::kNetworkFileSharesAllowed,
                                    GetOptions().native_smb);
}

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, Test) {
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
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsbPartition"),
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
        TestCase("fileDisplayStartupError")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoFiles, /* open_video_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioFiles, /* open_audio_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenCloseDownloads"),
                      TestCase("audioOpenCloseDownloads").InGuestMode(),
                      TestCase("audioOpenCloseDrive"),
                      TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive"),
                      TestCase("audioAutoAdvanceDrive"),
                      TestCase("audioRepeatAllModeSingleFileDrive"),
                      TestCase("audioNoRepeatModeSingleFileDrive"),
                      TestCase("audioRepeatOneModeSingleFileDrive"),
                      TestCase("audioRepeatAllModeMultipleFileDrive"),
                      TestCase("audioNoRepeatModeMultipleFileDrive"),
                      TestCase("audioRepeatOneModeMultipleFileDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageBacklight, /* open_image_backlight.js */
    SWAsFilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenBacklight").InGuestMode(),
                      TestCase("imageOpenBacklight")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageFiles, /* open_image_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenDownloads").InGuestMode(),
                      TestCase("imageOpenDownloads"),
                      TestCase("imageOpenDrive"),
                      TestCase("imageOpenGalleryOpenDownloads"),
                      TestCase("imageOpenGalleryOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive"),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive")));

// NaCl fails to compile zip plugin.pexe too often on ASAN, crbug.com/867738
// The tests are flaky on the debug bot and always time out first and then pass
// on retry. Disabled for debug as per crbug.com/936429.
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_ZipFiles DISABLED_ZipFiles
#else
#define MAYBE_ZipFiles ZipFiles
#endif
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MAYBE_ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(ZipCase("zipFileOpenDownloads").InGuestMode(),
                      ZipCase("zipFileOpenDownloads"),
                      ZipCase("zipFileOpenDownloadsShiftJIS"),
                      ZipCase("zipFileOpenDownloadsMacOs"),
                      ZipCase("zipFileOpenDownloadsWithAbsolutePaths"),
                      ZipCase("zipFileOpenDownloadsEncryptedCancelPassphrase"),
                      ZipCase("zipFileOpenDrive"),
                      ZipCase("zipFileOpenUsb"),
                      ZipCase("zipCannotZipFile").ZipNoNaCl(),
                      ZipCase("zipCannotZipFile").ZipNoNaCl().InGuestMode(),
                      ZipCase("zipCreateFileDownloads").InGuestMode(),
                      ZipCase("zipCreateFileDownloads"),
                      ZipCase("zipCreateFileDrive"),
                      ZipCase("zipCreateFileUsb")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("selectCreateFolderDownloads").FilesNg(),
                      TestCase("selectCreateFolderDownloads").DisableFilesNg(),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("keyboardDeleteDownloads").InGuestMode(),
                      TestCase("keyboardDeleteDownloads").FilesNg(),
                      TestCase("keyboardDeleteDownloads").DisableFilesNg(),
                      TestCase("keyboardDeleteDrive"),
                      TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
                      TestCase("keyboardDeleteFolderDownloads"),
                      TestCase("keyboardDeleteFolderDrive"),
                      TestCase("keyboardCopyDownloads").InGuestMode(),
                      TestCase("keyboardCopyDownloads"),
                      TestCase("keyboardCopyDrive"),
                      TestCase("keyboardFocusOutlineVisible"),
                      TestCase("keyboardFocusOutlineVisibleMouse"),
                      TestCase("keyboardSelectDriveDirectoryTree"),
                      TestCase("keyboardDisableCopyWhenDialogDisplayed"),
                      TestCase("keyboardOpenNewWindow"),
                      TestCase("keyboardOpenNewWindow").InGuestMode(),
                      TestCase("renameFileDownloads").InGuestMode(),
                      TestCase("renameFileDownloads"),
                      TestCase("renameFileDrive"),
                      TestCase("renameNewFolderDownloads").InGuestMode(),
                      TestCase("renameNewFolderDownloads").FilesNg(),
                      TestCase("renameNewFolderDownloads").DisableFilesNg(),
                      TestCase("renameNewFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
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
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkImportCrostiniImageEnabled"),
        TestCase("checkImportCrostiniImageDisabled"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider").EnableDocumentsProvider(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkDeleteDisabledInRecents").EnableUnifiedMediaView(),
        TestCase("checkGoToFileLocationEnabledInRecents")
            .EnableUnifiedMediaView(),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection")
            .EnableUnifiedMediaView()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
        TestCase("toolbarDeleteButtonKeepFocus"),
        TestCase("toolbarDeleteEntry").InGuestMode(),
        TestCase("toolbarDeleteEntry"),
        TestCase("toolbarRefreshButtonWithSelection").EnableArc(),
        TestCase("toolbarAltACommand").FilesNg(),
        TestCase("toolbarRefreshButtonHiddenInRecents"),
        TestCase("toolbarMultiMenuFollowsButton"),
        TestCase("toolbarSharesheetButtonWithSelection").EnableSharesheet(),
        TestCase("toolbarSharesheetContextMenuWithSelection")
            .EnableSharesheet(),
        TestCase("toolbarSharesheetNoEntrySelected").EnableSharesheet()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView").DisableFilesNg(),
        TestCase("openQuickView").FilesNg(),
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
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdfPreviewsDisabled"),
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
        TestCase("openQuickViewSmbfs").EnableSmbfs(),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewDocumentsProvider").EnableDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewTabIndexImage"),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexHtml"),
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexVideo"),
        TestCase("openQuickViewTabIndexDeleteDialog"),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection"),
        TestCase("openQuickViewAndDeleteCheckSelection"),
        TestCase("openQuickViewDeleteEntireCheckSelection"),
        TestCase("openQuickViewClickDeleteButton"),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory").DisableFilesNg(),
        TestCase("directoryTreeActiveDirectory").FilesNg(),
        TestCase("directoryTreeSelectedDirectory").DisableFilesNg(),
        TestCase("directoryTreeSelectedDirectory").FilesNg(),
        TestCase("directoryTreeRecentsSubtypeScroll").EnableUnifiedMediaView(),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeExpandFolder"),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff"),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu").FilesNg(),
        TestCase("dirCopyWithContextMenu").DisableFilesNg(),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithoutChangingCurrent"),
        TestCase("dirCutWithContextMenu").InGuestMode(),
        TestCase("dirCutWithContextMenu"),
        TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirCutWithKeyboard"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToExisting").InGuestMode(),
        TestCase("dirRenameToExisting"),
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
        TestCase("dirCreateMultipleFolders"),
#if !(defined(ADDRESS_SANITIZER) || !defined(NDEBUG))
        // Zip tests times out too often on ASAN and DEBUG. crbug.com/936429
        // and crbug.com/944697
        ZipCase("dirContextMenuZip"),
        ZipCase("dirEjectContextMenuZip"),
#endif
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider").EnableDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMediaView").EnableArc(),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("driveOpenSidebarOffline"),
                      TestCase("driveOpenSidebarSharedWithMe"),
                      TestCase("driveAutoCompleteQuery"),
                      TestCase("drivePinMultiple"),
                      TestCase("drivePinHosted"),
                      TestCase("drivePinFileMobileNetwork"),
                      TestCase("driveClickFirstSearchResult"),
                      TestCase("drivePressEnterToSearch"),
                      TestCase("drivePressClearSearch"),
                      TestCase("drivePressCtrlAFromSearch"),
                      TestCase("driveBackupPhotos"),
                      TestCase("driveAvailableOfflineGearMenu"),
                      TestCase("driveAvailableOfflineDirectoryGearMenu"),
                      TestCase("driveAvailableOfflineActionBar"),
                      TestCase("driveLinkToDirectory"),
                      TestCase("driveLinkOpenFileThroughLinkedDirectory"),
                      TestCase("driveLinkOpenFileThroughTransitiveLink"),
                      TestCase("driveWelcomeBanner")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDownloadsToMyFiles"),
        TestCase("transferFromDownloadsToMyFilesMove"),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromSharedToDownloads"),
        TestCase("transferFromSharedToDrive"),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDrive"),
        TestCase("transferFromTeamDriveToDrive"),
        TestCase("transferFromDriveToTeamDrive"),
        TestCase("transferFromTeamDriveToDownloads"),
        TestCase("transferHostedFileFromTeamDriveToDownloads"),
        TestCase("transferFromDownloadsToTeamDrive").DisableFilesNg(),
        TestCase("transferFromDownloadsToTeamDrive").FilesNg(),
        TestCase("transferBetweenTeamDrives").DisableFilesNg(),
        TestCase("transferBetweenTeamDrives").FilesNg(),
        TestCase("transferDragDropActiveLeave"),
        TestCase("transferDragDropActiveDrop"),
        TestCase("transferDragDropTreeItemAccepts").FilesNg(),
        TestCase("transferDragDropTreeItemDenies").FilesNg(),
        TestCase("transferDragAndHoverTreeItemEntryList"),
        TestCase("transferDragAndHoverTreeItemFakeEntry"),
        TestCase("transferDragFileListItemSelects"),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndHover"),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferDeletedFile"),
        TestCase("transferInfoIsRemembered"),
        TestCase("transferToUsbHasDestinationText"),
        TestCase("transferDismissedErrorIsRemembered")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
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
    ::testing::Values(TestCase("shareFileDrive").DisableFilesNg(),
                      TestCase("shareFileDrive").FilesNg(),
                      TestCase("shareDirectoryDrive"),
                      TestCase("shareHostedFileDrive"),
                      TestCase("manageHostedFileDrive"),
                      TestCase("manageFileDrive"),
                      TestCase("manageDirectoryDrive"),
                      TestCase("shareFileTeamDrive"),
                      TestCase("shareDirectoryTeamDrive"),
                      TestCase("shareHostedFileTeamDrive"),
                      TestCase("shareTeamDrive"),
                      TestCase("manageHostedFileTeamDrive"),
                      TestCase("manageFileTeamDrive"),
                      TestCase("manageDirectoryTeamDrive").DisableFilesNg(),
                      TestCase("manageDirectoryTeamDrive").FilesNg(),
                      TestCase("manageTeamDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SuggestAppDialog, /* suggest_app_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("suggestAppDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
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
                      TestCase("noActionBarOpenForDirectories")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts").DisableFilesNg(),
                      TestCase("traverseFolderShortcuts").FilesNg(),
                      TestCase("addRemoveFolderShortcuts").DisableFilesNg(),
                      TestCase("addRemoveFolderShortcuts").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus").FilesNg(),
        TestCase("tabindexSearchBoxFocus").DisableFilesNg(),
        TestCase("tabindexFocus").DisableFilesNg(),
        TestCase("tabindexFocusDownloads").FilesNg(),
        TestCase("tabindexFocusDownloads").DisableFilesNg(),
        TestCase("tabindexFocusDownloads").InGuestMode().FilesNg(),
        TestCase("tabindexFocusDownloads").InGuestMode().DisableFilesNg(),
        // TestCase("tabindexFocusBreadcrumbBackground").FilesNg(),
        TestCase("tabindexFocusBreadcrumbBackground").DisableFilesNg(),
        TestCase("tabindexFocusDirectorySelected").FilesNg(),
        TestCase("tabindexFocusDirectorySelected").DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloadsFilesNg").WithBrowser().FilesNg(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser().DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .DisableFilesNg(),
        TestCase("tabindexOpenDialogDownloadsFilesNg")
            .WithBrowser()
            .InGuestMode()
            .FilesNg(),
        TestCase("tabindexSaveFileDialogDriveFilesNg").WithBrowser().FilesNg(),
        TestCase("tabindexSaveFileDialogDrive").WithBrowser().DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloadsFilesNg")
            .WithBrowser()
            .FilesNg(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .DisableFilesNg(),
        TestCase("tabindexSaveFileDialogDownloadsFilesNg")
            .WithBrowser()
            .InGuestMode()
            .FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("saveFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDrive").WithBrowser(),
        TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled").WithBrowser()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("copyBetweenWindowsLocalToDrive"),
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      TestCase("copyBetweenWindowsDriveToUsb"),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads").DisableFilesNg(),
                      TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDownloads").FilesNg(),
                      TestCase("showGridViewDrive"),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewTitles").FilesNg(),
                      TestCase("showGridViewMouseSelectionA11y")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMount").DisableNativeSmb(),
                      TestCase("requestMountMultipleMounts"),
                      TestCase("requestMountMultipleMounts").DisableNativeSmb(),
                      TestCase("requestMountSourceDevice"),
                      TestCase("requestMountSourceDevice").DisableNativeSmb(),
                      TestCase("requestMountSourceFile"),
                      TestCase("requestMountSourceFile").DisableNativeSmb(),
                      TestCase("providerEject"),
                      TestCase("providerEject").DisableNativeSmb(),
                      TestCase("installNewServiceOnline"),
                      TestCase("installNewServiceOffline").Offline()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads"),
        TestCase("showHiddenFilesDownloads").InGuestMode(),
        TestCase("showHiddenFilesDrive"),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles"),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showSendFeedbackAction"),
        TestCase("enableDisableStorageSettingsLink"),
        TestCase("showAvailableStorageMyFiles"),
        TestCase("showAvailableStorageDrive"),
        TestCase("showAvailableStorageSmbfs").EnableSmbfs()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileListAriaAttributes").DisableFilesNg(),
        TestCase("fileListAriaAttributes").FilesNg(),
        TestCase("fileListFocusFirstItem").DisableFilesNg(),
        TestCase("fileListFocusFirstItem").FilesNg(),
        TestCase("fileListSelectLastFocusedItem").DisableFilesNg(),
        TestCase("fileListSelectLastFocusedItem").FilesNg(),
        TestCase("fileListKeyboardSelectionA11y").DisableFilesNg(),
        TestCase("fileListKeyboardSelectionA11y").FilesNg(),
        TestCase("fileListMouseSelectionA11y").DisableFilesNg(),
        TestCase("fileListMouseSelectionA11y").FilesNg(),
        TestCase("fileListDeleteMultipleFiles").DisableFilesNg(),
        TestCase("fileListDeleteMultipleFiles").FilesNg(),
        TestCase("fileListRenameFromSelectAll").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostini"),
                      TestCase("enableDisableCrostini"),
                      TestCase("sharePathWithCrostini"),
                      TestCase("pluginVmDirectoryNotSharedErrorDialog"),
                      TestCase("pluginVmFileOnExternalDriveErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh"),
        TestCase("showMyFiles").DisableFilesNg(),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts").DontMountVolumes(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce").DisableFilesNg(),
        TestCase("myFilesAutoExpandOnce").FilesNg(),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    LauncherSearch, /* launcher_search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("launcherOpenSearchResult"),
                      TestCase("launcherSearch"),
                      TestCase("launcherSearchOffline").Offline()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("recentsDownloads"),
        TestCase("recentsDrive"),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentsNested"),
        TestCase("recentAudioDownloads").EnableUnifiedMediaView(),
        TestCase("recentAudioDownloadsAndDrive").EnableUnifiedMediaView(),
        TestCase("recentImagesDownloads").EnableUnifiedMediaView(),
        TestCase("recentImagesDownloadsAndDrive").EnableUnifiedMediaView(),
        TestCase("recentVideosDownloads").EnableUnifiedMediaView(),
        TestCase("recentVideosDownloadsAndDrive").EnableUnifiedMediaView()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableDocumentsProvider(),
        TestCase("metadataDownloads").DisableFilesNg(),
        TestCase("metadataDownloads").FilesNg(),
        TestCase("metadataDrive").DisableFilesNg(),
        TestCase("metadataDrive").FilesNg(),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsClearSearchKeyDown"),
                      TestCase("searchDownloadsClearSearch"),
                      TestCase("searchHidingViaTab"),
                      TestCase("searchHidingTextEntryField"),
                      TestCase("searchButtonToggles"),
                      TestCase("searchQueryLaunchParam")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum")));

// TODO(adanilo) Remove 'breadcrumbsLeafNoFocus' when files-ng ships.
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate").DisableFilesNg(),
                      TestCase("breadcrumbsLeafNoFocus").DisableFilesNg(),
                      TestCase("breadcrumbsTooltip").DisableFilesNg(),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsRenderShortPath").FilesNg(),
                      TestCase("breadcrumbsEliderButtonHidden").FilesNg(),
                      TestCase("breadcrumbsRenderLongPath").FilesNg(),
                      TestCase("breadcrumbsMainButtonClick").FilesNg(),
                      TestCase("breadcrumbsMainButtonEnterKey").FilesNg(),
                      TestCase("breadcrumbsEliderButtonClick").FilesNg(),
                      TestCase("breadcrumbsEliderButtonKeyboard").FilesNg(),
                      TestCase("breadcrumbsEliderMenuClickOutside").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemClick").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemTabLeft").FilesNg(),
                      TestCase("breadcrumbsEliderMenuItemTabRight").FilesNg()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("formatDialog"),
                      TestCase("formatDialogEmpty"),
                      TestCase("formatDialogCancel"),
                      TestCase("formatDialogNameLength"),
                      TestCase("formatDialogNameInvalid"),
                      TestCase("formatDialogGearMenu")));

}  // namespace file_manager
