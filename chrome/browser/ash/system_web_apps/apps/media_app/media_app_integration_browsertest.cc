// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check_deref.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/entry_info.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/message_center/public/cpp/notification.h"

using ash::SystemWebAppType;
using platform_util::OpenOperationResult;

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

// Runs the provided `script` in a non-isolated JS world that can access
// variables defined in global scope (otherwise only DOM queries are allowed).
// The script's completion value must be a boolean.
bool ExtractBoolInGlobalScope(content::WebContents* web_ui,
                              const std::string& script) {
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);
  return content::EvalJs(app, script).ExtractBool();
}

class MediaAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  MediaAppIntegrationTest() {
    // Init with survey triggers enabled to ensure no bad interactions.
    // Always enable because not all bots run with
    // fieldtrial_testing_config.json. Simplify (slightly) by using the same
    // survey trigger ID in the params.
    const base::FieldTrialParams survey_params{
        {"prob", "1"},  // 100% probability for testing.
        {"survey_cycle_length", "90"},
        {"survey_start_date_ms", "1662336000000"},
        {"trigger_id", "s5EmUqzvY0jBnuKU19R0Tdf9ticy"}};

    feature_list_.InitWithFeaturesAndParameters(
        {{ash::kHatsMediaAppPdfSurvey.feature, survey_params},
         {ash::kHatsPhotosExperienceSurvey.feature, survey_params}},
        {});
  }

  void SetUp() override {
    ash::SystemWebAppIntegrationTest::SetUp();

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SystemWebAppIntegrationTest::SetUpCommandLine(command_line);

    // Use a fake audio stream. Some tests make noise otherwise, and could fight
    // with parallel tests for access to the audio device.
    command_line->AppendSwitch(switches::kDisableAudioOutput);
  }

  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();
  }

  apps::AppLaunchParams MediaAppLaunchParams();
  std::string MediaAppAppId();

  // Helper to initiate a test by launching a single file, as if from the files
  // app.
  content::WebContents* LaunchWithOneTestFile(const char* file);

  // Helper to initiate a test by launching with no files (zero state).
  content::WebContents* LaunchWithNoFiles();

  // Launch the MediApp with the given |file_path| as a launch param, and wait
  // for the application to finish loading.
  content::WebContents* DirectlyLaunchWithFile(const base::FilePath& file_path);

  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }
  struct DataArgsHelper {
    const char* const open_image = "0";
    const char* const open_video = "0";
    const char* const edit_image = "0";
    const char* const edit_video = "0";
  };
  void ExpectProductSurveyData(DataArgsHelper expected_data) {
    auto data = HatsProductSpecificDataForMediaApp();
    EXPECT_EQ(data["did_open_image_in_gallery"], expected_data.open_image);
    EXPECT_EQ(data["did_open_video_in_gallery"], expected_data.open_video);
    EXPECT_EQ(data["clicked_edit_image_in_photos"], expected_data.edit_image);
    EXPECT_EQ(data["clicked_edit_video_in_photos"], expected_data.edit_video);
  }

  void LaunchAndWait(const ash::SystemAppLaunchParams& params) {
    content::TestNavigationObserver observer =
        content::TestNavigationObserver(GURL(ash::kChromeUIMediaAppURL));
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::MEDIA,
                                 params);
    observer.Wait();
  }

 protected:
  ash::NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<file_manager::test::FolderInMyFiles> launch_folder_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

class MediaAppIntegrationWithFilesAppTest : public MediaAppIntegrationTest {
  void SetUpOnMainThread() override {
    MediaAppIntegrationTest::SetUpOnMainThread();
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
  }
};

