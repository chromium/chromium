// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace video_conference {

class FakeVcMediaListener : VideoConferenceMediaListener {
 public:
  struct State {
    int video_capture_count = 0;
    int audio_capture_count = 0;
    int window_capture_count = 0;
    int display_capture_count = 0;
  };

  FakeVcMediaListener()
      : VideoConferenceMediaListener(
            base::BindRepeating([]() {}),
            base::BindRepeating(
                [](content::WebContents* contents) -> VideoConferenceWebApp* {
                  // Should not be called.
                  EXPECT_TRUE(false);
                  return nullptr;
                })) {}

  FakeVcMediaListener(const FakeVcMediaListener&) = delete;
  FakeVcMediaListener& operator=(const FakeVcMediaListener&) = delete;

  ~FakeVcMediaListener() override = default;

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
        base::BindRepeating([](const base::UnguessableToken& id) {}));

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
      NOTREACHED();
    }

    return fake_devices;
  }

  int tab_count_{0};
};

// Tests video capturing is correctly detected by VideoConferenceMediaListener.
IN_PROC_BROWSER_TEST_F(VideoConferenceMediaListenerBrowserTest,
                       DeviceVideoCapturing) {
  std::unique_ptr<FakeVcMediaListener> media_listener =
      std::make_unique<FakeVcMediaListener>();

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
  std::unique_ptr<FakeVcMediaListener> media_listener =
      std::make_unique<FakeVcMediaListener>();

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
  std::unique_ptr<FakeVcMediaListener> media_listener =
      std::make_unique<FakeVcMediaListener>();

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

}  // namespace video_conference
