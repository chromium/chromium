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

  TestCase& EnableTrash() {
    options.enable_trash = true;
    return *this;
  }

  TestCase& EnableBannersFramework() {
    options.enable_banners_framework = true;
    return *this;
  }

  TestCase& DisableBannersFramework() {
    options.enable_banners_framework = false;
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

    if (options.enable_banners_framework)
      full_name += "_BannersFramework";

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
        TestCase("fileDisplayDownloads").FilesSwa(),
        TestCase("fileDisplayDownloads").FilesSwa().InGuestMode(),
        TestCase("fileDisplayDownloads").FilesSwa().TabletMode(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFolder")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayLaunchOnLocalFile").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFile")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOffline").Offline().FilesSwa(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayDriveOnline").FilesSwa(),
        TestCase("fileDisplayDriveOnlineNewWindow").DontObserveFileTasks(),
        TestCase("fileDisplayDriveOnlineNewWindow")
            .DontObserveFileTasks()
            .FilesSwa(),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartition").EnableSinglePartitionFormat(),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
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
    OpenVideoMediaApp, /* open_video_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").MediaSwa().InGuestMode(),
                      TestCase("videoOpenDownloads").MediaSwa(),
                      TestCase("videoOpenDrive").MediaSwa()));

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
                      TestCase("audioRepeatAllModeMultipleFileDrive"),
                      TestCase("audioNoRepeatModeSingleFileDrive"),
                      TestCase("audioRepeatOneModeSingleFileDrive"),
                      TestCase("audioRepeatAllModeSingleFileDrive"),
                      TestCase("audioNoRepeatModeMultipleFileDrive"),
                      TestCase("audioRepeatOneModeMultipleFileDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("imageOpenMediaAppDownloads").MediaSwa().InGuestMode(),
        TestCase("imageOpenMediaAppDownloads").MediaSwa(),
        TestCase("imageOpenMediaAppDrive").MediaSwa()));

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
        TestCase("zipFileOpenDownloads").InGuestMode(),
        TestCase("zipFileOpenDownloads"),
        TestCase("zipFileOpenDrive"),
        TestCase("zipFileOpenUsb"),
        TestCase("zipNotifyFileTasks"),
        TestCase("zipCreateFileDownloads").InGuestMode(),
        TestCase("zipCreateFileDownloads"),
        TestCase("zipCreateFileDownloads").InGuestMode().FilesSwa(),
        TestCase("zipCreateFileDownloads").FilesSwa(),
        TestCase("zipCreateFileDrive"),
        TestCase("zipCreateFileDrive").FilesSwa(),
        TestCase("zipCreateFileUsb"),
        TestCase("zipCreateFileUsb").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("selectCreateFolderDownloads"),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive")));

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
        TestCase("keyboardFocusOutlineVisible").EnableTrash(),
        TestCase("keyboardFocusOutlineVisibleMouse"),
        TestCase("keyboardFocusOutlineVisibleMouse").EnableTrash(),
#endif
        TestCase("keyboardSelectDriveDirectoryTree"),
        TestCase("keyboardDisableCopyWhenDialogDisplayed"),
        TestCase("keyboardOpenNewWindow"),
        TestCase("keyboardOpenNewWindow").InGuestMode(),
        TestCase("keyboardOpenNewWindow").FilesSwa(),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDrive"),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameNewFolderDownloads"),
        TestCase("renameRemovableWithKeyboardOnFileList")));

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
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkDeleteDisabledInRecents"),
        TestCase("checkGoToFileLocationEnabledInRecents"),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
                      TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
                      TestCase("toolbarDeleteButtonKeepFocus"),
                      TestCase("toolbarDeleteEntry").InGuestMode(),
                      TestCase("toolbarDeleteEntry").InGuestMode().FilesSwa(),
                      TestCase("toolbarDeleteEntry"),
                      TestCase("toolbarDeleteEntry").FilesSwa(),
                      TestCase("toolbarDeleteEntry").EnableTrash(),
                      TestCase("toolbarDeleteEntry").EnableTrash().FilesSwa(),
                      TestCase("toolbarRefreshButtonWithSelection")
                          .EnableGenericDocumentsProvider(),
                      TestCase("toolbarAltACommand"),
                      TestCase("toolbarRefreshButtonHiddenInRecents"),
                      TestCase("toolbarMultiMenuFollowsButton"),
                      TestCase("toolbarSharesheetButtonWithSelection"),
                      TestCase("toolbarSharesheetContextMenuWithSelection"),
                      TestCase("toolbarSharesheetNoEntrySelected")));

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
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdf").FilesSwa(),
        TestCase("openQuickViewPdfPopup"),
        TestCase("openQuickViewPdfPopup").FilesSwa(),
        TestCase("openQuickViewPdfPreviewsDisabled"),
        TestCase("openQuickViewPdfPreviewsDisabled").FilesSwa(),
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
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexAudio").FilesSwa(),
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
        TestCase("directoryTreeSelectedDirectory"),
        TestCase("directoryTreeRecentsSubtypeScroll"),
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
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode(),
#endif
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
        TestCase("dirContextMenuZip"),
        TestCase("dirContextMenuZipEject"),
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
        TestCase("drivePressClearSearch").FilesSwa(),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("driveBackupPhotos"),
        TestCase("driveBackupPhotos").EnableSinglePartitionFormat(),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkOpenFileThroughLinkedDirectory").MediaSwa(),
        TestCase("driveLinkOpenFileThroughTransitiveLink").MediaSwa(),
        TestCase("driveWelcomeBanner").EnableBannersFramework(),
        TestCase("driveWelcomeBanner").DisableBannersFramework(),
        TestCase("driveOfflineInfoBanner")
            .EnableDriveDssPin()
            .EnableBannersFramework(),
        TestCase("driveOfflineInfoBanner")
            .EnableDriveDssPin()
            .DisableBannersFramework(),
        TestCase("driveOfflineInfoBannerWithoutFlag").EnableBannersFramework(),
        TestCase("driveOfflineInfoBannerWithoutFlag").DisableBannersFramework(),
        TestCase("driveEnableDocsOfflineDialog"),
        TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBanner").EnableBannersFramework(),
        TestCase("holdingSpaceWelcomeBanner").DisableBannersFramework(),
        TestCase("holdingSpaceWelcomeBanner")
            .EnableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBanner")
            .DisableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerWontShowAfterBeingDismissed")
            .DisableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerWontShowForModalDialogs")
            .WithBrowser()
            .DisableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser()
            .EnableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser()
            .EnableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerWontShowForModalDialogs")
            .WithBrowser()
            .DisableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerWontShowOnDrive")
            .DisableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerWontShowOnDrive")
            .DisableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")
            .EnableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")
            .DisableBannersFramework(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")
            .EnableBannersFramework()
            .FilesSwa(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")
            .DisableBannersFramework()
            .FilesSwa()));

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
        TestCase("transferDragDropTreeItemAccepts"),
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
        TestCase("transferDeletedFile").FilesSwa().EnableTrash(),
        TestCase("transferInfoIsRemembered"),
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
        TestCase("transferShowPendingMessageForZeroRemainingTime").FilesSwa(),
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
        TestCase("tabindexFocus").DisableBannersFramework(),
        TestCase("tabindexFocus").EnableBannersFramework(),
        TestCase("tabindexFocusDownloads").DisableBannersFramework(),
        TestCase("tabindexFocusDownloads").EnableBannersFramework(),
        TestCase("tabindexFocusDownloads")
            .DisableBannersFramework()
            .InGuestMode(),
        TestCase("tabindexFocusDirectorySelected").DisableBannersFramework(),
        TestCase("tabindexFocusDirectorySelected").EnableBannersFramework(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .DisableBannersFramework(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .EnableBannersFramework(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .DisableBannersFramework()
            .InGuestMode(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .EnableBannersFramework()
            .InGuestMode()
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
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDownloads").WithBrowser().InIncognito().FilesSwa(),
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
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      TestCase("copyBetweenWindowsDriveToUsb"),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDownloads"),
                      TestCase("showGridViewDrive").EnableBannersFramework(),
                      TestCase("showGridViewDrive").DisableBannersFramework(),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewButtonSwitches").FilesSwa(),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewTitles"),
                      TestCase("showGridViewTitles").FilesSwa(),
                      TestCase("showGridViewMouseSelectionA11y"),
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
        TestCase("requestMount").DisableNativeSmb(),
        TestCase("requestMountMultipleMounts"),
        TestCase("requestMountMultipleMounts").DisableNativeSmb(),
        TestCase("requestMountSourceDevice"),
        TestCase("requestMountSourceDevice").DisableNativeSmb(),
        TestCase("requestMountSourceFile"),
        TestCase("requestMountSourceFile").DisableNativeSmb(),
        TestCase("providerEject"),
        TestCase("providerEject").DisableNativeSmb(),
        TestCase("deduplicatedUmaMetricForFileSystemProviders"),
        TestCase("deduplicatedUmaMetricForFileSystemProviders").FilesSwa()));

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
        TestCase("showAvailableStorageSmbfs"),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa()));

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
    ::testing::Values(
        TestCase("mountCrostini"),
        TestCase("enableDisableCrostini"),
        TestCase("sharePathWithCrostini").EnableBannersFramework(),
        TestCase("sharePathWithCrostini").DisableBannersFramework(),
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
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts")
            .DontMountVolumes()
            .FilesSwa(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("recentsDownloads"),
                      TestCase("recentsDrive"),
                      TestCase("recentsCrostiniNotMounted"),
                      TestCase("recentsCrostiniMounted"),
                      TestCase("recentsDownloadsAndDrive"),
                      TestCase("recentsDownloadsAndDriveWithOverlap"),
                      TestCase("recentsNested"),
                      TestCase("recentAudioDownloads"),
                      TestCase("recentAudioDownloadsAndDrive"),
                      TestCase("recentImagesDownloads"),
                      TestCase("recentImagesDownloadsAndDrive"),
                      TestCase("recentVideosDownloads"),
                      TestCase("recentVideosDownloadsAndDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .FilesSwa(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive"),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsClearSearchKeyDown"),
                      TestCase("searchDownloadsClearSearchKeyDown").FilesSwa(),
                      TestCase("searchDownloadsClearSearch"),
                      TestCase("searchDownloadsClearSearch").FilesSwa(),
                      TestCase("searchHidingViaTab"),
                      TestCase("searchHidingTextEntryField"),
                      TestCase("searchButtonToggles"),
                      TestCase("searchQueryLaunchParam")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum"),
                      TestCase("metricsRecordEnum").FilesSwa(),
                      TestCase("metricsOpenSwa").FilesSwa()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate"),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsDownloadsTranslation").FilesSwa(),
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
                      TestCase("breadcrumbsEliderMenuItemTabRight"),
                      TestCase("breadcrumbsDontExceedAvailableViewport")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
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
        // TODO(crbug.com/1228128): Remove disabled test when framework is live.
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .EnableBannersFramework(),
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .DisableBannersFramework()));

}  // namespace file_manager