class MediaAppIntegrationPhotosIntegrationTest
    : public MediaAppIntegrationTest {
 public:
  void TestPhotosIntegrationForVideo(bool expect_flag_enabled,
                                     const char* photos_version) {
    TestPhotosIntegration(expect_flag_enabled, photos_version,
                          /* flag= */ "photosAvailableForVideo");
  }

  void TestPhotosIntegrationForImage(bool expect_flag_enabled,
                                     const char* photos_version) {
    TestPhotosIntegration(expect_flag_enabled, photos_version,
                          /* flag= */ "photosAvailableForImage");
  }

 private:
  void TestPhotosIntegration(bool expect_flag_enabled,
                             const char* photos_version,
                             const char* flag) {
    InstallPhotosApp(profile(), photos_version);
    content::WebContents* web_ui = LaunchWithNoFiles();

    EXPECT_EQ(expect_flag_enabled, GetFlagInApp(web_ui, flag));
  }

  static apps::AppPtr MakePhotosApp(const char* photos_version) {
    auto app = std::make_unique<apps::App>(apps::AppType::kChromeApp,
                                           arc::kGooglePhotosAppId);
    // TODO(b/239776967): expand testing to adjust app readiness.
    app->readiness = apps::Readiness::kReady;
    app->version = photos_version;
    return app;
  }

  static void InstallPhotosApp(Profile* profile, const char* photos_version) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakePhotosApp(photos_version));
    proxy->OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
                  /*should_notify_initialized=*/false);
  }

  static bool GetFlagInApp(content::WebContents* web_ui, const char* flag) {
    constexpr char kGetLoadTimeData[] = R"(
        (function getLoadTimeData() {
          return !!loadTimeData?.data_['$1'];
        })()
    )";

    return ExtractBoolInGlobalScope(
        web_ui,
        base::ReplaceStringPlaceholders(kGetLoadTimeData, {flag}, nullptr));
  }
};

using MediaAppIntegrationAllProfilesTest = MediaAppIntegrationTest;
using MediaAppIntegrationWithFilesAppAllProfilesTest =
    MediaAppIntegrationWithFilesAppTest;

// Scoped observer of notifications that will spin a run loop until a
// notification is displayed.
class NotificationWatcher : public NotificationDisplayService::Observer {
 public:
  NotificationWatcher(Profile* profile,
                      ash::NetworkPortalDetectorMixin& network_portal_detector)
      : profile_(profile) {
    // Notifications only fire if the device is "online". Simulate that.
    network_portal_detector.SimulateDefaultNetworkState(
        ash::NetworkPortalDetectorMixin::NetworkStatus::kOnline);

    NotificationDisplayServiceFactory::GetForProfile(profile_)->AddObserver(
        this);
  }
  ~NotificationWatcher() override {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }
  std::string NextSeenNotificationId() {
    if (seen_notification_id_.empty()) {
      run_loop_.Run();
    }
    return seen_notification_id_;
  }

 private:
  raw_ptr<Profile> profile_;
  base::RunLoop run_loop_;
  std::string seen_notification_id_;

  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    seen_notification_id_ = notification.id();
    if (run_loop_.IsRunningOnCurrentThread()) {
      run_loop_.Quit();
    }
  }

  void OnNotificationClosed(const std::string& notification_id) override {}
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {}
};

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

content::WebContents* PrepareActiveBrowserForTest(
    int expected_browser_count = 2) {
  WaitForBrowserCount(expected_browser_count);
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* web_ui =
      app_browser->tab_strip_model()->GetActiveWebContents();
  MediaAppUiBrowserTest::PrepareAppForTest(web_ui);
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
// The script's completion value must be a string.
std::string ExtractStringInGlobalScope(content::WebContents* web_ui,
                                       const std::string& script) {
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);
  return content::EvalJs(app, script).ExtractString();
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

apps::AppLaunchParams MediaAppIntegrationTest::MediaAppLaunchParams() {
  return LaunchParamsForApp(ash::SystemWebAppType::MEDIA);
}

std::string MediaAppIntegrationTest::MediaAppAppId() {
  return *GetManager().GetAppIdForSystemApp(ash::SystemWebAppType::MEDIA);
}

content::WebContents* MediaAppIntegrationTest::LaunchWithOneTestFile(
    const char* file) {
  launch_folder_ =
      std::make_unique<file_manager::test::FolderInMyFiles>(profile());
  launch_folder_->Add({TestFile(file)});
  EXPECT_EQ(launch_folder_->Open(TestFile(file)),
            platform_util::OPEN_SUCCEEDED);
  return PrepareActiveBrowserForTest();
}

content::WebContents* MediaAppIntegrationTest::LaunchWithNoFiles() {
  content::WebContents* web_ui = LaunchApp(MediaAppLaunchParams());
  MediaAppUiBrowserTest::PrepareAppForTest(web_ui);
  return web_ui;
}

content::WebContents* MediaAppIntegrationTest::DirectlyLaunchWithFile(
    const base::FilePath& file_path) {
  apps::AppLaunchParams params = MediaAppLaunchParams();
  params.launch_files.push_back(file_path);
  return LaunchApp(std::move(params));
}

std::vector<apps::IntentLaunchInfo> GetAppsForMimeType(
    apps::AppServiceProxy* proxy,
    const std::string& mime_type) {
  std::vector<apps::IntentFilePtr> intent_files;
  auto file =
      std::make_unique<apps::IntentFile>(GURL("filesystem://path/to/file.bin"));
  file->mime_type = mime_type;
  file->is_directory = false;
  intent_files.push_back(std::move(file));
  return proxy->GetAppsForFiles(std::move(intent_files));
}

}  // namespace

