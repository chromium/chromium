// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/components/media_app_ui/buildflags.h"
#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/entry_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using platform_util::OpenOperationResult;
using web_app::SystemAppType;

namespace {

// Path to a subfolder in chrome/test/data that holds test files.
constexpr base::FilePath::CharType kTestFilesFolderInTestData[] =
    FILE_PATH_LITERAL("chromeos/file_manager");

// An 800x600 image/png (all blue pixels).
constexpr char kFilePng800x600[] = "image.png";

// A 640x480 image/jpeg (all green pixels).
constexpr char kFileJpeg640x480[] = "image3.jpg";

// A RAW file from an Olympus camera with the original preview/thumbnail data
// swapped out with "exif.jpg".
constexpr char kRaw378x272[] = "raw.orf";

// A 1-second long 648x486 VP9-encoded video with stereo Opus-encoded audio.
constexpr char kFileVideoVP9[] = "world.webm";

constexpr char kUnhandledRejectionScript[] =
    "window.dispatchEvent("
    "new CustomEvent('simulate-unhandled-rejection-for-test'));";

constexpr char kTypeErrorScript[] =
    "window.dispatchEvent("
    "new CustomEvent('simulate-type-error-for-test'));";

constexpr char kDomExceptionScript[] =
    "window.dispatchEvent("
    "new "
    "CustomEvent('simulate-unhandled-rejection-with-dom-exception-for-test'));";

class MediaAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  MediaAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMediaApp}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MediaAppIntegrationWithFilesAppTest : public MediaAppIntegrationTest {
  void SetUpOnMainThread() override {
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
    MediaAppIntegrationTest::SetUpOnMainThread();
  }
};

using MediaAppIntegrationAllProfilesTest = MediaAppIntegrationTest;
using MediaAppIntegrationWithFilesAppAllProfilesTest =
    MediaAppIntegrationWithFilesAppTest;

// Gets the base::FilePath for a named file in the test folder.
base::FilePath TestFile(const std::string& ascii_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.Append(kTestFilesFolderInTestData);
  path = path.AppendASCII(ascii_name);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(path));
  return path;
}

void PrepareAppForTest(content::WebContents* web_ui) {
  EXPECT_TRUE(WaitForLoadStop(web_ui));
  EXPECT_EQ(nullptr, MediaAppUiBrowserTest::EvalJsInAppFrame(
                         web_ui, MediaAppUiBrowserTest::AppJsTestLibrary()));
}

content::WebContents* PrepareActiveBrowserForTest() {
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(web_ui);
  return web_ui;
}

// Waits for a promise that resolves with image dimensions, once an <img>
// element appears in the light DOM with the provided `alt=` attribute.
content::EvalJsResult WaitForImageAlt(content::WebContents* web_ui,
                                      const std::string& alt) {
  constexpr char kScript[] = R"(
      (async () => {
        const img = await waitForNode('img[alt="$1"]');
        return `$${img.naturalWidth}x$${img.naturalHeight}`;
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(
      web_ui, base::ReplaceStringPlaceholders(kScript, {alt}, nullptr));
}

// Waits for the "shownav" attribute to show up in the MediaApp's current
// handler. Also checks the panel isn't open indicating an edit is not in
// progress. This prevents trying to traverse a directory before other files are
// available / while editing.
content::EvalJsResult WaitForNavigable(content::WebContents* web_ui) {
  constexpr char kScript[] = R"(
      (async () => {
        await waitForNode(':not([panelopen])[shownav]');
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kScript);
}

void TouchFileSync(const base::FilePath& path, const base::Time& time) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::TouchFile(path, time, time));
}

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaApp) {
  const GURL url(chromeos::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::MEDIA, url, "Gallery"));
}

// Test that the MediaApp successfully loads a file passed in on its launch
// params.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchWithFile) {
  WaitForTestSystemAppInstall();
  content::WebContents* app;
  {
    auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);

    // Add the 800x600 PNG image to launch params.
    params.launch_files.push_back(TestFile(kFilePng800x600));

    app = LaunchApp(std::move(params));
  }
  PrepareAppForTest(app);

  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));

  // Relaunch with a different file. This currently re-uses the existing window,
  // so we don't wait for page load here.
  {
    auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);
    params.launch_files = {TestFile(kFileJpeg640x480)};
    LaunchAppWithoutWaiting(std::move(params));
  }

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));
}

