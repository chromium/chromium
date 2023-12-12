// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/webui/media_app_ui/media_app_guest_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_path.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// File containing the test utility library, shared with integration tests.
constexpr base::FilePath::CharType kTestLibraryPath[] =
    FILE_PATH_LITERAL("ash/webui/system_apps/public/js/dom_testing_helpers.js");

// Test cases that run in the guest (untrusted) context.
constexpr char kGuestTestCases[] = "media_app_guest_ui_browsertest.js";

// Test cases that run in the host (trusted) context.
constexpr char kTestHarness[] = "media_app_ui_browsertest.js";

// Path to test files loaded via the TestFileRequestFilter.
constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/media_app_ui");

// Paths requested on the media-app origin that should be delivered by the test
// handler.
constexpr const char* kTestFiles[] = {
    kGuestTestCases,  kTestHarness, "guest_query_receiver.js",
    "test_worker.js", "driver.js",
};

}  // namespace

MediaAppUiBrowserTest::MediaAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(ash::kChromeUIMediaAppURL,
                                ash::kChromeUIMediaAppGuestURL,
                                {base::FilePath(kTestLibraryPath)},
                                kGuestTestCases,
                                kTestHarness) {
  ConfigureDefaultTestRequestHandler(
      base::FilePath(kTestFileLocation),
      {std::begin(kTestFiles), std::end(kTestFiles)});
}

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

// static
std::string MediaAppUiBrowserTest::AppJsTestLibrary() {
  return SandboxedWebUiAppTestBase::LoadJsTestLibrary(
      base::FilePath(kTestLibraryPath));
}

// static
void MediaAppUiBrowserTest::PrepareAppForTest(content::WebContents* web_ui) {
  EXPECT_TRUE(WaitForLoadStop(web_ui));
  EXPECT_EQ(nullptr, MediaAppUiBrowserTest::EvalJsInAppFrame(
                         web_ui, MediaAppUiBrowserTest::AppJsTestLibrary()));
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestCanLoad) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, HasTitleAndLang) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, LaunchFile) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, ReportsErrorsFromTrustedContext) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NonLaunchableIpcAfterFastLoad) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, ReLaunchableAfterFastLoad) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, MultipleFilesHaveTokens) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, SingleAudioLaunch) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, MultipleSelectionLaunch) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, LaunchUnopenableFile) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, LaunchUnnavigableDirectory) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NavigateWithUnopenableSibling) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, FileThatBecomesDirectory) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, CanOpenFeedbackDialog) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, CanFullscreenVideo) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, LoadVideoWithSubtitles) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, OverwriteOriginalIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, RejectZeroByteWrites) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, OverwriteOriginalPickerFallback) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, FilePickerValidateExtension) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, CrossContextErrors) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, DeleteOriginalIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, DeletionOpensNextFile) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, DeleteMissingFile) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, RenameMissingFile) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, OpenAllowedFileIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NavigateIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NavigateOutOfSync) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, RenameOriginalIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, RequestSaveFileIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GetExportFileIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, SaveAsIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, SaveAsErrorHandling) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, OpenFilesWithFilePickerIPC) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, RelatedFiles) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, SortedFilesByTime) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, SortedFilesByName) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GetFileNotCalledOnAllFiles) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestHasFocus) {
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NotifyCurrentFileLight) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NotifyCurrentFileDark) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, NotifyCurrentFileAppIconDark) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  RunCurrentTest();
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest,
                       BodyHasCorrectBackgroundColorInLightMode) {
  ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  RunCurrentTest();
}

// Test cases injected into the guest context.
// See implementations in media_app_guest_ui_browsertest.js.

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestCanSpawnWorkers) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestHasLang) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestLoadsLoadTimeData) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestCanLoadWithCspRestrictions) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestStartsWithDefaultFileList) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestFailsToFetchMissingFonts) {
  RunCurrentTest("runTestInGuest");
}

IN_PROC_BROWSER_TEST_F(MediaAppUiBrowserTest, GuestCanFilterInPlace) {
  RunCurrentTest("runTestInGuest");
}