// Test that the Media App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaApp) {
  const GURL url(ash::kChromeUIMediaAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::MEDIA, url, "Gallery"));
}

// Test that the MediaApp successfully loads a file passed in on its launch
// params. This exercises only web_applications logic.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchWithFile) {
  // Launch the App for the first time.
  content::WebContents* app = DirectlyLaunchWithFile(TestFile(kFilePng800x600));
  Browser* first_browser = chrome::FindBrowserWithActiveWindow();
  MediaAppUiBrowserTest::PrepareAppForTest(app);

  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));
  ExpectProductSurveyData({.open_image = "1"});

  // Launch with a different file in a new window.
  app = DirectlyLaunchWithFile(TestFile(kFileJpeg640x480));
  Browser* second_browser = chrome::FindBrowserWithActiveWindow();
  MediaAppUiBrowserTest::PrepareAppForTest(app);

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));
  EXPECT_NE(first_browser, second_browser);
  ExpectProductSurveyData({.open_image = "1"});  // The "1" is a bool.
}

// Test that the MediaApp successfully loads a file using
// LaunchSystemWebAppAsync. This exercises high level integration with SWA
// platform (a different code path than MediaAppLaunchWithFile test).
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       MediaAppWithLaunchSystemWebAppAsync) {
  // Launch the App for the first time.
  ash::SystemAppLaunchParams audio_params;
  audio_params.launch_paths.push_back(TestFile(kFilePng800x600));
  LaunchAndWait(audio_params);
  Browser* first_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* app = PrepareActiveBrowserForTest();

  EXPECT_EQ("800x600", WaitForImageAlt(app, kFilePng800x600));
  ExpectProductSurveyData({.open_image = "1"});

  // Launch the App for the second time.
  ash::SystemAppLaunchParams image_params;
  image_params.launch_paths.push_back(TestFile(kFileJpeg640x480));
  LaunchAndWait(image_params);
  app = PrepareActiveBrowserForTest(3);
  Browser* second_browser = chrome::FindBrowserWithActiveWindow();

  EXPECT_EQ("640x480", WaitForImageAlt(app, kFileJpeg640x480));
  EXPECT_NE(first_browser, second_browser);
  ExpectProductSurveyData({.open_image = "1"});
}

// Test that the Media App launches a single window for images.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchImageMulti) {
  ash::SystemAppLaunchParams image_params;
  image_params.launch_paths = {TestFile(kFilePng800x600),
                               TestFile(kFileJpeg640x480)};

  LaunchAndWait(image_params);

  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, browser_list->size());  // 1 extra for the browser test browser.

  content::TitleWatcher watcher(
      browser_list->get(1)->tab_strip_model()->GetActiveWebContents(),
      u"image.png");
  EXPECT_EQ(u"image.png", watcher.WaitAndGetTitle());
  ExpectProductSurveyData({.open_image = "1"});
}

// Test that the Media App launches multiple windows for PDFs.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppLaunchPdfMulti) {
  ash::SystemAppLaunchParams pdf_params;
  pdf_params.launch_paths = {TestFile(kFilePdfTall), TestFile(kFilePdfImg)};

  LaunchAndWait(pdf_params);

  WaitForBrowserCount(3);  // 1 extra for the browser test browser.
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(3u, browser_list->size());

  content::TitleWatcher watcher1(
      browser_list->get(1)->tab_strip_model()->GetActiveWebContents(),
      u"tall.pdf");
  content::TitleWatcher watcher2(
      browser_list->get(2)->tab_strip_model()->GetActiveWebContents(),
      u"img.pdf");
  EXPECT_EQ(u"tall.pdf", watcher1.WaitAndGetTitle());
  EXPECT_EQ(u"img.pdf", watcher2.WaitAndGetTitle());
  ExpectProductSurveyData({});  // Only images and video are tracked.
}

