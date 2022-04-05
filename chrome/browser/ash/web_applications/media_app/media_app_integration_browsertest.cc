// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/web_applications/media_app/media_web_app_info.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/entry_info.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/color_palette.h"

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

// A RAW file from a Nikon camera.
constexpr char kRaw120x160[] = "raw.nef";

// A 1-second long 648x486 VP9-encoded video with stereo Opus-encoded audio.
constexpr char kFileVideoVP9[] = "world.webm";

// A 5-second long 96kb/s Ogg-Vorbis 44.1kHz mono audio file.
constexpr char kFileAudioOgg[] = "music.ogg";

// A 1-page (8.5" x 11") PDF with some text and metadata.
constexpr char kFilePdfTall[] = "tall.pdf";

// A small square image PDF created by a camera.
constexpr char kFilePdfImg[] = "img.pdf";

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
    feature_list_.InitAndEnableFeature(ash::features::kMediaAppHandlesPdf);
  }

  void MediaAppLaunchWithFile(bool audio_enabled);
  void MediaAppWithLaunchSystemWebAppAsync(bool audio_enabled);
  void MediaAppEligibleOpenTask(bool audio_enabled);

  // Helper to initiate a test by launching a single file.
  content::WebContents* LaunchWithOneTestFile(const char* file);

  // Helper to initiate a test by launching with no files (zero state).
  content::WebContents* LaunchWithNoFiles();

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<file_manager::test::FolderInMyFiles> launch_folder_;
};

class MediaAppIntegrationWithFilesAppTest : public MediaAppIntegrationTest {
  void SetUpOnMainThread() override {
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
    MediaAppIntegrationTest::SetUpOnMainThread();
  }
};

