// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"

#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/lacros/for_which_extension_type.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/aura/window.h"

namespace {

using Apps = std::vector<apps::AppPtr>;
using Accesses = std::vector<apps::CapabilityAccessPtr>;

// This fake intercepts and tracks all calls to Publish().
class LacrosExtensionAppsPublisherFake : public LacrosExtensionAppsPublisher {
 public:
  LacrosExtensionAppsPublisherFake()
      : LacrosExtensionAppsPublisher(InitForChromeApps()) {
    // Since LacrosExtensionAppsPublisherTest run without Ash, Lacros won't get
    // the Ash extension keeplist data from Ash (passed via crosapi). Therefore,
    // set empty ash keeplist for test.
    extensions::SetEmptyAshKeeplistForTest();
    apps::EnableHostedAppsInLacrosForTesting();
  }
  ~LacrosExtensionAppsPublisherFake() override = default;

  LacrosExtensionAppsPublisherFake(const LacrosExtensionAppsPublisherFake&) =
      delete;
  LacrosExtensionAppsPublisherFake& operator=(
      const LacrosExtensionAppsPublisherFake&) = delete;

  void VerifyAppCapabilityAccess(const std::string& app_id,
                                 size_t count,
                                 absl::optional<bool> accessing_camera,
                                 absl::optional<bool> accessing_microphone) {
    ASSERT_EQ(count, accesses_history().size());
    Accesses& accesses = accesses_history().back();
    ASSERT_EQ(1u, accesses.size());
    EXPECT_EQ(app_id, accesses[0]->app_id);

    if (accessing_camera.has_value()) {
      ASSERT_TRUE(accesses[0]->camera.has_value());
      EXPECT_EQ(accessing_camera.value(), accesses[0]->camera.value());
    } else {
      ASSERT_FALSE(accesses[0]->camera.has_value());
    }

    if (accessing_microphone.has_value()) {
      ASSERT_TRUE(accesses[0]->microphone.has_value());
      EXPECT_EQ(accessing_microphone.value(), accesses[0]->microphone.value());
    } else {
      ASSERT_FALSE(accesses[0]->microphone.has_value());
    }
  }

  std::vector<Apps>& apps_history() { return apps_history_; }

  std::vector<Accesses>& accesses_history() { return accesses_history_; }

  std::map<std::string, std::string>& app_windows() { return app_windows_; }

 private:
  // Override to intercept calls to Publish().
  void Publish(Apps apps) override { apps_history_.push_back(std::move(apps)); }

  // Override to intercept calls to PublishCapabilityAccesses().
  void PublishCapabilityAccesses(Accesses accesses) override {
    accesses_history_.push_back(std::move(accesses));
  }

  // Override to intercept calls to OnAppWindowAdded().
  void OnAppWindowAdded(const std::string& app_id,
                        const std::string& window_id) override {
    app_windows_[window_id] = app_id;
  }

  // Override to intercept calls to OnAppWindowRemoved().
  void OnAppWindowRemoved(const std::string& app_id,
                          const std::string& window_id) override {
    EXPECT_TRUE(app_windows_.find(window_id) != app_windows_.end());
    EXPECT_EQ(app_windows_[window_id], app_id);
    app_windows_.erase(window_id);
  }

  // Override to pretend that crosapi is initialized.
  bool InitializeCrosapi() override { return true; }

  // Holds the contents of all calls to Publish() in chronological order.
  std::vector<Apps> apps_history_;

  // Holds the contents of all calls to PublishCapabilityAccesses() in
  // chronological order.
  std::vector<Accesses> accesses_history_;