// Test that the Media App appears as a handler for files in the App Service.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppHandlesIntents) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  const std::string media_app_id = MediaAppAppId();

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

namespace {
// icon-button ids are calculated from a hash of the button labels. Id is used
// because the UI toolkit has loose guarantees about where the actual label
// appears in the shadow DOM.
constexpr char kInfoButtonSelector[] = "#icon-button-2283726";
constexpr char kAnnotationButtonSelector[] = "#icon-button-2138468";
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
      return button.hasAttribute('selected');
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
  content::WebContents* app =
      DirectlyLaunchWithFile(TestFile(kFileJpeg640x480));
  MediaAppUiBrowserTest::PrepareAppForTest(app);

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
  content::WebContents* app =
      DirectlyLaunchWithFile(TestFile(kFileJpeg640x480));
  MediaAppUiBrowserTest::PrepareAppForTest(app);
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
      const discardButton = await getNode('#DiscardEdits',
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
          '#Done', ['backlight-app-bar', 'backlight-app']);
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
  EXPECT_EQ(true, ExecJs(app, kOpenPdfInViewer));

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
  EXPECT_EQ(true, ExecJs(app, kAdd270DegreeRotation));

  // Navigate to the next file in the directory.
  EXPECT_EQ(true, ExecJs(web_ui, "advance(1)"));

  // Width and height should be swapped now.
  EXPECT_EQ("272x378", WaitForImageAlt(web_ui, kRaw378x272));
  // Raw files aren't tracked (they are not directly editable by Photos).
  ExpectProductSurveyData({});
}

// Ensures that chrome://media-app is available as a file task for the ChromeOS
// file manager and eligible for opening appropriate files / mime types.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaAppEligibleOpenTask) {
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(TestFile(kFilePng800x600));
  file_paths.push_back(TestFile(kFileVideoVP9));
  file_paths.push_back(TestFile(kFileAudioOgg));

  for (const auto& file_path : file_paths) {
    std::vector<file_manager::file_tasks::FullTaskDescriptor> result =
        file_manager::test::GetTasksForFile(profile(), file_path);

    ASSERT_LT(0u, result.size());
    EXPECT_EQ(1u, result.size());
    const auto& task = result[0];
    const auto& descriptor = task.task_descriptor;

    EXPECT_EQ("Gallery", task.task_title);
    EXPECT_EQ(descriptor.app_id, MediaAppAppId());
    EXPECT_EQ(ash::kChromeUIMediaAppURL, descriptor.action_id);
    EXPECT_EQ(file_manager::file_tasks::TASK_TYPE_WEB_APP,
              descriptor.task_type);
  }
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationAllProfilesTest,
                       ShownInLauncherAndSearch) {
  // Check system_web_app_manager has the correct attributes for Media App.
  auto* system_app = GetManager().GetSystemApp(ash::SystemWebAppType::MEDIA);
  EXPECT_TRUE(system_app->ShouldShowInLauncher());
  EXPECT_TRUE(system_app->ShouldShowInSearchAndShelf());
}