// Regression test for b/172881869.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, LoadsPdf) {
  WaitForTestSystemAppInstall();
  LaunchApp(web_app::SystemAppType::MEDIA);
  content::WebContents* app = PrepareActiveBrowserForTest();
  // TODO(crbug/1148090): To fully load PDFs, "frame-src" needs to be set, this
  // test doesn't provide coverage for that.
  // Note: If "object-src" is not set in the CSP, the `<embed>` element fails to
  // load and times out.
  constexpr char loadPdf[] = R"(
      (() => {
        const embedBlob =  document.createElement('embed');
        embedBlob.type ='application/pdf';
        embedBlob.height = '100%';
        embedBlob.width = '100%';
        const loadPromise = new Promise((resolve, reject) => {
          embedBlob.addEventListener('load', () => resolve(true));
          embedBlob.addEventListener('error', () => reject(false));
        });
        document.body.appendChild(embedBlob);
        embedBlob.src = 'blob:chrome-untrusted://media-app/fake-pdf-blob-hash';
        return loadPromise;
      })();
  )";

  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(app, loadPdf));
}

// This test tries to load files bundled in our CIPD package. The CIPD package
// is included in the `linux-chromeos-chrome` trybot but not in
// `linux-chromeos-rel` trybot. Only include this test when our CIPD package is
// present.
#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, LoadsInkForImageAnnotation) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);
  params.launch_files = {TestFile(kFileJpeg640x480)};
  content::WebContents* app = LaunchApp(std::move(params));
  PrepareAppForTest(app);

  // Ensure test image loaded.
  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));

  // TODO(b/175766054): Decipher if we can one line getting the annotate button
  // in `waitForNode`.
  // Note the button id (icon-button-3709949292) corresponds to the annotation
  // button and is calculated from a hash of the label ("Annotate"). This id is
  // used since cl/366443893 because the UI toolkit has loose guarantees about
  // where the actual label appears in the shadow DOM.
  constexpr char clickAnnotate[] = R"(
    (async () => {
      const appBar = await waitForNode('backlight-app-bar', ['backlight-app']);
      const annotateButton = appBar.shadowRoot.querySelector('#icon-button-3709949292');
      annotateButton.click();
      return true;
    })();
  )";
  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(app, clickAnnotate));

  // Checks ink is loaded for images by ensuring the ink engine canvas has a non
  // zero width and height attributes (checking <canvas.width/height is
  // insufficient since it has a default width of 300 and height of 150).
  // Note: The loading of ink engine elements can be async.
  constexpr char checkInkLoaded[] = R"(
    (async () => {
      const inkEngineCanvas = await waitForNode('canvas#ink-engine[width]', ['backlight-image-handler']);
      return !!inkEngineCanvas &&
        !!inkEngineCanvas.getAttribute('height') &&
        inkEngineCanvas.getAttribute('height') !== '0' &&
        !!inkEngineCanvas.getAttribute('width') &&
        inkEngineCanvas.getAttribute('width') !== '0';
    })();
  )";
  // TODO(b/175840855): Consider checking `inkEngineCanvas` size, it is
  // currently different to image size.
  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(app, checkInkLoaded));
}
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP)

// Test that the MediaApp can load RAW files passed on launch params.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, HandleRawFiles) {
  WaitForTestSystemAppInstall();

  content::WebContents* web_ui;
  {
    auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);

    // Add the handcrafted RAW file to launch params and launch.
    params.launch_files.push_back(TestFile(kRaw378x272));
    web_ui = LaunchApp(std::move(params));
    PrepareAppForTest(web_ui);
  }

  EXPECT_EQ("378x272", WaitForImageAlt(web_ui, kRaw378x272));

  // Loading a raw file will put the RAW loading module into the JS context.
  // Inject a script to manipulate the RAW loader into returning a result that
  // includes an Exif rotation.
  constexpr char kAdd270DegreeRotation[] = R"(
    (function() {
      const realPiexImage = PiexModule.image;
      PiexModule.image = (memory, length) => {
        const response = realPiexImage(memory, length);
        response.preview.orientation = 8;
        return response;
      };
    })();
  )";
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);
  EXPECT_EQ(true, ExecuteScript(app, kAdd270DegreeRotation));

  // Launch with a file that has a different name to ensure the rotated version
  // of the file is detected robustly.
  {
    auto clearFileParams = LaunchParamsForApp(web_app::SystemAppType::MEDIA);
    clearFileParams.launch_files = {TestFile(kFileJpeg640x480)};
    LaunchAppWithoutWaiting(std::move(clearFileParams));
  }
  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  {
    auto params = LaunchParamsForApp(web_app::SystemAppType::MEDIA);

    // Add the handcrafted RAW file to launch params and launch.
    params.launch_files.push_back(TestFile(kRaw378x272));
    LaunchAppWithoutWaiting(std::move(params));
  }
  // Width and height should be swapped now.
  EXPECT_EQ("272x378", WaitForImageAlt(web_ui, kRaw378x272));
}

