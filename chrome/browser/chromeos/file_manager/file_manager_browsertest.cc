// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
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

  // TODO(crbug.com/912236) Remove once transition to new ZIP system is done.
  TestCase& ZipNoNaCl() {
    options.zip_no_nacl = true;
    return *this;
  }

  TestCase& EnableDriveDssPin() {
    options.drive_dss_pin = true;
    return *this;
  }

  TestCase& EnableSharesheet() {
    options.enable_sharesheet = true;
    return *this;
  }

  TestCase& DisableSharesheet() {
    options.enable_sharesheet = false;
    return *this;
  }

  TestCase& EnableTrash() {
    options.enable_trash = true;
    return *this;
  }

  TestCase& EnableHoldingSpace(bool enable) {
    options.enable_holding_space = enable;
    return *this;
  }

  TestCase& DisableJsModules() {
    options.enable_js_modules = false;
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

    if (options.zip_no_nacl)
      full_name += "_ZipNoNaCl";

    if (options.drive_dss_pin)
      full_name += "_DriveDssPin";

    if (options.single_partition_format)
      full_name += "_SinglePartitionFormat";

    if (!options.enable_js_modules)
      full_name += "_NonJsModule";

    if (options.enable_trash)
      full_name += "_Trash";

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
        TestCase("fileDisplayDownloads").DisableJsModules(),
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDrive").DisableJsModules(),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsb").DisableJsModules(),
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
                      TestCase("audioOpenDownloads").DisableJsModules(),
                      TestCase("audioOpenDrive"),
                      TestCase("audioAutoAdvanceDrive"),
                      TestCase("audioRepeatAllModeSingleFileDrive"),
                      TestCase("audioNoRepeatModeSingleFileDrive"),
                      TestCase("audioRepeatOneModeSingleFileDrive"),
                      TestCase("audioRepeatAllModeMultipleFileDrive"),
                      TestCase("audioNoRepeatModeMultipleFileDrive"),
                      TestCase("audioRepeatOneModeMultipleFileDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("imageOpenMediaAppDownloads").MediaSwa().InGuestMode(),
        TestCase("imageOpenMediaAppDownloads").MediaSwa().DisableJsModules(),
        TestCase("imageOpenMediaAppDownloads").MediaSwa(),
        TestCase("imageOpenMediaAppDrive").MediaSwa()));