// Note: Error reporting tests are limited to one per test instance otherwise we
// run into "Too many calls to this API" error.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       TrustedContextReportsConsoleErrors) {
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  content::WebContents* web_ui = LaunchWithNoFiles();

  // Pass multiple arguments to console.error() to also check they are parsed
  // and captured in the error message correctly.
  constexpr char kConsoleError[] =
      "console.error('YIKES', {data: 'something'}, new Error('deep error'));";
  EXPECT_EQ(true, ExecJs(web_ui, kConsoleError));
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

  content::WebContents* web_ui = LaunchWithNoFiles();

  EXPECT_EQ(true, ExecJs(web_ui, kDomExceptionScript));
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

  content::WebContents* app = LaunchWithNoFiles();

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

  content::WebContents* web_ui = LaunchWithNoFiles();

  EXPECT_EQ(true, ExecJs(web_ui, kUnhandledRejectionScript));
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

  content::WebContents* app = LaunchWithNoFiles();

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

  content::WebContents* web_ui = LaunchWithNoFiles();

  EXPECT_EQ(true, ExecJs(web_ui, kTypeErrorScript));
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

  content::WebContents* app = LaunchWithNoFiles();

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
  MediaAppUiBrowserTest::PrepareAppForTest(web_ui);

  EXPECT_EQ(open_result, platform_util::OPEN_SUCCEEDED);

  // Check that chrome://media-app launched and the test file loads.
  EXPECT_NE(test_browser, app_browser);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(app_browser->app_name()),
            MediaAppAppId());
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));

  // Check the metric is recorded.
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromFileManager", 1);
  histograms.ExpectBucketCount("Apps.MediaApp.Load.OtherOpenWindowCount", 0, 1);
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPhotosIntegrationTest,
                       PhotosVersionNewEnoughForImageIntegration) {
  TestPhotosIntegrationForImage(/* expect_flag_enabled= */ true, "6.12");
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPhotosIntegrationTest,
                       PhotosVersionTooOldForImageIntegration) {
  TestPhotosIntegrationForImage(/* expect_flag_enabled= */ false, "6.11");
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPhotosIntegrationTest,
                       PhotosVersionNewEnoughForVideoIntegration) {
  TestPhotosIntegrationForVideo(/* expect_flag_enabled= */ true, "6.13");
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPhotosIntegrationTest,
                       PhotosVersionTooOldForVideoIntegration) {
  TestPhotosIntegrationForVideo(/* expect_flag_enabled= */ false, "5.3");
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationPhotosIntegrationTest,
                       PhotosVersionDevelopment) {
  TestPhotosIntegrationForImage(/* expect_flag_enabled= */ true, "DEVELOPMENT");
  TestPhotosIntegrationForVideo(/* expect_flag_enabled= */ true, "DEVELOPMENT");
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       HasCorrectThemeAndBackgroundColor) {
  webapps::AppId app_id = MediaAppAppId();

  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile())->registrar_unsafe();

  EXPECT_EQ(registrar.GetAppThemeColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppBackgroundColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppDarkModeThemeColor(app_id), gfx::kGoogleGrey900);
  EXPECT_EQ(registrar.GetAppDarkModeBackgroundColor(app_id),
            gfx::kGoogleGrey900);
}

// Ensures both the "audio" and "gallery" flavours of the MediaApp can be
// launched at the same time when launched via the files app.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest,
                       FileOpenCanLaunchBothAudioAndImages) {
  base::HistogramTester histograms;

  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFileJpeg640x480), TestFile(kFileAudioOgg)});

  // Launch with the audio file.
  EXPECT_EQ(folder.Open(TestFile(kFileAudioOgg)),
            platform_util::OPEN_SUCCEEDED);
  WaitForBrowserCount(2);
  Browser* audio_app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* audio_web_ui =
      audio_app_browser->tab_strip_model()->GetActiveWebContents();
  MediaAppUiBrowserTest::PrepareAppForTest(audio_web_ui);

  // Launch with the image file.
  EXPECT_EQ(folder.Open(TestFile(kFileJpeg640x480)),
            platform_util::OPEN_SUCCEEDED);
  WaitForBrowserCount(3);
  Browser* image_app_browser = chrome::FindBrowserWithActiveWindow();
  content::WebContents* image_web_ui =
      image_app_browser->tab_strip_model()->GetActiveWebContents();
  MediaAppUiBrowserTest::PrepareAppForTest(image_web_ui);

  EXPECT_NE(image_app_browser, audio_app_browser);
  EXPECT_TRUE(ash::IsBrowserForSystemWebApp(image_app_browser,
                                            ash::SystemWebAppType::MEDIA));
  EXPECT_TRUE(ash::IsBrowserForSystemWebApp(audio_app_browser,
                                            ash::SystemWebAppType::MEDIA));

  // Verify that launch params were correctly proceed by the "second" app to
  // launch.
  EXPECT_EQ(kFileAudioOgg, WaitForAudioTrackTitle(audio_web_ui));
  EXPECT_EQ("640x480", WaitForImageAlt(image_web_ui, kFileJpeg640x480));

  // Check the metrics are recorded: 2 launches from file manager machinery, and
  // 0 other open windows for the first launch; 1 other open window for the
  // second launch.
  histograms.ExpectTotalCount("Apps.DefaultAppLaunch.FromFileManager", 2);
  histograms.ExpectBucketCount("Apps.MediaApp.Load.OtherOpenWindowCount", 0, 1);
  histograms.ExpectBucketCount("Apps.MediaApp.Load.OtherOpenWindowCount", 1, 1);
}