// Ensures that chrome://media-app is available as a file task for the ChromeOS
// file manager and eligible for opening appropriate files / mime types.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAllProfilesTest,
                       MediaAppEligibleOpenTask) {
  constexpr bool kIsDirectory = false;
  const extensions::EntryInfo image_entry(TestFile(kFilePng800x600),
                                          "image/png", kIsDirectory);
  const extensions::EntryInfo video_entry(TestFile(kFileVideoVP9), "video/webm",
                                          kIsDirectory);

  WaitForTestSystemAppInstall();

  for (const auto& single_entry : {video_entry, image_entry}) {
    SCOPED_TRACE(single_entry.mime_type);
    std::vector<file_manager::file_tasks::FullTaskDescriptor> result;
    file_manager::file_tasks::FindWebTasks(profile(), {single_entry}, &result);

    ASSERT_LT(0u, result.size());
    EXPECT_EQ(1u, result.size());
    const auto& task = result[0];
    const auto& descriptor = task.task_descriptor();

    EXPECT_EQ("Gallery", task.task_title());
    EXPECT_EQ(extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
              task.task_verb());
    EXPECT_EQ(descriptor.app_id, *GetManager().GetAppIdForSystemApp(
                                     web_app::SystemAppType::MEDIA));
    EXPECT_EQ(chromeos::kChromeUIMediaAppURL, descriptor.action_id);
    EXPECT_EQ(file_manager::file_tasks::TASK_TYPE_WEB_APP,
              descriptor.task_type);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAllProfilesTest,
                       HiddenInLauncherAndSearch) {
  WaitForTestSystemAppInstall();

  // Check system_web_app_manager has the correct attributes for Media App.
  EXPECT_FALSE(
      GetManager().ShouldShowInLauncher(web_app::SystemAppType::MEDIA));
  EXPECT_FALSE(GetManager().ShouldShowInSearch(web_app::SystemAppType::MEDIA));
}

// Note: Error reporting tests are limited to one per test instance otherwise we
// run into "Too many calls to this API" error.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       TrustedContextReportsConsoleErrors) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* web_ui = LaunchApp(web_app::SystemAppType::MEDIA);

  // Pass multiple arguments to console.error() to also check they are parsed
  // and captured in the error message correctly.
  constexpr char kConsoleError[] =
      "console.error('YIKES', {data: 'something'}, new Error('deep error'));";
  EXPECT_EQ(true, ExecuteScript(web_ui, kConsoleError));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find(
                "error_message=Unexpected%3A%20%22YIKES%22%0A%7B%22data%22%"
                "3A%22something%22%7D%0AError%3A%20deep%20error"))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       TrustedContextReportsDomExceptions) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* web_ui = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true, ExecuteScript(web_ui, kDomExceptionScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find("error_message=Unhandled%20rejection%3A%20Error%"
                              "3A%20NotAFile%3A%20Not%20a%20file."))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       UntrustedContextReportsDomExceptions) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* app = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kDomExceptionScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find("error_message=Not%20a%20file."))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       TrustedContextReportsUnhandledExceptions) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* web_ui = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true, ExecuteScript(web_ui, kUnhandledRejectionScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find("error_message=Unhandled%20rejection%3A%20Error%"
                              "3A%20FakeErrorName%3A%20fake_throw"))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       UntrustedContextReportsUnhandledExceptions) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* app = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(
                      app, kUnhandledRejectionScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos, report.query.find("error_message=fake_throw"))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       TrustedContextReportsTypeErrors) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* web_ui = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true, ExecuteScript(web_ui, kTypeErrorScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find(
                "error_message=Error%3A%20ErrorEvent%3A%20Uncaught%20TypeError%"
                "3A%20event.notAFunction%20is%20not%20a%20function"))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       UntrustedContextReportsTypeErrors) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  WaitForTestSystemAppInstall();
  content::WebContents* app = LaunchApp(web_app::SystemAppType::MEDIA);

  EXPECT_EQ(true,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kTypeErrorScript));
  auto report = endpoint.WaitForReport();
  EXPECT_NE(std::string::npos,
            report.query.find("event.notAFunction%20is%20not%20a%20function"))
      << report.query;
  EXPECT_NE(std::string::npos, report.query.find("prod=ChromeOS_MediaApp"));
}