  // Holds the list of currently showing app windows, as seen by
  // OnAppWindowAdded() and OnAppWindowRemoved(). The key is the window_id and
  // the value is the app_id.
  std::map<std::string, std::string> app_windows_;
};

const size_t kDefaultAppsSize = 1u;

// Verify that only default apps have been published. Web store app
// (hosted app) is the default app that is always loaded by chrome component
// extension loader.
void VerifyOnlyDefaultAppsPublished(
    LacrosExtensionAppsPublisherFake* publisher) {
  ASSERT_GE(publisher->apps_history().size(), 1u);

  Apps& default_apps = publisher->apps_history()[0];
  ASSERT_EQ(kDefaultAppsSize, default_apps.size());

  auto& default_app = default_apps[0];
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success = lacros_extensions_util::GetProfileAndExtension(
      default_app->app_id, &profile, &extension);
  ASSERT_TRUE(success);
  ASSERT_TRUE(extension->is_hosted_app());
  ASSERT_EQ(extensions::kWebStoreAppId, extension->id());
  ASSERT_TRUE(default_app->is_platform_app.has_value());
  ASSERT_FALSE(default_app->is_platform_app.value());
}

// Adds a fake media device with the specified `stream_type` and starts
// capturing. Returns a closure to stop the capturing.
base::OnceClosure StartMediaCapture(content::WebContents* web_contents,
                                    blink::mojom::MediaStreamType stream_type) {
  blink::mojom::StreamDevices fake_devices;
  blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");

  if (blink::IsAudioInputMediaType(stream_type)) {
    fake_devices.audio_device = device;
  } else {
    fake_devices.video_device = device;
  }

  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents, fake_devices);

  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());

  return base::BindOnce(
      [](std::unique_ptr<content::MediaStreamUI> ui) { ui.reset(); },
      std::move(ui));
}

using LacrosExtensionAppsPublisherTest = extensions::ExtensionBrowserTest;

// When publisher is created and initialized, only chrome default apps
// should be published.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, DefaultApps) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  ASSERT_TRUE(publisher->apps_history().empty());
  publisher->Initialize();
  VerifyOnlyDefaultAppsPublished(publisher.get());
}

// If the profile has one app installed, then creating a publisher should
// immediately result in a call to Publish() with 1 entry.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, OneApp) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  ASSERT_GE(publisher->apps_history().size(), 1u);
  Apps& apps = publisher->apps_history()[0];
  // The platform app is added after the default apps.
  ASSERT_EQ(kDefaultAppsSize + 1u, apps.size());
  auto& platform_app = apps.back();
  ASSERT_TRUE(platform_app->is_platform_app.has_value());
  ASSERT_TRUE(platform_app->is_platform_app.value());
}

// Same as OneApp, but with two pre-installed apps.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, TwoApps) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal_id"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  ASSERT_GE(publisher->apps_history().size(), 1u);
  Apps& apps = publisher->apps_history()[0];
  // The platform apps are added after the default apps.
  ASSERT_EQ(kDefaultAppsSize + 2u, apps.size());
  auto& platform_app_1 = apps[kDefaultAppsSize];
  ASSERT_TRUE(platform_app_1->is_platform_app.has_value());
  ASSERT_TRUE(platform_app_1->is_platform_app.value());
  auto& platform_app_2 = apps[kDefaultAppsSize + 1];
  ASSERT_TRUE(platform_app_2->is_platform_app.has_value());
  ASSERT_TRUE(platform_app_2->is_platform_app.value());
}

// If an app is installed after the AppsPublisher is created, there should be a
// corresponding event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest,
                       InstallAppAfterCreate) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  ASSERT_TRUE(publisher->apps_history().empty());
  publisher->Initialize();
  VerifyOnlyDefaultAppsPublished(publisher.get());
  ASSERT_GE(publisher->apps_history().size(), 1u);

  LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  ASSERT_GE(publisher->apps_history().size(), 2u);
  Apps& apps = publisher->apps_history().back();
  ASSERT_EQ(1u, apps.size());
  auto& platform_app = apps.back();
  ASSERT_TRUE(platform_app->is_platform_app.has_value());
  ASSERT_TRUE(platform_app->is_platform_app.value());
}

// If an app is unloaded, there should be a corresponding unload event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Unload) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  UnloadExtension(extension->id());

  ASSERT_GE(publisher->apps_history().size(), 2u);

  // The first event should be a ready event.
  {
    Apps& apps = publisher->apps_history()[0];
    ASSERT_EQ(kDefaultAppsSize + 1u, apps.size());
    ASSERT_EQ(apps.back()->readiness, apps::Readiness::kReady);
  }

  // The last event should be an unload event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kDisabledByUser);
  }
}

// If an app is uninstalled, there should be a corresponding uninstall event.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, Uninstall) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();
  UninstallExtension(extension->id());

  ASSERT_GE(publisher->apps_history().size(), 2u);

  // The first event should be a ready event.
  {
    Apps& apps = publisher->apps_history()[0];
    ASSERT_EQ(2u, apps.size());
    ASSERT_EQ(apps[1]->readiness, apps::Readiness::kReady);
  }

  // The last event should be an uninstall event.
  {
    Apps& apps = publisher->apps_history().back();
    ASSERT_EQ(1u, apps.size());
    ASSERT_EQ(apps[0]->readiness, apps::Readiness::kUninstalledByUser);
  }
}