class MediaAppIntegrationAudioEnabledTest : public MediaAppIntegrationTest {
 public:
  MediaAppIntegrationAudioEnabledTest() {
    feature_list_.InitAndEnableFeature(ash::features::kMediaAppHandlesAudio);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MediaAppIntegrationAudioDisabledTest : public MediaAppIntegrationTest {
 public:
  MediaAppIntegrationAudioDisabledTest() {
    feature_list_.InitAndDisableFeature(ash::features::kMediaAppHandlesAudio);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MediaAppIntegrationDarkLightModeEnabledTest
    : public MediaAppIntegrationTest {
 public:
  MediaAppIntegrationDarkLightModeEnabledTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kDarkLightMode);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MediaAppIntegrationDarkLightModeDisabledTest
    : public MediaAppIntegrationTest {
 public:
  MediaAppIntegrationDarkLightModeDisabledTest() {
    feature_list_.InitAndDisableFeature(chromeos::features::kDarkLightMode);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MediaAppIntegrationPdfDisabledTest : public MediaAppIntegrationTest {
 public:
  MediaAppIntegrationPdfDisabledTest() {
    // This reverts the "default"-enabled state of the feature set in the
    // base class test harness.
    feature_list_.InitAndDisableFeature(ash::features::kMediaAppHandlesPdf);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

using MediaAppIntegrationAllProfilesTest = MediaAppIntegrationTest;
using MediaAppIntegrationWithFilesAppAllProfilesTest =
    MediaAppIntegrationWithFilesAppTest;

class BrowserWindowWaiter : public BrowserListObserver {
 public:
  void WaitForBrowserAdded() {
    BrowserList::GetInstance()->AddObserver(this);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    BrowserList::GetInstance()->RemoveObserver(this);
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override { quit_closure_.Run(); }

 private:
  base::RepeatingClosure quit_closure_;
};

// Waits for the number of active Browsers in the test process to reach `count`.
void WaitForBrowserCount(size_t count) {
  EXPECT_LE(BrowserList::GetInstance()->size(), count) << "Too many browsers";
  while (BrowserList::GetInstance()->size() < count) {
    BrowserWindowWaiter().WaitForBrowserAdded();
  }
}

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

std::string FindAnyTTF() {
  const base::FilePath root_path(FILE_PATH_LITERAL("/usr/share/fonts"));
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator enumerator(
      root_path, true, base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.ttf"),
      base::FileEnumerator::FolderSearchPolicy::ALL,
      base::FileEnumerator::ErrorPolicy::IGNORE_ERRORS);
  const base::FilePath candidate = enumerator.Next();
  std::vector<std::string> components = candidate.GetComponents();
  if (components.size() < 5) {
    return {};
  }
  std::vector<std::string> slice(components.begin() + 4, components.end());
  return base::JoinString(slice, "/");
}

void PrepareAppForTest(content::WebContents* web_ui) {
  EXPECT_TRUE(WaitForLoadStop(web_ui));
  EXPECT_EQ(nullptr, MediaAppUiBrowserTest::EvalJsInAppFrame(
                         web_ui, MediaAppUiBrowserTest::AppJsTestLibrary()));
}

content::WebContents* PrepareActiveBrowserForTest(
    int expected_browser_count = 2) {
  WaitForBrowserCount(expected_browser_count);
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(web_ui);
  return web_ui;
}

// Waits for a promise that resolves with the audio track title, once a <div>
// element with title track information appears in the light DOM.
content::EvalJsResult WaitForAudioTrackTitle(content::WebContents* web_ui) {
  constexpr char kScript[] = R"(
      (async function waitForAudioTrackTitle() {
        return (await waitForNode('div.title:not(:empty)')).innerText;
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kScript);
}

// Waits for a promise that resolves with image dimensions, once an <img>
// element appears in the light DOM with the provided `alt=` attribute.
content::EvalJsResult WaitForImageAlt(content::WebContents* web_ui,
                                      const std::string& alt) {
  constexpr char kScript[] = R"(
      (async function waitForImageAlt() {
        const img = await waitForNode('img[alt="$1"]');
        return `$${img.naturalWidth}x$${img.naturalHeight}`;
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(
      web_ui, base::ReplaceStringPlaceholders(kScript, {alt}, nullptr));
}

// Runs the provided `script` in a non-isolated JS world that can access
// variables defined in global scope (otherwise only DOM queries are allowed).
// The script must call `domAutomationController.send(result)` to return.
std::string ExtractStringInGlobalScope(content::WebContents* web_ui,
                                       const std::string& script) {
  std::string result;
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(app, script, &result));
  return result;
}

// Waits for the "filetraversalenabled" attribute to show up in the MediaApp's
// current handler. Also checks the panel isn't open indicating an edit is not
// in progress. This prevents trying to traverse a directory before other files
// are available / while editing.
content::EvalJsResult WaitForNavigable(content::WebContents* web_ui) {
  constexpr char kScript[] = R"(
      (async function waitForNavigable() {
        await waitForNode(':not([panelopen])[filetraversalenabled]');
      })();
  )";

  return MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kScript);
}

void TouchFileSync(const base::FilePath& path, const base::Time& time) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::TouchFile(path, time, time));
}

content::WebContents* MediaAppIntegrationTest::LaunchWithOneTestFile(
    const char* file) {
  WaitForTestSystemAppInstall();
  launch_folder_ =
      std::make_unique<file_manager::test::FolderInMyFiles>(profile());
  launch_folder_->Add({TestFile(file)});
  EXPECT_EQ(launch_folder_->Open(TestFile(file)),
            platform_util::OPEN_SUCCEEDED);
  return PrepareActiveBrowserForTest();
}

content::WebContents* MediaAppIntegrationTest::LaunchWithNoFiles() {
  WaitForTestSystemAppInstall();
  content::WebContents* web_ui = LaunchApp(web_app::SystemAppType::MEDIA);
  PrepareAppForTest(web_ui);
  return web_ui;
}

std::vector<apps::IntentLaunchInfo> GetAppsForMimeType(
    apps::AppServiceProxy* proxy,
    const std::string& mime_type) {
  std::vector<apps::mojom::IntentFilePtr> intent_files;
  auto file = apps::mojom::IntentFile::New();
  file->url = GURL("filesystem://path/to/file.bin");
  file->mime_type = mime_type;
  file->is_directory = apps::mojom::OptionalBool::kFalse;
  intent_files.push_back(std::move(file));
  return proxy->GetAppsForFiles(std::move(intent_files));
}

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaApp) {
  const GURL url(ash::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(web_app::SystemAppType::MEDIA, url, "Gallery"));
}

// Test that the MediaApp successfully loads a file passed in on its launch
// params. This exercises only web_applications logic.
void MediaAppIntegrationTest::MediaAppLaunchWithFile(bool audio_enabled) {
  WaitForTestSystemAppInstall();
  // Launch the App for the first time.
  content::WebContents* app = LaunchAppWithFile(web_app::SystemAppType::MEDIA,
                                                TestFile(kFilePng800x600));
  Browser* first_browser = chrome::FindBrowserWithActiveWindow();
  PrepareAppForTest(app);

  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));

  // Launch with a different file.
  if (audio_enabled) {
    // Open file in new window.
    app = LaunchAppWithFile(web_app::SystemAppType::MEDIA,
                            TestFile(kFileJpeg640x480));
  } else {
    // Open file in same window.
    LaunchAppWithFileWithoutWaiting(web_app::SystemAppType::MEDIA,
                                    TestFile(kFileJpeg640x480));
  }
  Browser* second_browser = chrome::FindBrowserWithActiveWindow();
  PrepareAppForTest(app);

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));
  if (audio_enabled) {
    EXPECT_NE(first_browser, second_browser);
  } else {
    EXPECT_EQ(first_browser, second_browser);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest,
                       MediaAppLaunchWithFile) {
  MediaAppLaunchWithFile(true);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioDisabledTest,
                       MediaAppLaunchWithFile) {
  MediaAppLaunchWithFile(false);
}

// Test that the MediaApp successfully loads a file using
// LaunchSystemWebAppAsync. This exercises high level integration with SWA
// platform (a different code path than MediaAppLaunchWithFile test).
void MediaAppIntegrationTest::MediaAppWithLaunchSystemWebAppAsync(
    bool audio_enabled) {
  WaitForTestSystemAppInstall();
  // Launch the App for the first time.
  web_app::SystemAppLaunchParams audio_params;
  audio_params.launch_paths.push_back(TestFile(kFilePng800x600));
  web_app::LaunchSystemWebAppAsync(browser()->profile(),
                                   web_app::SystemAppType::MEDIA, audio_params);
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());
  Browser* first_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* app = PrepareActiveBrowserForTest();

  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));