// End-to-end test to ensure that the MediaApp successfully registers as a file
// handler with the ChromeOS file manager on startup and acts as the default
// handler for a given file.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppAllProfilesTest,
                       FileOpenUsesMediaApp) {
  WaitForTestSystemAppInstall();
  Browser* test_browser = chrome::FindBrowserWithActiveWindow();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFilePng800x600)});
  OpenOperationResult open_result = folder.Open(TestFile(kFilePng800x600));

  // Window focus changes on ChromeOS are synchronous, so just get the newly
  // focused window.
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(web_ui);

  EXPECT_EQ(open_result, platform_util::OPEN_SUCCEEDED);

  // Check that chrome://media-app launched and the test file loads.
  EXPECT_NE(test_browser, app_browser);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser->app_name()),
            *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA));
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));
}

// Test that the MediaApp can navigate other files in the directory of a file
// that was opened, even if those files have changed since launch.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppAllProfilesTest,
                       FileOpenCanTraverseDirectory) {
  WaitForTestSystemAppInstall();

  // Initialize a folder with 2 files: 1 JPEG, 1 PNG. Note this approach doesn't
  // guarantee the modification times of the files so, and therefore does not
  // suggest an ordering to the files of the directory contents. But by having
  // at most two active files, we can still write a robust test.
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({
      TestFile(kFileJpeg640x480),
      TestFile(kFilePng800x600),
  });

  const base::FilePath copied_jpeg_640x480 = folder.files()[0];

  // Stamp the file with a time far in the past, so it can be "updated".
  // Note: Add a bit to the epoch to workaround https://crbug.com/1080434.
  TouchFileSync(copied_jpeg_640x480,
                base::Time::UnixEpoch() + base::TimeDelta::FromDays(1));

  // Sent an open request using only the 640x480 JPEG file.
  folder.Open(copied_jpeg_640x480);
  content::WebContents* web_ui = PrepareActiveBrowserForTest();

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // We load the first file when the app launches, other files in the working
  // directory are loaded afterwards. Wait for the second load to occur
  // indicated by being able to navigate.
  WaitForNavigable(web_ui);

  // Navigate to the next file in the directory.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));

  // Navigating again should wraparound.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // Navigate backwards.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(-1)"));
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));

  // Update the Jpeg, which invalidates open DOM File objects.
  TouchFileSync(copied_jpeg_640x480, base::Time::Now());

  // We should still be able to open the updated file.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));
  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // TODO(tapted): Test mixed file types. We used to test here with a file of a
  // different type in the list of files to open. Navigating would skip over it.
  // And opening it as the focus file would only open that file. Currently there
  // is no file type we register as a handler for that is not an image or video
  // file, and they all appear in the same "camera roll", so there is no way to
  // test mixed file types.
}

// Integration test for rename using the WritableFileSystem and Streams APIs.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppAllProfilesTest,
                       RenameFile) {
  WaitForTestSystemAppInstall();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFileJpeg640x480)});
  folder.Open(TestFile(kFileJpeg640x480));
  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  // Rename "image3.jpg" to "x.jpg".
  constexpr int kRenameResultSuccess = 0;
  constexpr char kScript[] =
      "lastLoadedReceivedFileList.item(0).renameOriginalFile('x.jpg')"
      ".then(result => domAutomationController.send(result));";
  int result = ~kRenameResultSuccess;
  EXPECT_EQ(true, content::ExecuteScriptAndExtractInt(app, kScript, &result));
  EXPECT_EQ(kRenameResultSuccess, result);

  folder.Refresh();

  EXPECT_EQ(1u, folder.files().size());
  EXPECT_EQ("x.jpg", folder.files()[0].BaseName().value());

  std::string expected_contents, renamed_contents;
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(
      base::ReadFileToString(TestFile(kFileJpeg640x480), &expected_contents));
  // Consistency check against the file size (2108 bytes) of image3.jpg in the
  // test data directory.
  EXPECT_EQ(2108u, expected_contents.size());
  EXPECT_TRUE(base::ReadFileToString(folder.files()[0], &renamed_contents));
  EXPECT_EQ(expected_contents, renamed_contents);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationAllProfilesTest);

// Note: All MediaAppIntegrationWithFilesAppTest cases above currently want
// coverage for all profile types, so the "less" prarameterized prefix is not
// instantiated to avoid a gtest warning.

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationWithFilesAppAllProfilesTest);
