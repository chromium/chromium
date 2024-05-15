// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"

#include <algorithm>
#include <memory>

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#endif
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace video_conference {

class FakeVideoConferenceMediaListener : public VideoConferenceMediaListener {
 public:
  struct State {
    int video_capture_count = 0;
    int audio_capture_count = 0;
    int window_capture_count = 0;
    int display_capture_count = 0;
  };

  FakeVideoConferenceMediaListener()
      : VideoConferenceMediaListener(
            base::DoNothing(),
            base::BindRepeating(
                [](content::WebContents* contents) -> VideoConferenceWebApp* {
                  // Should not be called.
                  EXPECT_TRUE(false);
                  return nullptr;
                }),
            base::DoNothing()) {}

  FakeVideoConferenceMediaListener(const FakeVideoConferenceMediaListener&) =
      delete;
  FakeVideoConferenceMediaListener& operator=(
      const FakeVideoConferenceMediaListener&) = delete;

  ~FakeVideoConferenceMediaListener() override = default;

  // MediaStreamCaptureIndicator::Observer overrides
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override {
    state_.video_capture_count += is_capturing_video ? 1 : -1;
  }

  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override {
    state_.audio_capture_count += is_capturing_audio ? 1 : -1;
  }

  void OnIsCapturingWindowChanged(content::WebContents* contents,
                                  bool is_capturing_window) override {
    state_.window_capture_count += is_capturing_window ? 1 : -1;
  }

  void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                   bool is_capturing_display) override {
    state_.display_capture_count += is_capturing_display ? 1 : -1;
  }

  State& state() { return state_; }

 private:
  State state_;
};

class VideoConferenceMediaListenerBrowserTest : public InProcessBrowserTest {
 public:
  VideoConferenceMediaListenerBrowserTest() = default;

  VideoConferenceMediaListenerBrowserTest(
      const VideoConferenceMediaListenerBrowserTest&) = delete;
  VideoConferenceMediaListenerBrowserTest& operator=(
      const VideoConferenceMediaListenerBrowserTest&) = delete;

  ~VideoConferenceMediaListenerBrowserTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }
#endif

  // Adds a fake media device with the specified `MediaStreamType` and starts
  // the capturing.
  // Returns a callback to stop the capturing.
  base::OnceCallback<void()> StartCapture(
      content::WebContents* web_contents,
      blink::mojom::MediaStreamType stream_type) {
    auto devices = CreateFakeDevice(stream_type);

    auto ui = GetCaptureIndicator()->RegisterMediaStream(web_contents, devices);

    ui->OnStarted(base::RepeatingClosure(),
                  content::MediaStreamUI::SourceCallback(),
                  /*label=*/std::string(), /*screen_capture_ids=*/{},
                  content::MediaStreamUI::StateChangeCallback());

    return base::BindOnce(
        [](std::unique_ptr<content::MediaStreamUI> ui) { ui.reset(); },
        std::move(ui));
  }

  VideoConferenceWebApp* CreateVcWebAppInNewTab() {
    EXPECT_TRUE(AddTabAtIndex(tab_count_, GURL("about:blank"),
                              ui::PAGE_TRANSITION_LINK));

    auto* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_count_);
    tab_count_++;
    return CreateVcWebApp(web_contents);
  }

 private:
  scoped_refptr<MediaStreamCaptureIndicator> GetCaptureIndicator() {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->GetMediaStreamCaptureIndicator();
  }

  VideoConferenceWebApp* CreateVcWebApp(content::WebContents* web_contents) {
    content::WebContentsUserData<VideoConferenceWebApp>::CreateForWebContents(
        web_contents, base::UnguessableToken::Create(),
        base::BindRepeating([](const base::UnguessableToken& id) {}),
        base::DoNothingAs<void(
            crosapi::mojom::VideoConferenceClientUpdatePtr)>());

    return content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
        web_contents);
  }

  blink::mojom::StreamDevices CreateFakeDevice(
      blink::mojom::MediaStreamType stream_type) {
    blink::mojom::StreamDevices fake_devices;
    blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");

    if (blink::IsAudioInputMediaType(stream_type)) {
      fake_devices.audio_device = device;
    } else if (blink::IsVideoInputMediaType(stream_type)) {
      fake_devices.video_device = device;
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    return fake_devices;
  }

  int tab_count_{0};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// Tests video capturing is correctly detected by VideoConferenceMediaListener.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       DeviceVideoCapturing) {
  std::unique_ptr<FakeVideoConferenceMediaListener> media_listener =
      std::make_unique<FakeVideoConferenceMediaListener>();

  // Start video capture
  auto* vc_app1 = CreateVcWebAppInNewTab();
  auto stop_capture_callback1 =
      StartCapture(&vc_app1->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_EQ(media_listener->state().video_capture_count, 1);

  auto* vc_app2 = CreateVcWebAppInNewTab();
  auto stop_capture_callback2 =
      StartCapture(&vc_app2->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_EQ(media_listener->state().video_capture_count, 2);

  // Stop video capture
  std::move(stop_capture_callback1).Run();
  EXPECT_EQ(media_listener->state().video_capture_count, 1);

  std::move(stop_capture_callback2).Run();
  EXPECT_EQ(media_listener->state().video_capture_count, 0);
}

// Tests audio capturing is correctly detected by VideoConferenceMediaListener.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       DeviceAudioCapturing) {
  std::unique_ptr<FakeVideoConferenceMediaListener> media_listener =
      std::make_unique<FakeVideoConferenceMediaListener>();

  // Start audio capture
  auto* vc_app1 = CreateVcWebAppInNewTab();
  auto stop_capture_callback1 =
      StartCapture(&vc_app1->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_EQ(media_listener->state().audio_capture_count, 1);

  auto* vc_app2 = CreateVcWebAppInNewTab();
  auto stop_capture_callback2 =
      StartCapture(&vc_app2->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_EQ(media_listener->state().audio_capture_count, 2);

  // Stop audio capture
  std::move(stop_capture_callback1).Run();
  EXPECT_EQ(media_listener->state().audio_capture_count, 1);

  std::move(stop_capture_callback2).Run();
  EXPECT_EQ(media_listener->state().audio_capture_count, 0);
}

// Tests desktop capturing is correctly detected by
// VideoConferenceMediaListener.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       DesktopCapturing) {
  std::unique_ptr<FakeVideoConferenceMediaListener> media_listener =
      std::make_unique<FakeVideoConferenceMediaListener>();

  // Start desktop capture
  auto* vc_app1 = CreateVcWebAppInNewTab();
  auto stop_capture_callback1 =
      StartCapture(&vc_app1->GetWebContents(),
                   blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);
  EXPECT_EQ(media_listener->state().window_capture_count, 1);

  auto* vc_app2 = CreateVcWebAppInNewTab();
  auto stop_capture_callback2 =
      StartCapture(&vc_app2->GetWebContents(),
                   blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE);
  EXPECT_EQ(media_listener->state().window_capture_count, 2);

  // Stop desktop capture
  std::move(stop_capture_callback1).Run();
  EXPECT_EQ(media_listener->state().window_capture_count, 1);

  std::move(stop_capture_callback2).Run();
  EXPECT_EQ(media_listener->state().window_capture_count, 0);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       TestExtensionIDShouldNotBeTracked) {
  std::unique_ptr<FakeVideoConferenceMediaListener> media_listener =
      std::make_unique<FakeVideoConferenceMediaListener>();

  // We can't directly navigate to TestExtensionUrl, so we use this workaround
  // to set the url afterwards.
  EXPECT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);

  for (const std::string& app_id : kSkipAppIds) {
    const GURL url = GURL(base::StrCat(
        {"chrome-extension://", app_id, "/_generated_background_page.html"}));
    web_contents->GetController().GetLastCommittedEntry()->SetURL(GURL(url));

    // Verify that the url is indeed changed.
    EXPECT_EQ(web_contents->GetURL().host(), app_id);

    // Access video.
    auto stop_capture_callback = StartCapture(
        web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

    // Verify no app is tracked.
    EXPECT_EQ(media_listener->state().window_capture_count, 0);

    // Verify that VideoConferenceWebApp is not created for web_contents.
    EXPECT_EQ(
        content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
            web_contents),
        nullptr);
  }
}

// These tests call methods on `VideoConferenceManagerAsh` that are not part of
// the crosapi interface. As a result these tests are run on ash-chrome only.
// TODO(b/274368285): Add lacros support for these tests.
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests request-on-mute functionality appropriately updates tray controller.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest, RequestOnMute) {
  ash::FakeVideoConferenceTrayController* controller =
      static_cast<ash::FakeVideoConferenceTrayController*>(
          ash::VideoConferenceTrayController::Get());
  ASSERT_TRUE(controller);

  auto* vc_manager = crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->video_conference_manager_ash();
  ASSERT_TRUE(vc_manager);

  auto* vc_app1 = CreateVcWebAppInNewTab();
  auto* vc_app2 = CreateVcWebAppInNewTab();

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kCamera, /*disabled=*/true);

  // Initially should be zero.
  EXPECT_EQ(controller->device_used_while_disabled_records().size(), 0u);

  // Start capture (and store callback in variable to prevent destructor from
  // stopping capture).
  auto stop_capture_callback1 =
      StartCapture(&vc_app1->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_EQ(controller->device_used_while_disabled_records().size(), 1u);

  vc_manager->SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
      /*disabled=*/true);
  auto stop_capture_callback2 =
      StartCapture(&vc_app2->GetWebContents(),
                   blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_EQ(controller->device_used_while_disabled_records().size(), 2u);
}

// Tests that a VC webapp corresponding to an extension is removed from the
// client when capturing stops.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       ExtensionRemovedWhenCapturingStopped) {
  auto* vc_manager = crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->video_conference_manager_ash();
  ASSERT_TRUE(vc_manager);

  std::unique_ptr<FakeVideoConferenceMediaListener> media_listener =
      std::make_unique<FakeVideoConferenceMediaListener>();

  EXPECT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);

  // Start capturing camera (this should create a VCWebApp on the VC client).
  auto stop_capture_callback = StartCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  // Get that VCWebApp.
  auto* vc_app =
      content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
          web_contents);
  ASSERT_TRUE(vc_app);

  // Make `vc_app` an extension.
  vc_app->state().is_extension = true;

  vc_manager->GetMediaApps(base::BindLambdaForTesting([](ash::MediaApps apps) {
    EXPECT_EQ(apps.size(), 1u);
    EXPECT_TRUE(apps[0]->is_capturing_camera);
    EXPECT_FALSE(apps[0]->is_capturing_microphone);
    EXPECT_FALSE(apps[0]->is_capturing_screen);
  }));

  std::move(stop_capture_callback).Run();

  // The VC app for the extension should be destroyed and removed from client.
  vc_manager->GetMediaApps(base::BindLambdaForTesting(
      [](ash::MediaApps apps) { EXPECT_EQ(apps.size(), 0u); }));

  // The VCWebApp associated with this webcontents should have been destroyed.
  EXPECT_FALSE(
      content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
          web_contents));
}
#endif

}  // namespace video_conference