  // Launch the App for the second time.
  web_app::SystemAppLaunchParams image_params;
  image_params.launch_paths.push_back(TestFile(kFileJpeg640x480));
  web_app::LaunchSystemWebAppAsync(browser()->profile(),
                                   web_app::SystemAppType::MEDIA, image_params);
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());
  app = PrepareActiveBrowserForTest(audio_enabled ? 3 : 2);
  Browser* second_browser = chrome::FindBrowserWithActiveWindow();

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));
  if (audio_enabled) {
    EXPECT_NE(first_browser, second_browser);
  } else {
    EXPECT_EQ(first_browser, second_browser);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest,
                       MediaAppWithLaunchSystemWebAppAsync) {
  MediaAppWithLaunchSystemWebAppAsync(true);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioDisabledTest,
                       MediaAppWithLaunchSystemWebAppAsync) {
  MediaAppWithLaunchSystemWebAppAsync(false);
}

// Test that the Media App launches a single window for images.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchImageMulti) {
  WaitForTestSystemAppInstall();
  web_app::SystemAppLaunchParams image_params;
  image_params.launch_paths = {TestFile(kFilePng800x600),
                               TestFile(kFileJpeg640x480)};

  web_app::LaunchSystemWebAppAsync(browser()->profile(),
                                   web_app::SystemAppType::MEDIA, image_params);
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());

  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, browser_list->size());  // 1 extra for the browser test browser.

  content::TitleWatcher watcher(
      browser_list->get(1)->tab_strip_model()->GetActiveWebContents(),
      u"image.png");
  EXPECT_EQ(u"image.png", watcher.WaitAndGetTitle());
}

// Test that the Media App launches multiple windows for PDFs.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchPdfMulti) {
  WaitForTestSystemAppInstall();
  web_app::SystemAppLaunchParams pdf_params;
  pdf_params.launch_paths = {TestFile(kFilePdfTall), TestFile(kFilePdfImg)};

  web_app::LaunchSystemWebAppAsync(browser()->profile(),
                                   web_app::SystemAppType::MEDIA, pdf_params);
  web_app::FlushSystemWebAppLaunchesForTesting(browser()->profile());

  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(3u, browser_list->size());  // 1 extra for the browser test browser.

  content::TitleWatcher watcher1(
      browser_list->get(1)->tab_strip_model()->GetActiveWebContents(),
      u"tall.pdf");
  content::TitleWatcher watcher2(
      browser_list->get(2)->tab_strip_model()->GetActiveWebContents(),
      u"img.pdf");
  EXPECT_EQ(u"tall.pdf", watcher1.WaitAndGetTitle());
  EXPECT_EQ(u"img.pdf", watcher2.WaitAndGetTitle());
}

// Test that the Media App appears as a handler for files in the App Service.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppHandlesIntents) {
  WaitForTestSystemAppInstall();
  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  const std::string media_app_id =
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA);

  {
    // Smoke test that a binary blob is not handled by Media App.
    std::vector<apps::IntentLaunchInfo> intent_launch_info =
        GetAppsForMimeType(proxy, "application/octet-stream");

    // Media App should not be in the returned list of handlers.
    EXPECT_FALSE(base::ranges::any_of(
        intent_launch_info,
        [&media_app_id](const apps::IntentLaunchInfo& info) {
          return info.app_id == media_app_id;
        }));
  }

  auto media_app_info = CreateWebAppInfoForMediaWebApp();

  // Ensure that Media App is returned as a handler for every mime type listed
  // in its file handlers.
  for (const auto& file_handler : media_app_info->file_handlers) {
    for (const auto& accept : file_handler.accept) {
      std::vector<apps::IntentLaunchInfo> intent_launch_info =
          GetAppsForMimeType(proxy, accept.mime_type);

      // Media App should be in the returned list of handlers.
      EXPECT_FALSE(intent_launch_info.empty()) << " at " << accept.mime_type;
      EXPECT_TRUE(base::ranges::any_of(
          intent_launch_info,
          [&media_app_id](const apps::IntentLaunchInfo& info) {
            return info.app_id == media_app_id;
          }))
          << " at " << accept.mime_type;
    }
  }
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
  constexpr char kLoadPdf[] = R"(
      (function loadPdf() {
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

  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(app, kLoadPdf));
}

namespace {
// icon-button ids are calculated from a hash of the button labels. Id is used
// because the UI toolkit has loose guarantees about where the actual label
// appears in the shadow DOM.
constexpr char kInfoButtonSelector[] = "#icon-button-2283726";
constexpr char kAnnotationButtonSelector[] = "#icon-button-3709949292";
constexpr char kCropAndRotateButtonSelector[] = "#icon-button-2723030533";

// Clicks the button on the app bar with the specified selector.
void clickAppBarButton(content::WebContents* app, const std::string& selector) {
  constexpr char kClickButton[] = R"(
      (async function clickAppBarButton() {
        const button =
            await getNode('$1', ['backlight-app-bar', 'backlight-app']);
        button.click();
      })();
  )";
  MediaAppUiBrowserTest::EvalJsInAppFrame(
      app, base::ReplaceStringPlaceholders(kClickButton, {selector}, nullptr));
}