// Ensures audio files opened in the media app successfully autoplay.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, Autoplay) {
  content::WebContents* web_ui = LaunchWithOneTestFile(kFileAudioOgg);

  EXPECT_EQ(kFileAudioOgg, WaitForAudioTrackTitle(web_ui));

  constexpr char kWaitForPlayedLength[] = R"(
      (async function waitForPlayedLength() {
        const audioElement = await waitForNode('audio[src^="blob:"]');
        console.log(`<audio> has played.length=${audioElement.played.length}.`);
        if (audioElement.played.length > 0) {
          return audioElement.played.length;
        }
        console.log(`Wait: timeupdate on <audio src="${audioElement.src}">...`);
        // Wait for a timeupdate. If autoplay malfunctions, this will timeout.
        await new Promise(resolve => {
          audioElement.addEventListener('timeupdate', resolve, {once: true});
        });
        console.log(`Returning. played.length=${audioElement.played.length}.`);
        return audioElement.played.length;
      })();
  )";

  EXPECT_LE(
      1, MediaAppUiBrowserTest::EvalJsInAppFrame(web_ui, kWaitForPlayedLength));
}

// Ensures the autoplay on audio file launch updates the global media controls
// with an appropriate media source name.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MediaControls) {
  using std::optional;
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
  EXPECT_EQ(true, ExecJs(web_ui, "advance(1)"));
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));

  // Navigating again should wraparound.
  EXPECT_EQ(true, ExecJs(web_ui, "advance(1)"));
  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // Navigate backwards.
  EXPECT_EQ(true, ExecJs(web_ui, "advance(-1)"));
  EXPECT_EQ("800x600", WaitForImageAlt(web_ui, kFilePng800x600));

  // Update the Jpeg, which invalidates open DOM File objects.
  TouchFileSync(copied_jpeg_640x480, base::Time::Now());

  // We should still be able to open the updated file.
  EXPECT_EQ(true, ExecJs(web_ui, "advance(1)"));
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
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({TestFile(kFileJpeg640x480)});
  folder.Open(TestFile(kFileJpeg640x480));
  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  // lastLoadedReceivedFileList is only set when the load IPC is received, so
  // ensure that has completed before trying to index it.
  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // Rename "image3.jpg" to "x.jpg".
  constexpr int kRenameResultSuccess = 0;
  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item(0).renameOriginalFile('x.jpg')";
  EXPECT_EQ(kRenameResultSuccess, content::EvalJs(app, kScript));

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
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({
      TestFile(kFileJpeg640x480),
      TestFile(kFilePng800x600),
  });
  folder.Open(TestFile(kFileJpeg640x480));
  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item(0).deleteOriginalFile()"
      ".then(() => 42);";
  EXPECT_EQ(42, content::EvalJs(
                    app, kScript));  // Magic success (no exception thrown).

  // Ensure the file *not* deleted is the only one that remains.
  folder.Refresh();
  EXPECT_EQ(1u, folder.files().size());
  EXPECT_EQ(kFilePng800x600, folder.files()[0].BaseName().value());
}

