// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_apps_publisher.h"

#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace {

using Accesses = std::vector<apps::CapabilityAccessPtr>;

// This fake intercepts and tracks all calls to Publish().
class LacrosAppsPublisherFake : public LacrosAppsPublisher {
 public:
  LacrosAppsPublisherFake() : LacrosAppsPublisher() {
    // Since LacrosAppsPublisherTest run without Ash, Lacros won't get
    // the Ash extension keeplist data from Ash (passed via crosapi). Therefore,
    // set empty ash keeplist for test.
    extensions::SetEmptyAshKeeplistForTest();
    apps::EnableHostedAppsInLacrosForTesting();
  }
  ~LacrosAppsPublisherFake() override = default;

  LacrosAppsPublisherFake(const LacrosAppsPublisherFake&) = delete;
  LacrosAppsPublisherFake& operator=(const LacrosAppsPublisherFake&) = delete;

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

  std::vector<Accesses>& accesses_history() { return accesses_history_; }

 private:
  // Override to intercept calls to PublishCapabilityAccesses().
  void PublishCapabilityAccesses(Accesses accesses) override {
    accesses_history_.push_back(std::move(accesses));
  }

  // Override to pretend that crosapi is initialized.
  bool InitializeCrosapi() override { return true; }

  // Holds the contents of all calls to PublishCapabilityAccesses() in
  // chronological order.
  std::vector<Accesses> accesses_history_;
};

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

using LacrosAppsPublisherTest = extensions::ExtensionBrowserTest;

// Verify AppCapabilityAccess is modified for browser tabs.
IN_PROC_BROWSER_TEST_F(LacrosAppsPublisherTest, RequestAccessingForTab) {
  std::unique_ptr<LacrosAppsPublisherFake> publisher =
      std::make_unique<LacrosAppsPublisherFake>();
  publisher->Initialize();

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("app.com", "/ssl/google.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Request accessing the camera for `web_contents`.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  publisher->VerifyAppCapabilityAccess(app_constants::kLacrosAppId, 1u, true,
                                       absl::nullopt);

  // Request accessing the microphone for `web_contents`.
  base::OnceClosure audio_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  publisher->VerifyAppCapabilityAccess(app_constants::kLacrosAppId, 2u,
                                       absl::nullopt, true);

  // Stop accessing the camera for `web_contents`.
  std::move(video_closure).Run();
  publisher->VerifyAppCapabilityAccess(app_constants::kLacrosAppId, 3u, false,
                                       absl::nullopt);

  // Stop accessing the microphone for `web_contents`.
  std::move(audio_closure).Run();
  publisher->VerifyAppCapabilityAccess(app_constants::kLacrosAppId, 4u,
                                       absl::nullopt, false);
}

// Verify AppCapabilityAccess for Chrome apps is not handled by
// LacrosAppsPublisher.
IN_PROC_BROWSER_TEST_F(LacrosAppsPublisherTest, NoRequestAccessingForHostApp) {
  std::unique_ptr<LacrosAppsPublisherFake> publisher =
      std::make_unique<LacrosAppsPublisherFake>();
  publisher->Initialize();

  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));
  ASSERT_TRUE(extension);

  // Navigate to the app's launch URL.
  auto url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_content =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_content);

  // Request accessing the camera and microphone for `web_contents`.
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  // Verify the publisher does not handle the access for the Chrome app.
  ASSERT_TRUE(publisher->accesses_history().empty());
}

// Verify AppCapabilityAccess for web apps is not handled by
// LacrosAppsPublisher.
IN_PROC_BROWSER_TEST_F(LacrosAppsPublisherTest, NoRequestAccessingForWebApp) {
  std::unique_ptr<LacrosAppsPublisherFake> publisher =
      std::make_unique<LacrosAppsPublisherFake>();
  publisher->Initialize();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = url;
  web_app_info->scope = url;
  auto app_id = web_app::test::InstallWebApp(browser()->profile(),
                                             std::move(web_app_info));

  // Launch |app_id| for the web app.
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

}  // namespace