// Returns true if the button on the app bar with the specified selector has the
// 'on' attribute (indicating it's styled as active).
bool isAppBarButtonOn(content::WebContents* app, const std::string& selector) {
  constexpr char kIsButtonOn[] = R"(
    (async function isAppBarButtonOn() {
      const button =
          await getNode('$1', ['backlight-app-bar', 'backlight-app']);
      return button.hasAttribute('on');
    })();
  )";
  return MediaAppUiBrowserTest::EvalJsInAppFrame(
             app,
             base::ReplaceStringPlaceholders(kIsButtonOn, {selector}, nullptr))
      .ExtractBool();
}
}  // namespace

// These tests try to load files bundled in our CIPD package. The CIPD package
// is included in the `linux-chromeos-chrome` trybot but not in
// `linux-chromeos-rel` trybot. Only include these when our CIPD package is
// present. We disable the tests rather than comment them out entirely so that
// they are still subject to compilation on open-source builds.
#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
#define MAYBE_LoadsInkForImageAnnotation LoadsInkForImageAnnotation
#define MAYBE_InformationPanel InformationPanel
#define MAYBE_SavesToOriginalFile SavesToOriginalFile
#define MAYBE_OpenPdfInViewerPopup OpenPdfInViewerPopup
#else
#define MAYBE_LoadsInkForImageAnnotation DISABLED_LoadsInkForImageAnnotation
#define MAYBE_InformationPanel DISABLED_InformationPanel
#define MAYBE_SavesToOriginalFile DISABLED_SavesToOriginalFile
#define MAYBE_OpenPdfInViewerPopup DISABLED_OpenPdfInViewerPopup
#endif

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       MAYBE_LoadsInkForImageAnnotation) {
  WaitForTestSystemAppInstall();
  content::WebContents* app = LaunchAppWithFile(web_app::SystemAppType::MEDIA,
                                                TestFile(kFileJpeg640x480));
  PrepareAppForTest(app);

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));

  clickAppBarButton(app, kAnnotationButtonSelector);

  // Checks ink is loaded for images by ensuring the ink engine canvas has a non
  // zero width and height attributes (checking <canvas.width/height is
  // insufficient since it has a default width of 300 and height of 150).
  // Note: The loading of ink engine elements can be async.
  constexpr char kCheckInkLoaded[] = R"(
    (async function checkInkLoaded() {
      const inkEngineCanvas = await waitForNode(
          'canvas.ink-engine[width]', ['backlight-image-handler']);
      return !!inkEngineCanvas &&
        !!inkEngineCanvas.getAttribute('height') &&
        inkEngineCanvas.getAttribute('height') !== '0' &&
        !!inkEngineCanvas.getAttribute('width') &&
        inkEngineCanvas.getAttribute('width') !== '0';
    })();
  )";
  // TODO(b/175840855): Consider checking `inkEngineCanvas` size, it is
  // currently different to image size.
  EXPECT_EQ(true,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kCheckInkLoaded));
}

// Tests that clicking on the 'Info' button in the app bar toggles the
// information panel.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MAYBE_InformationPanel) {
  WaitForTestSystemAppInstall();
  content::WebContents* app = LaunchAppWithFile(web_app::SystemAppType::MEDIA,
                                                TestFile(kFileJpeg640x480));
  PrepareAppForTest(app);
  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));

  // Expect info panel to not be open on first load.
  constexpr char kHasInfoPanelOpen[] = R"(
    (async function hasInfoPanelOpen() {
      const metadataPanel = await getNode(
          'backlight-metadata-panel', ['backlight-image-handler']);
      return !!metadataPanel;
    })();
  )";
  EXPECT_EQ(false,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kHasInfoPanelOpen));
  EXPECT_EQ(false, isAppBarButtonOn(app, kInfoButtonSelector));

  // Expect info panel to be open after clicking info button.
  clickAppBarButton(app, kInfoButtonSelector);
  EXPECT_EQ(true,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kHasInfoPanelOpen));
  EXPECT_EQ(true, isAppBarButtonOn(app, kInfoButtonSelector));

  // Expect info panel to be closed after clicking info button again.
  // After closing we must wait for the DOM update because the panel doesn't
  // disappear from the DOM until the close animation is complete.
  clickAppBarButton(app, kInfoButtonSelector);
  constexpr char kWaitForImageHandlerUpdate[] = R"(
    (async function waitForImageHandlerUpdate() {
      const imageHandler = await getNode('backlight-image-handler');
      await childListUpdate(imageHandler.shadowRoot);
    })();
  )";
  MediaAppUiBrowserTest::EvalJsInAppFrame(app, kWaitForImageHandlerUpdate);
  EXPECT_EQ(false,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kHasInfoPanelOpen));
  EXPECT_EQ(false, isAppBarButtonOn(app, kInfoButtonSelector));
}