// If the app window is loaded after to creating the publisher, everything
// should still work.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, LaunchAppWindow) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  // There should be no windows tracked.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }

  // Load and launch the app.
  const extensions::Extension* extension =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps/minimal"));
  auto* registry = extensions::AppWindowRegistry::Get(profile());
  extensions::AppWindow* app_window =
      registry->GetCurrentAppWindowForApp(extension->id());
  ASSERT_TRUE(app_window);

  // Check that the window is tracked correctly.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(1u, app_windows.size());
    EXPECT_EQ(app_windows.begin()->second, extension->id());
    EXPECT_EQ(app_windows.begin()->first,
              lacros_window_utility::GetRootWindowUniqueId(
                  app_window->GetNativeWindow()));
  }

  // Check that the window is no longer tracked. This process is asynchronous.
  app_window->GetBaseWindow()->Close();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }
}

// If the app window is loaded prior to creating the publisher, everything
// should still work.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, PreLaunchAppWindow) {
  const extensions::Extension* extension =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps/minimal"));
  auto* registry = extensions::AppWindowRegistry::Get(profile());
  extensions::AppWindow* app_window =
      registry->GetCurrentAppWindowForApp(extension->id());
  ASSERT_TRUE(app_window);

  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  // Check that the window is tracked correctly.
  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(1u, app_windows.size());
    EXPECT_EQ(app_windows.begin()->second, extension->id());
    EXPECT_EQ(app_windows.begin()->first,
              lacros_window_utility::GetRootWindowUniqueId(
                  app_window->GetNativeWindow()));
  }

  // Check that the window is no longer tracked. This process is asynchronous.
  app_window->GetBaseWindow()->Close();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  {
    auto& app_windows = publisher->app_windows();
    ASSERT_EQ(0u, app_windows.size());
  }
}

// Verify AppCapabilityAccess is modified for Chrome apps.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest,
                       RequestAccessingForPlatformApp) {
  const extensions::Extension* extension =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps/minimal"));
  ASSERT_TRUE(extension);

  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  auto* registry = extensions::AppWindowRegistry::Get(profile());
  extensions::AppWindow* app_window =
      registry->GetCurrentAppWindowForApp(extension->id());
  ASSERT_TRUE(app_window);
  content::WebContents* web_contents = app_window->web_contents();
  ASSERT_TRUE(web_contents);

  // Request accessing the camera for `web_contents`.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  publisher->VerifyAppCapabilityAccess(extension->id(), 1u, true,
                                       absl::nullopt);

  // Request accessing the microphone for `web_contents`.
  base::OnceClosure audio_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  publisher->VerifyAppCapabilityAccess(extension->id(), 2u, absl::nullopt,
                                       true);

  // Stop accessing the microphone for `web_contents`.
  std::move(audio_closure).Run();
  publisher->VerifyAppCapabilityAccess(extension->id(), 3u, absl::nullopt,
                                       false);

  // Stop accessing the camera for `web_contents`.
  std::move(video_closure).Run();
  publisher->VerifyAppCapabilityAccess(extension->id(), 4u, false,
                                       absl::nullopt);
}

// Verify AppCapabilityAccess for web apps is not handled by
// LacrosExtensionAppsPublisher.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, NoAccessingForWebApp) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = url;
  web_app_info->scope = url;
  auto app_id = web_app::test::InstallWebApp(browser()->profile(),
                                             std::move(web_app_info));

  // Launch `app_id` for the web app.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateToURLAndWait(browser(), url);
  content::WebContents* web_content =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_content);

  // Request accessing the camera and microphone for `web_contents`.
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  // Verify the publisher does not handle the access for the web app.
  ASSERT_TRUE(publisher->accesses_history().empty());
}

// Verify AppCapabilityAccess for browser tabs is not handled by
// LacrosExtensionAppsPublisher.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsPublisherTest, NoAccessingForTab) {
  std::unique_ptr<LacrosExtensionAppsPublisherFake> publisher =
      std::make_unique<LacrosExtensionAppsPublisherFake>();
  publisher->Initialize();

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("app.com", "/ssl/google.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Request accessing the camera and microphone for `web_contents`.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  // Verify the publisher does not handle the access for the tab.
  ASSERT_TRUE(publisher->accesses_history().empty());
}

}  // namespace