// Integration test for deleting a special file using the WritableFiles API.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       FailToDeleteReservedFile) {
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
      ".then(() => 'bad-success')"
      ".catch(e => e.name);";
  EXPECT_EQ("InvalidModificationError",
            ExtractStringInGlobalScope(web_ui, kScript));

  // The file should still be there.
  folder.Refresh();
  EXPECT_EQ(1u, folder.files().size());
  EXPECT_EQ("thumbs.db", folder.files()[0].BaseName().value());
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest, CheckArcWritable) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Create a new filesystem which represents a mounted archive. ARC should
  // never be able to write to such a filesystem.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
      "archive", storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), temp_dir.GetPath()));
  file_manager::VolumeManager::Get(profile())->AddVolumeForTesting(
      temp_dir.GetPath(), file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE,
      ash::DeviceType::kUnknown, true /* read_only */);

  // Copy the test image into the new filesystem.
  base::FilePath image_path = temp_dir.GetPath().Append(kFileJpeg640x480);
  EXPECT_TRUE(base::CopyFile(TestFile(kFileJpeg640x480), image_path));

  // Open the image.
  base::RunLoop run_loop;
  platform_util::OpenItem(
      profile(), image_path, platform_util::OPEN_FILE,
      base::BindLambdaForTesting(
          [&](OpenOperationResult result) { run_loop.Quit(); }));
  run_loop.Run();

  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item(0).isArcWritable()";
  EXPECT_EQ(false, content::EvalJs(app, kScript));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationWithFilesAppTest,
                       CheckBrowserWritable) {
  file_manager::test::FolderInMyFiles folder(profile());
  folder.Add({
      TestFile(kFileJpeg640x480),
      TestFile(kFilePng800x600),
  });
  // Remove ability to write to the second file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePermissionRestorer restore_permissions_for(folder.files()[1]);
  EXPECT_EQ(true, base::MakeFileUnwritable(folder.files()[1]));

  folder.Open(TestFile(kFileJpeg640x480));
  content::WebContents* web_ui = PrepareActiveBrowserForTest();
  content::RenderFrameHost* app = MediaAppUiBrowserTest::GetAppFrame(web_ui);

  EXPECT_EQ("640x480", WaitForImageAlt(web_ui, kFileJpeg640x480));

  // WaitForImageAlt only requires one image to load, but a follow-up IPC is
  // used to load additional files, which might not be available on
  // lastLoadedReceivedFileList if it is inspected now. Check the length, and
  // retry if there are not two files yet. There is enough context switching
  // here that 0-1 retries are usually sufficient.
  int received_file_length = 0;
  do {
    received_file_length =
        content::EvalJs(app, "lastLoadedReceivedFileList().length;")
            .ExtractInt();
  } while (received_file_length != 2);

  constexpr char kScript[] =
      "lastLoadedReceivedFileList().item($1).isBrowserWritable()";
  // The first file should be writable.
  EXPECT_EQ(true, content::EvalJs(app, base::ReplaceStringPlaceholders(
                                           kScript, {"0"}, nullptr)));
  // The second file should not be writable.
  EXPECT_EQ(false, content::EvalJs(app, base::ReplaceStringPlaceholders(
                                            kScript, {"1"}, nullptr)));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, OpenVideoFile) {
  content::WebContents* web_ui = LaunchWithOneTestFile(kFileVideoVP9);

  EXPECT_NE(web_ui, nullptr);
  ExpectProductSurveyData({.open_video = "1"});
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, ToggleBrowserFullscreen) {
  content::WebContents* web_ui = LaunchWithOneTestFile(kFileVideoVP9);
  Browser* app_browser = chrome::FindBrowserWithActiveWindow();

  constexpr char kToggleFullscreen[] = R"(
      (async function toggleFullscreen() {
        await customLaunchData.delegate.toggleBrowserFullscreenMode();
        return "success";
      })();
  )";

  EXPECT_FALSE(app_browser->window()->IsFullscreen());

  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, kToggleFullscreen));
  EXPECT_TRUE(app_browser->window()->IsFullscreen());

  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, kToggleFullscreen));
  EXPECT_FALSE(app_browser->window()->IsFullscreen());
}

// Tests that invoking the maybeTriggerPdfHats() MediaApp delegate method fires
// the notification that asks the user whether to complete a HaTS survey.
// Note kForceHappinessTrackingSystem is set in the test fixture to ignore the
// "dice roll" that would normally only show the prompt by chance.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MaybeTriggerPdfHats) {
  // Enable HaTS testing for PDF editing.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kForceHappinessTrackingSystem,
      features::kHappinessTrackingMediaAppPdf.name);

  content::WebContents* web_ui = LaunchWithOneTestFile(kFilePdfTall);

  constexpr char kMaybeTriggerPdfHats[] = R"(
      (async function triggerPdfHats() {
        await customLaunchData.delegate.maybeTriggerPdfHats();
        return "success";
      })();
  )";

  NotificationWatcher notification_watcher(profile(), network_portal_detector_);

  EXPECT_EQ("success",
            ExtractStringInGlobalScope(web_ui, kMaybeTriggerPdfHats));
  EXPECT_EQ(notification_watcher.NextSeenNotificationId(), "hats_notification");
}