// Tests that the media app is able to overwrite the original file on save.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       MAYBE_SavesToOriginalFile) {
  WaitForTestSystemAppInstall();
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFilePng800x600)});

  const auto kTestFile = folder.files()[0];
  // Stamp the file with a time far in the past, so it can be "updated".
  // Note: Add a bit to the epoch to workaround https://crbug.com/1080434.
  TouchFileSync(kTestFile, base::Time::UnixEpoch() + base::Days(1));

  folder.Open(kTestFile);
  content::WebContents* app = PrepareActiveBrowserForTest();
  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));

  constexpr char kHasSaveDiscardButtons[] = R"(
    (async function hasSaveDiscardButtons() {
      const discardButton = await getNode('ea-button[label="Discard edits"]',
          ['backlight-app-bar', 'backlight-app']);
      const saveButton = await getNode('backlight-split-button[label="Save"]',
          ['backlight-app-bar', 'backlight-app']);
      return !!discardButton && !!saveButton;
    })();
  )";
  // The save/discard buttons should not show when the file is unchanged.
  EXPECT_EQ(false, MediaAppUiBrowserTest::EvalJsInAppFrame(
                       app, kHasSaveDiscardButtons));

  // Make a change to the file (rotate image), then check the save/discard
  // buttons now exist.
  clickAppBarButton(app, kCropAndRotateButtonSelector);
  constexpr char kRotateImage[] = R"(
    (async function rotateImage() {
      await waitForNode('backlight-crop-panel', ['backlight-image-handler']);
      const rotateAntiClockwiseButton = await getNode('#icon-button-427243323',
          ['backlight-crop-panel', 'backlight-image-handler']);
      rotateAntiClockwiseButton.click();
      const doneButton = await waitForNode(
          'ea-button[label="Done"]', ['backlight-app-bar', 'backlight-app']);
      doneButton.click();
      await waitForNode('backlight-split-button[label="Save"]',
          ['backlight-app-bar', 'backlight-app']);
    })();
  )";
  MediaAppUiBrowserTest::EvalJsInAppFrame(app, kRotateImage);
  EXPECT_EQ(true, MediaAppUiBrowserTest::EvalJsInAppFrame(
                      app, kHasSaveDiscardButtons));

  // Save the changes, then wait for the save to go through.
  constexpr char kClickSaveButton[] = R"(
    (async function clickSaveButton() {
      const saveButton = await getNode('ea-button[label="Save"]',
          ['backlight-split-button[label="Save"]', 'backlight-app-bar',
          'backlight-app']);
      saveButton.click();
      window['savedToastPromise'] = new Promise(resolve => {
        document.addEventListener('show-toast', (event) => {
          if (event.detail.message === 'Saved') {
            resolve(true);
          }
        });
      });
      saveButton.click();
    })();
  )";
  MediaAppUiBrowserTest::EvalJsInAppFrame(app, kClickSaveButton);

  constexpr char kWaitForSaveToast[] = R"(
    (async function waitForSaveToast() {
      const savedToast = await window['savedToastPromise'];
      return !!savedToast;
    })();
  )";
  EXPECT_EQ(true,
            MediaAppUiBrowserTest::EvalJsInAppFrame(app, kWaitForSaveToast));
  MediaAppUiBrowserTest::EvalJsInAppFrame(app, kWaitForSaveToast);

  // Verify the contents of the file is different to the original.
  std::string original_contents, rotated_contents;
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(
      base::ReadFileToString(TestFile(kFilePng800x600), &original_contents));
  EXPECT_TRUE(base::ReadFileToString(kTestFile, &rotated_contents));
  EXPECT_NE(original_contents, rotated_contents);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MAYBE_OpenPdfInViewerPopup) {
  // A small test PDF.
  constexpr char kOpenPdfInViewer[] = R"(
    const pdf = `%PDF-1.0
1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj 2 0 ` +
`obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj 3 0 ` +
`obj<</Type/Page/MediaBox[0 0 3 3]>>endobj
xref
0 4
0000000000 65535 f
0000000010 00000 n
0000000053 00000 n
0000000102 00000 n
trailer<</Size 4/Root 1 0 R>>
startxref
149
%EOF`;
    const pdfBlob = new Blob([pdf], {type: 'application/pdf'});
    const blobUrl = URL.createObjectURL(pdfBlob);
    const blobUuid =
        (new URL(blobUrl.substring(5))).pathname.substring(1);
    document.querySelector('backlight-app').delegate.openInSandboxedViewer(
        'PDF Accessibility Mode - TestPdfTitle.pdf', blobUuid);
  )";

  content::WebContents* web_ui = LaunchWithNoFiles();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  WaitForBrowserCount(2);
  EXPECT_EQ(true, ExecuteScript(app, kOpenPdfInViewer));

  WaitForBrowserCount(3);
  Browser* popup_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* popup_ui =
      popup_browser->tab_strip_model()->GetActiveWebContents();

  content::TitleWatcher watcher(popup_ui,
                                u"PDF Accessibility Mode - TestPdfTitle.pdf");

  EXPECT_EQ(u"PDF Accessibility Mode - TestPdfTitle.pdf",
            watcher.WaitAndGetTitle());
  EXPECT_EQ(u"PDF Accessibility Mode - TestPdfTitle.pdf", popup_ui->GetTitle());

  const char16_t kExpectedWindowTitle[] =
      u"Gallery - PDF Accessibility Mode - TestPdfTitle.pdf";
  aura::Window* popup_window = popup_ui->GetTopLevelNativeWindow();

  // The NativeWindow title change may happen asynchronously.
  if (popup_window->GetTitle() != kExpectedWindowTitle) {
    struct NativeWindowTitleWatcher : public aura::WindowObserver {
      base::RunLoop run_loop;
      void OnWindowTitleChanged(aura::Window* window) override {
        run_loop.Quit();
      }
    } wait_for_title_change;
    popup_window->AddObserver(&wait_for_title_change);
    wait_for_title_change.run_loop.Run();
    popup_window->RemoveObserver(&wait_for_title_change);
  }

  EXPECT_EQ(kExpectedWindowTitle, popup_window->GetTitle());

  EXPECT_TRUE(content::WaitForLoadStop(popup_ui));
  content::RenderFrameHost* untrusted_ui = ChildFrameAt(popup_ui, 0);
  content::RenderFrameHost* embed_ui = ChildFrameAt(untrusted_ui, 0);
  content::RenderFrameHost* pdf_ui = ChildFrameAt(embed_ui, 0);

  // Spot-check that the <embed> element hosting <pdf-viewer> (which is nested
  // inside "our" <embed> element) exists. Figuring out more about this element
  // is hard - the normal ways we inject test code results in "Failure to
  // communicate with DOMMessageQueue", and would result in bad coupling to the
  // PDF viewer UI.
  EXPECT_TRUE(pdf_ui) << "Nested PDF <embed> element not found";
}