// TODO(crbug/1030935): Remove these tests when removing Gallery, the equivalent
// coverage for MediaApp is provided by the tests above.
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageFiles, /* open_image_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenDownloads").InGuestMode(),
                      TestCase("imageOpenDownloads"),
                      TestCase("imageOpenDownloads").DisableJsModules(),
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
                      ZipCase("zipNotifyFileTasks").ZipNoNaCl(),
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
                      TestCase("selectCreateFolderDownloads"),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderDownloads").DisableJsModules(),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("keyboardDeleteDownloads").InGuestMode(),
        TestCase("keyboardDeleteDownloads"),
        TestCase("keyboardDeleteDownloads").DisableJsModules(),
        TestCase("keyboardDeleteDownloads").EnableTrash(),
        TestCase("keyboardDeleteDrive"),
        TestCase("keyboardDeleteDrive").EnableTrash(),
        TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
        TestCase("keyboardDeleteFolderDownloads"),
        TestCase("keyboardDeleteFolderDownloads").EnableTrash(),
        TestCase("keyboardDeleteFolderDrive"),
        TestCase("keyboardCopyDownloads").InGuestMode(),
        TestCase("keyboardCopyDownloads"),
        TestCase("keyboardCopyDownloads").EnableTrash(),
        TestCase("keyboardCopyDrive"),
        TestCase("keyboardFocusOutlineVisible"),
        TestCase("keyboardFocusOutlineVisible").EnableTrash(),
        TestCase("keyboardFocusOutlineVisibleMouse"),
        TestCase("keyboardFocusOutlineVisibleMouse").EnableTrash(),
        TestCase("keyboardSelectDriveDirectoryTree"),
        TestCase("keyboardDisableCopyWhenDialogDisplayed"),
        TestCase("keyboardOpenNewWindow"),
        TestCase("keyboardOpenNewWindow").InGuestMode(),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDrive"),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameNewFolderDownloads"),
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
        TestCase("checkTrashContextMenu").EnableTrash(),
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
        TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
        TestCase("toolbarDeleteButtonKeepFocus"),
        TestCase("toolbarDeleteEntry").InGuestMode(),
        TestCase("toolbarDeleteEntry"),
        TestCase("toolbarDeleteEntry").DisableJsModules(),
        TestCase("toolbarDeleteEntry").EnableTrash(),
        TestCase("toolbarRefreshButtonWithSelection").EnableArc(),
        TestCase("toolbarAltACommand"),
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
        TestCase("openQuickView"),
        TestCase("openQuickView").DisableJsModules(),
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
        TestCase("openQuickViewPdfPopup"),
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
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
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
        TestCase("openQuickViewTabIndexDeleteDialog").EnableTrash(),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection"),
        TestCase("openQuickViewAndDeleteSingleSelection").EnableTrash(),
        TestCase("openQuickViewAndDeleteCheckSelection"),
        TestCase("openQuickViewAndDeleteCheckSelection").EnableTrash(),
        TestCase("openQuickViewDeleteEntireCheckSelection"),
        TestCase("openQuickViewDeleteEntireCheckSelection").EnableTrash(),
        TestCase("openQuickViewClickDeleteButton"),
        TestCase("openQuickViewClickDeleteButton").EnableTrash(),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewDeleteButtonNotShown").EnableTrash(),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory"),
        TestCase("directoryTreeSelectedDirectory"),
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
        TestCase("dirCopyWithContextMenu"),
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
        TestCase("dirContextMenuMyFiles").EnableTrash(),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuMyFilesWithPaste").EnableTrash(),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuCrostini").EnableTrash(),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuUsbDcim").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMediaView").EnableArc(),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuTrash").EnableTrash(),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline").EnableGenericDocumentsProvider(),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("driveAutoCompleteQuery"),
        TestCase("drivePinMultiple"),
        TestCase("drivePinHosted"),
        TestCase("drivePinFileMobileNetwork"),
        TestCase("drivePinToggleUpdatesInFakeEntries"),
        TestCase("driveClickFirstSearchResult"),
        TestCase("drivePressEnterToSearch"),
        TestCase("drivePressClearSearch"),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("driveBackupPhotos"),
        TestCase("driveBackupPhotos").EnableSinglePartitionFormat(),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkOpenFileThroughLinkedDirectory").MediaSwa(),
        TestCase("driveLinkOpenFileThroughTransitiveLink").MediaSwa(),
        TestCase("driveWelcomeBanner"),
        TestCase("driveOfflineInfoBanner").EnableDriveDssPin(),
        TestCase("driveOfflineInfoBannerWithoutFlag"),
        TestCase("driveEnableDocsOfflineDialog"),
        TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBannerWithFeatureDisabled")
            .EnableHoldingSpace(false),
        TestCase("holdingSpaceWelcomeBannerWithFeatureEnabled")
            .EnableHoldingSpace(true),
        TestCase("holdingSpaceWelcomeBannerWontShowAfterBeingDismissed")
            .EnableHoldingSpace(true),
        TestCase("holdingSpaceWelcomeBannerWontShowAfterReachingLimit")
            .EnableHoldingSpace(true),
        TestCase("holdingSpaceWelcomeBannerWontShowForModalDialogs")
            .EnableHoldingSpace(true)
            .WithBrowser(),
        TestCase("holdingSpaceWelcomeBannerWontShowOnDrive")
            .EnableHoldingSpace(true),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")
            .EnableHoldingSpace(true)));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDriveToDownloads").DisableJsModules(),
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
        TestCase("transferFromDownloadsToTeamDrive"),
        TestCase("transferBetweenTeamDrives"),
        TestCase("transferDragDropActiveLeave"),
        TestCase("transferDragDropActiveDrop"),
        TestCase("transferDragDropTreeItemAccepts"),
        TestCase("transferDragDropTreeItemDenies"),
        TestCase("transferDragAndHoverTreeItemEntryList"),
        TestCase("transferDragAndHoverTreeItemFakeEntry"),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat(),
        TestCase("transferDragFileListItemSelects"),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndHover"),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferDeletedFile"),
        TestCase("transferDeletedFile").EnableTrash(),
        TestCase("transferInfoIsRemembered"),
        TestCase("transferToUsbHasDestinationText"),
        TestCase("transferDismissedErrorIsRemembered"),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText"),
        TestCase("transferUpdateSamePanelItem"),
        TestCase("transferShowPendingMessageForZeroRemainingTime")));

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
    ::testing::Values(TestCase("shareFileDrive"),
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
                      TestCase("manageDirectoryTeamDrive"),
                      TestCase("manageTeamDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("executeDefaultTaskDownloads"),
        TestCase("executeDefaultTaskDownloads").InGuestMode(),
        TestCase("executeDefaultTaskDownloads").DisableJsModules(),
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
    ::testing::Values(TestCase("traverseFolderShortcuts"),
                      TestCase("addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusDirectorySelected").DisableSharesheet(),
        TestCase("tabindexFocusDirectorySelectedSharesheetEnabled")
            .EnableSharesheet(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("tabindexSaveFileDialogDrive").WithBrowser(),
        TestCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        TestCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().DisableJsModules(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogAriaMultipleSelect").WithBrowser(),
        TestCase("saveFileDialogAriaSingleSelect").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser().DisableJsModules(),
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
        TestCase("saveFileDialogDefaultFilterKeyNavigation").WithBrowser(),
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
    ::testing::Values(
        TestCase("copyBetweenWindowsLocalToDrive"),
        TestCase("copyBetweenWindowsLocalToUsb"),
        TestCase("copyBetweenWindowsLocalToUsb").DisableJsModules(),
        TestCase("copyBetweenWindowsUsbToDrive"),
        TestCase("copyBetweenWindowsDriveToLocal"),
        TestCase("copyBetweenWindowsDriveToUsb"),
        TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDownloads"),
                      TestCase("showGridViewDownloads").DisableJsModules(),
                      TestCase("showGridViewDrive"),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewTitles"),
                      TestCase("showGridViewMouseSelectionA11y"),
                      TestCase("showGridViewDocumentsProvider")
                          .EnableGenericDocumentsProvider()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMount").DisableNativeSmb(),
                      TestCase("requestMount").DisableJsModules(),
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
        // Disabled until Drive quota can be properly displayed.
        // crbug.com/1177203
        // TestCase("showAvailableStorageDrive"),
        TestCase("showAvailableStorageSmbfs").EnableSmbfs(),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides"),
                      TestCase("filesTooltipHidesOnDeleteDialogClosed")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("fileListAriaAttributes"),
                      TestCase("fileListFocusFirstItem"),
                      TestCase("fileListSelectLastFocusedItem"),
                      TestCase("fileListKeyboardSelectionA11y"),
                      TestCase("fileListMouseSelectionA11y"),
                      TestCase("fileListDeleteMultipleFiles"),
                      TestCase("fileListDeleteMultipleFiles").EnableTrash(),
                      TestCase("fileListRenameSelectedItem"),
                      TestCase("fileListRenameFromSelectAll")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostini"),
                      TestCase("enableDisableCrostini"),
                      TestCase("sharePathWithCrostini"),
                      TestCase("pluginVmDirectoryNotSharedErrorDialog"),
                      TestCase("pluginVmFileOnExternalDriveErrorDialog"),
                      TestCase("pluginVmFileDropFailErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh"),
        TestCase("showMyFiles"),
        TestCase("showMyFiles").EnableTrash(),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts").DontMountVolumes(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

#if !defined(OFFICIAL_BUILD)
WRAPPED_INSTANTIATE_TEST_SUITE_P(
    LaunchFilesAppSwa, /* launch_files_app_swa.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("launchFilesAppSwa").FilesSwa(),
                      TestCase("launchFilesAppSwa").FilesSwa().InGuestMode()));
#endif

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    LauncherSearch, /* launcher_search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("launcherOpenSearchResult").MediaSwa(),
                      TestCase("launcherSearch"),
                      TestCase("launcherSearch").DisableJsModules(),
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
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive"),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithResults").DisableJsModules(),
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

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate"),
                      TestCase("breadcrumbsNavigate").DisableJsModules(),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsRenderShortPath"),
                      TestCase("breadcrumbsEliderButtonHidden"),
                      TestCase("breadcrumbsRenderLongPath"),
                      TestCase("breadcrumbsMainButtonClick"),
                      TestCase("breadcrumbsMainButtonEnterKey"),
                      TestCase("breadcrumbsEliderButtonClick"),
                      TestCase("breadcrumbsEliderButtonKeyboard"),
                      TestCase("breadcrumbsEliderMenuClickOutside"),
                      TestCase("breadcrumbsEliderMenuItemClick"),
                      TestCase("breadcrumbsEliderMenuItemTabLeft"),
                      TestCase("breadcrumbsEliderMenuItemTabRight")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("formatDialog"),
        TestCase("formatDialog").DisableJsModules(),
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
    ::testing::Values(TestCase("trashMoveToTrash").EnableTrash(),
                      TestCase("trashPermanentlyDelete").EnableTrash(),
                      TestCase("trashRestoreFromToast").EnableTrash(),
                      TestCase("trashRestoreFromTrash").EnableTrash(),
                      TestCase("trashRestoreFromTrashShortcut").EnableTrash(),
                      TestCase("trashEmptyTrash").EnableTrash(),
                      TestCase("trashEmptyTrashShortcut").EnableTrash(),
                      TestCase("trashDeleteFromTrash").EnableTrash()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    AndroidPhotos, /* android_photos.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("androidPhotosBanner").EnablePhotosDocumentsProvider()));

}  // namespace file_manager