// Tests that the Photos happiness tracking survey triggers when the monitored
// app is closed, after force-enabling display of the survey.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, MaybeTriggerPhotosHats) {
  // Enable HaTS testing for the Photos Experience.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kForceHappinessTrackingSystem,
      features::kHappinessTrackingPhotosExperience.name);

  // Pretend the Gallery is the Android Photos app, so it can be tracked for
  // survey triggers that fire when the app is closed.
  std::string media_app_app_id = MediaAppAppId();
  SetPhotosExperienceSurveyTriggerAppIdForTesting(media_app_app_id.c_str());

  NotificationWatcher notification_watcher(profile(), network_portal_detector_);

  LaunchWithNoFiles();
  chrome::FindBrowserWithActiveWindow()->window()->Close();

  EXPECT_EQ(notification_watcher.NextSeenNotificationId(), "hats_notification");

  // Avoid leaving a ref to the std::string about to be destroyed.
  SetPhotosExperienceSurveyTriggerAppIdForTesting("");
}

// Tests the survey trigger codepaths without kForceHappinessTrackingSystem,
// which skips over some important coverage.
IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, SurveyTriggers) {
  // Surveys only trigger for the device owner. Fake it.
  auto owner_id =
      ash::ProfileHelper::Get()->GetUserByProfile(profile())->GetAccountId();
  GetFakeUserManager().SetOwnerId(owner_id);

  // Do some consistency checks. If these fail then the method we want to test
  // will bail out early.
  EXPECT_TRUE(ash::ProfileHelper::IsOwnerProfile(profile()));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(ash::kHatsMediaAppPdfSurvey.feature));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(ash::kHatsPhotosExperienceSurvey.feature));

  // The constructor configures the survey features with a 100% probability, so
  // it should always trigger.
  EXPECT_TRUE(ash::HatsNotificationController::ShouldShowSurveyToProfile(
      profile(), ash::kHatsMediaAppPdfSurvey));
  EXPECT_TRUE(ash::HatsNotificationController::ShouldShowSurveyToProfile(
      profile(), ash::kHatsPhotosExperienceSurvey));
}

IN_PROC_BROWSER_TEST_P(MediaAppIntegrationTest, CapturesUserActionsForHats) {
  ExpectProductSurveyData({});  // Initially nothing.

  using base::RecordAction;
  using base::UserMetricsAction;

  // Actions aren't tracked when the app isn't running (the corresponding
  // buttons are impossible to click).
  RecordAction(UserMetricsAction("MediaApp.Image.Tool.EditInPhotos"));
  RecordAction(UserMetricsAction("MediaApp.Video.Tool.EditInPhotos"));

  LaunchWithNoFiles();
  ExpectProductSurveyData({});

  RecordAction(UserMetricsAction("MediaApp.Image.Tool.EditInPhotos"));
  ExpectProductSurveyData({.edit_image = "1"});

  RecordAction(UserMetricsAction("MediaApp.Video.Tool.EditInPhotos"));
  ExpectProductSurveyData({.edit_image = "1", .edit_video = "1"});

  // Actions are boolean, and never go back to false.
  RecordAction(UserMetricsAction("MediaApp.Image.Tool.EditInPhotos"));
  ExpectProductSurveyData({.edit_image = "1", .edit_video = "1"});
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
            return 'success';
          } else {
            return `Failed: status:$${response.status} size:$${blob.size}`;
          }
        } catch (e) {
          return `Failed: $${e}`;
        }
      })();
  )";
  const std::string script =
      base::ReplaceStringPlaceholders(kFetchTestFont, {font_to_try}, nullptr);

  content::WebContents* web_ui = LaunchWithNoFiles();
  EXPECT_EQ("success", ExtractStringInGlobalScope(web_ui, script));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationPhotosIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationAllProfilesTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MediaAppIntegrationWithFilesAppTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    MediaAppIntegrationWithFilesAppAllProfilesTest);