// Test that the MediaApp can load RAW files passed on launch params.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest, HandleRawFiles) {
  WaitForTestSystemAppInstall();

  // Initialize a folder with 2 RAW images. Note this approach doesn't guarantee
  // the modification times of the files so, and therefore does not suggest an
  // ordering to the files of the directory contents. But by having at most two
  // active files, we can still write a robust test. We load two RAW images so
  // that the piexif module can load and we can perform an injection to check
  // that EXIF is respected.
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kRaw120x160), TestFile(kRaw378x272)});
  folder.Open(TestFile(kRaw120x160));

  // Window focus changes on ChromeOS are synchronous, so just get the newly
  // focused window.
  content::WebContents* web_ui = PrepareActiveBrowserForTest();

  EXPECT_EQ("120x160", WaitForImageAlt(web_ui, kRaw120x160));

  // We load the first file when the app launches, other files in the working
  // directory are loaded afterwards. Wait for the second load to occur
  // indicated by being able to navigate.
  WaitForNavigable(web_ui);

  // Loading a raw file will put the RAW loading module into the JS context.
  // Inject a script to manipulate the RAW loader into returning a result that
  // includes an Exif rotation.
  constexpr char kAdd270DegreeRotation[] = R"(
    (function add270DegreeRotation() {
      const realPiexImage = getPiexModuleForTesting().image;
      getPiexModuleForTesting().image = (memory, length) => {
        const response = realPiexImage(memory, length);
        response.preview.orientation = 8;
        return response;
      };
    })();
  )";
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);
  EXPECT_EQ(true, ExecuteScript(app, kAdd270DegreeRotation));

  // Navigate to the next file in the directory.
  EXPECT_EQ(true, ExecuteScript(web_ui, "advance(1)"));

  // Width and height should be swapped now.
  EXPECT_EQ("272x378", WaitForImageAlt(web_ui, kRaw378x272));
}

// Ensures that chrome://media-app is available as a file task for the ChromeOS
// file manager and eligible for opening appropriate files / mime types.
void MediaAppIntegrationTest::MediaAppEligibleOpenTask(bool audio_enabled) {
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(TestFile(kFilePng800x600));
  file_paths.push_back(TestFile(kFileVideoVP9));
  if (audio_enabled) {
    file_paths.push_back(TestFile(kFileAudioOgg));
  }

  WaitForTestSystemAppInstall();

  for (const auto& file_path : file_paths) {
    std::vector<file_manager::file_tasks::FullTaskDescriptor> result =
        file_manager::test::GetTasksForFile(profile(), file_path);

    // Files SWA internal task "select" matches any file, we ignore it here.
    base::EraseIf(result, [](auto task) {
      return task.task_descriptor.app_id == file_manager::kFileManagerSwaAppId;
    });

    ASSERT_LT(0u, result.size());
    EXPECT_EQ(1u, result.size());
    const auto& task = result[0];
    const auto& descriptor = task.task_descriptor;

    EXPECT_EQ("Gallery", task.task_title);
    EXPECT_EQ(extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
              task.task_verb);
    EXPECT_EQ(descriptor.app_id, *GetManager().GetAppIdForSystemApp(
                                     web_app::SystemAppType::MEDIA));
    EXPECT_EQ(ash::kChromeUIMediaAppURL, descriptor.action_id);
    EXPECT_EQ(file_manager::file_tasks::TASK_TYPE_WEB_APP,
              descriptor.task_type);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest,
                       MediaAppEligibleOpenTask) {
  MediaAppEligibleOpenTask(true);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioDisabledTest,
                       MediaAppEligibleOpenTask) {
  MediaAppEligibleOpenTask(false);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAllProfilesTest,
                       ShownInLauncherAndSearch) {
  WaitForTestSystemAppInstall();

  // Check system_web_app_manager has the correct attributes for Media App.
  auto* system_app = GetManager().GetSystemApp(web_app::SystemAppType::MEDIA);
  EXPECT_TRUE(system_app->ShouldShowInLauncher());
  EXPECT_TRUE(system_app->ShouldShowInSearch());
}

// Test for the old behaviour with fewer permutations. Can be removed along with
// features::MediaAppHandlesPdf.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPdfDisabledTest,
                       HiddenInLauncherAndSearch) {
  WaitForTestSystemAppInstall();

  // Check system_web_app_manager has the correct attributes for Media App.
  auto* system_app = GetManager().GetSystemApp(web_app::SystemAppType::MEDIA);
  EXPECT_FALSE(system_app->ShouldShowInLauncher());
  EXPECT_FALSE(system_app->ShouldShowInSearch());
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
            report.query.find("error_message=Unhandled%20rejection%3A"
                              "%20%5BNotAFile%5D%20Not%20a%20file."))
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
            report.query.find("error_message=Unhandled%20rejection%3A%20%5B"
                              "FakeErrorName%5D%20fake_throw"))
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
                "error_message=ErrorEvent%3A%20%5B%5D%20Uncaught%20TypeError%"
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
  base::HistogramTester histograms;

  WaitForTestSystemAppInstall();
  Browser* test_browser = chrome::FindBrowserWithActiveWindow();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFilePng800x600)});
  OpenOperationResult open_result = folder.Open(TestFile(kFilePng800x600));

  // Although window focus changes on ChromeOS are synchronous, the app launch
  // codepaths may not be, so ensure a Browser is created.
  WaitForBrowserCount(2);
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

  // Check the metric is recorded.
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromFileManager", 1);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationDarkLightModeEnabledTest,
                       HasCorrectThemeAndBackgroundColor) {
  WaitForTestSystemAppInstall();
  web_app::AppId app_id =
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA);

  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile())->registrar();

  EXPECT_EQ(registrar.GetAppThemeColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppBackgroundColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppDarkModeThemeColor(app_id), gfx::kGoogleGrey900);
  EXPECT_EQ(registrar.GetAppDarkModeBackgroundColor(app_id),
            gfx::kGoogleGrey900);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationDarkLightModeDisabledTest,
                       HasCorrectThemeAndBackgroundColor) {
  WaitForTestSystemAppInstall();
  web_app::AppId app_id =
      *GetManager().GetAppIdForSystemApp(web_app::SystemAppType::MEDIA);

  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile())->registrar();

  EXPECT_EQ(registrar.GetAppThemeColor(app_id), gfx::kGoogleGrey900);
  EXPECT_EQ(registrar.GetAppBackgroundColor(app_id), gfx::kGoogleGrey800);
}

// Ensures both the "audio" and "gallery" flavours of the MediaApp can be
// launched at the same time when launched via the files app.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest,
                       FileOpenCanLaunchBothAudioAndImages) {
  base::HistogramTester histograms;

  WaitForTestSystemAppInstall();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFileJpeg640x480), TestFile(kFileAudioOgg)});

  // Launch with the audio file.
  EXPECT_EQ(folder.Open(TestFile(kFileAudioOgg)),
            platform_util::OPEN_SUCCEEDED);
  WaitForBrowserCount(2);
  Browser* audio_app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* audio_web_ui =
      audio_app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(audio_web_ui);

  // Launch with the image file.
  EXPECT_EQ(folder.Open(TestFile(kFileJpeg640x480)),
            platform_util::OPEN_SUCCEEDED);
  WaitForBrowserCount(3);
  Browser* image_app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* image_web_ui =
      image_app_browser->tab_strip_model()->GetActiveWebContents();
  PrepareAppForTest(image_web_ui);

  EXPECT_NE(image_app_browser, audio_app_browser);
  EXPECT_TRUE(web_app::IsBrowserForSystemWebApp(image_app_browser,
                                                SystemAppType::MEDIA));
  EXPECT_TRUE(web_app::IsBrowserForSystemWebApp(audio_app_browser,
                                                SystemAppType::MEDIA));

  // Verify that launch params were correctly proceed by the "second" app to
  // launch.
  EXPECT_EQ(kFileAudioOgg, WaitForAudioTrackTitle(audio_web_ui));
  EXPECT_EQ("640x480", WaitForImageAlt(image_web_ui, kFileJpeg640x480));

  // Check the metrics are recorded.
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromFileManager", 2);
}

// Ensures audio files opened in the media app successfully autoplay.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest, Autoplay) {
  content::WebContents* web_ui = LaunchWithOneTestFile(kFileAudioOgg);

  EXPECT_EQ(kFileAudioOgg, WaitForAudioTrackTitle(web_ui));

  constexpr char kWaitForPlayedLength[] = R"(
      (async function waitForPlayedLength() {
        const audioElement = await waitForNode('audio');
        if (audioElement.played.length > 0) {
          return audioElement.played.length;
        }
        // Wait for a timeupdate. If autoplay malfunctions, this will timeout.
        await new Promise(resolve => {
          audioElement.addEventListener('timeupdate', resolve, {once: true});
        });
        return audioElement.played.length;
      })();
  )";

  EXPECT_LE(
      1, MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kWaitForPlayedLength));
}

// Ensures the autoplay on audio file launch updates the global media controls
// with an appropriate media source name.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAudioEnabledTest, MediaControls) {
  using absl::optional;
  class MediaControlsObserver
      : public media_session::mojom::MediaControllerObserver {
   public:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr info) override {}
    void MediaSessionMetadataChanged(
        const optional<media_session::MediaMetadata>& metadata) override {
      if (metadata) {
        source_title = metadata->source_title;
        if (run_loop.IsRunningOnCurrentThread()) {
          run_loop.Quit();
        }
      }
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& action)
        override {}
    void MediaSessionChanged(
        const optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const optional<media_session::MediaPosition>& position) override {}

    std::u16string source_title;
    base::RunLoop run_loop;
  } observer;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_(&observer);
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote;
  content::GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote.BindNewPipeAndPassReceiver());
  media_controller_remote->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());

  LaunchWithOneTestFile(kFileAudioOgg);

  if (observer.source_title.empty()) {
    observer.run_loop.Run();
  }

  EXPECT_EQ(u"Gallery", observer.source_title);
}

// Test that the MediaApp can traverse other files in the directory of a file
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
  TouchFileSync(copied_jpeg_640x480, base::Time::UnixEpoch() + base::Days(1));

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
      "lastLoadedReceivedFileList().item(0).renameOriginalFile('x.jpg')"
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

// Integration test for deleting a file using the WritableFiles API.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest, DeleteFile) {
  WaitForTestSystemAppInstall();

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({
      TestFile(kFileJpeg640x480),
      TestFile(kFilePng800x600),
  });
  folder.Open(TestFile(kFileJpeg640x480));
  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  int result = 0;
  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item(0).deleteOriginalFile()"
      ".then(() => domAutomationController.send(42));";
  EXPECT_EQ(true, content::ExecuteScriptAndExtractInt(app, kScript, &result));
  EXPECT_EQ(42, result);  // Magic success (no exception thrown).

  // Ensure the file *not* deleted is the only one that remains.
  folder.Refresh();
  EXPECT_EQ(1u, folder.files().size());
  EXPECT_EQ(kFilePng800x600, folder.files()[0].BaseName().value());
}

// Integration test for deleting a special file using the WritableFiles API.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       FailToDeleteReservedFile) {
  WaitForTestSystemAppInstall();

  file_manager::test::FolderInMyFiles folder(profile());

  // Files like "thumbs.db" can't be accessed by filename using WritableFiles.
  const base::FilePath reserved_file =
      base::FilePath().AppendASCII("thumbs.db");
  folder.AddWithName(TestFile(kFileJpeg640x480), reserved_file);

  // Even though the file doesn't have a ".jpg" extension, MIME sniffing in the
  // files app should still direct the file at the image/jpeg handler of the
  // media app.
  folder.Open(reserved_file);

  content::WebContents* web_ui = PrepareActiveBrowserForTest();

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, "thumbs.db"));

  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item(0).deleteOriginalFile()"
      ".then(() => domAutomationController.send('bad-success'))"
      ".catch(e => domAutomationController.send(e.name));";
  EXPECT_EQ("InvalidModificationError",
            ExtractStringInGlobalScope(web_ui, kScript));

  // The file should still be there.
  folder.Refresh();
  EXPECT_EQ(1u, folder.files().size());
  EXPECT_EQ("thumbs.db", folder.files()[0].BaseName().value());
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, ToggleBrowserFullscreen) {
  content::WebContents* web_ui = LaunchWithOneTestFile(kFileVideoVP9);
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();

  constexpr char kToggleFullscreen[] = R"(
      (async function toggleFullscreen() {
        await customLaunchData.delegate.toggleBrowserFullscreenMode();
        domAutomationController.send("success");
      })();
  )";

  EXPECT_FALSE(app_browser->window()->IsFullscreen());

  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, kToggleFullscreen));
  EXPECT_TRUE(app_browser->window()->IsFullscreen());

  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, kToggleFullscreen));
  EXPECT_FALSE(app_browser->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, GuestCanReadLocalFonts) {
  // For this test, we first need to find a valid font to request from
  // /usr/share/fonts. build/linux/install-chromeos-fonts.py installs some known
  // fonts into /usr/*local*/share/fonts, so that's no good. A set of known font
  // files *should* be available on any machine, but the subdirectory varies.
  // E.g. NotoSans-Regular.ttf exists in fonts/truetype/noto/ on some machines,
  // but it has a different parent folder on others.
  //
  // For a robust test, poke around on disk and pick the first non-zero .ttf
  // file to deliver.
  //
  // Note that although the path differs across bots, it will be consistent on
  // the ChromeOS image.
  const std::string font_to_try = FindAnyTTF();
  DLOG(INFO) << "Found: " << font_to_try;

  constexpr char kFetchTestFont[] = R"(
      (async function fetchTestFont() {
        try {
          const response = await fetch('/fonts/$1');
          const blob = await response.blob();

          if (response.status === 200 && blob.size > 0) {
            domAutomationController.send('success');
          } else {
            domAutomationController.send(
                `Failed: status:$${response.status} size:$${blob.size}`);
          }
        } catch (e) {
          domAutomationController.send(`Failed: $${e}`);
        }
      })();
  )";
  const std::string script =
      base::ReplaceStringPlaceholders(kFetchTestFont, {font_to_try}, nullptr);

  content::WebContents* web_ui = LaunchWithNoFiles();
  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, script));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationAudioEnabledTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationAudioDisabledTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationPdfDisabledTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationDarkLightModeEnabledTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationDarkLightModeDisabledTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationAllProfilesTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationWithFilesAppTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationWithFilesAppAllProfilesTest);
