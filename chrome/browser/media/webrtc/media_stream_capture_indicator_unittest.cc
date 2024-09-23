// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace {

class LenientMockObserver : public MediaStreamCaptureIndicator::Observer {
 public:
  LenientMockObserver() = default;

  LenientMockObserver(const LenientMockObserver&) = delete;
  LenientMockObserver& operator=(const LenientMockObserver&) = delete;

  ~LenientMockObserver() override {}

  // Helper functions used to set the expectations of the mock methods. This
  // allows passing function pointers to
  // MediaStreamCaptureIndicatorTest::TestObserverMethod.

  void SetOnIsCapturingVideoChangedExpectation(content::WebContents* contents,
                                               bool is_capturing_video) {
    EXPECT_CALL(*this, OnIsCapturingVideoChanged(contents, is_capturing_video));
  }

  void SetOnIsCapturingAudioChangedExpectation(content::WebContents* contents,
                                               bool is_capturing_audio) {
    EXPECT_CALL(*this, OnIsCapturingAudioChanged(contents, is_capturing_audio));
  }

  void SetOnIsBeingMirroredChangedExpectation(content::WebContents* contents,
                                              bool is_being_mirrored) {
    EXPECT_CALL(*this, OnIsBeingMirroredChanged(contents, is_being_mirrored));
  }

  void SetOnIsCapturingWindowChangedExpectation(content::WebContents* contents,
                                                size_t times) {
    EXPECT_CALL(*this, OnIsCapturingWindowChanged(contents, testing::_))
        .Times(times);
  }

  void SetOnIsCapturingWindowChangedExpectation(content::WebContents* contents,
                                                bool is_capturing_window) {
    EXPECT_CALL(*this,
                OnIsCapturingWindowChanged(contents, is_capturing_window));
  }

  void SetOnIsCapturingDisplayChangedExpectation(content::WebContents* contents,
                                                 size_t times) {
    EXPECT_CALL(*this, OnIsCapturingDisplayChanged(contents, testing::_))
        .Times(times);
  }

  void SetOnIsCapturingDisplayChangedExpectation(content::WebContents* contents,
                                                 bool is_capturing_display) {
    EXPECT_CALL(*this,
                OnIsCapturingDisplayChanged(contents, is_capturing_display));
  }

 private:
  MOCK_METHOD2(OnIsCapturingVideoChanged,
               void(content::WebContents* contents, bool is_capturing_video));
  MOCK_METHOD2(OnIsCapturingAudioChanged,
               void(content::WebContents* contents, bool is_capturing_audio));
  MOCK_METHOD2(OnIsBeingMirroredChanged,
               void(content::WebContents* contents, bool is_being_mirrored));
  MOCK_METHOD2(OnIsCapturingWindowChanged,
               void(content::WebContents* contents, bool is_capturing_window));
  MOCK_METHOD2(OnIsCapturingDisplayChanged,
               void(content::WebContents* contents, bool is_capturing_display));
};
using MockObserver = testing::StrictMock<LenientMockObserver>;

typedef void (MockObserver::*MockObserverSetExpectationsMethod)(
    content::WebContents* web_contents,
    bool value);
typedef void (MockObserver::*MockObserverStreamTypeSetExpectationsMethod)(
    content::WebContents* web_contents,
    size_t value);
typedef bool (MediaStreamCaptureIndicator::*AccessorMethod)(
    content::WebContents* web_contents) const;

class MediaStreamCaptureIndicatorTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaStreamCaptureIndicatorTest() {}
  ~MediaStreamCaptureIndicatorTest() override {}
  MediaStreamCaptureIndicatorTest(const MediaStreamCaptureIndicatorTest&) =
      delete;
  MediaStreamCaptureIndicatorTest& operator=(
      const MediaStreamCaptureIndicatorTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://www.example.com/"));
    indicator_ = MediaCaptureDevicesDispatcher::GetInstance()
                     ->GetMediaStreamCaptureIndicator();
    observer_ = std::make_unique<MockObserver>();
    indicator_->AddObserver(observer());
  }

  void TearDown() override {
    indicator_->RemoveObserver(observer());
    observer_.reset();
    indicator_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MediaStreamCaptureIndicator* indicator() { return indicator_.get(); }
  MockObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<MockObserver> observer_;
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
};

struct ObserverMethodTestParam {
  ObserverMethodTestParam(
      blink::mojom::MediaStreamType stream_type,
      media::mojom::DisplayMediaInformationPtr display_media_info,
      MockObserverSetExpectationsMethod observer_method,
      AccessorMethod accessor_method)
      : stream_type(stream_type),
        display_media_info(display_media_info.Clone()),
        observer_method(observer_method),
        accessor_method(accessor_method) {}

  ObserverMethodTestParam(const ObserverMethodTestParam& other)
      : stream_type(other.stream_type),
        display_media_info(other.display_media_info.Clone()),
        observer_method(other.observer_method),
        accessor_method(other.accessor_method) {}

  blink::mojom::MediaStreamType stream_type;
  media::mojom::DisplayMediaInformationPtr display_media_info;
  MockObserverSetExpectationsMethod observer_method;
  AccessorMethod accessor_method;
};

ObserverMethodTestParam kObserverMethodTestParams[] = {
    {blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
     /*display_media_info=*/nullptr,
     &MockObserver::SetOnIsCapturingVideoChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingVideo},
    {blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
     /*display_media_info=*/nullptr,
     &MockObserver::SetOnIsCapturingAudioChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingAudio},
    {blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
     /*display_media_info=*/nullptr,
     &MockObserver::SetOnIsBeingMirroredChangedExpectation,
     &MediaStreamCaptureIndicator::IsBeingMirrored},
    {blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
     /*display_media_info=*/nullptr,
     &MockObserver::SetOnIsCapturingWindowChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingWindow},
    {blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
     media::mojom::DisplayMediaInformation::New(
         media::mojom::DisplayCaptureSurfaceType::MONITOR,
         /*logical_surface=*/true,
         media::mojom::CursorCaptureType::NEVER,
         /*capture_handle=*/nullptr,
         /*initial_zoom_level=*/100),
     &MockObserver::SetOnIsCapturingDisplayChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingDisplay},
};

class MediaStreamCaptureIndicatorObserverMethodTest
    : public MediaStreamCaptureIndicatorTest,
      public testing::WithParamInterface<ObserverMethodTestParam> {};

blink::mojom::StreamDevices CreateFakeDevice(
    const ObserverMethodTestParam& param) {
  blink::mojom::StreamDevices fake_devices;
  blink::MediaStreamDevice device(param.stream_type, "fake_device",
                                  "fake_device");
  if (param.display_media_info)
    device.display_media_info = param.display_media_info->Clone();

  if (blink::IsAudioInputMediaType(param.stream_type))
    fake_devices.audio_device = device;
  else if (blink::IsVideoInputMediaType(param.stream_type))
    fake_devices.video_device = device;
  else
    NOTREACHED_IN_MIGRATION();

  return fake_devices;
}

struct StreamTypeTestParam {
  StreamTypeTestParam(
      blink::mojom::MediaStreamType video_stream_type,
      content::DesktopMediaID::Type media_type,
      MockObserverStreamTypeSetExpectationsMethod observer_method)
      : video_stream_type(video_stream_type),
        media_type(media_type),
        observer_method(observer_method) {}

  blink::mojom::MediaStreamType video_stream_type;
  content::DesktopMediaID::Type media_type;
  MockObserverStreamTypeSetExpectationsMethod observer_method;
};

StreamTypeTestParam kStreamTypeTestParams[] = {
    {blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
     content::DesktopMediaID::Type::TYPE_SCREEN,
     &MockObserver::SetOnIsCapturingDisplayChangedExpectation},
    {blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
     content::DesktopMediaID::Type::TYPE_SCREEN,
     &MockObserver::SetOnIsCapturingDisplayChangedExpectation},
    {blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
     content::DesktopMediaID::Type::TYPE_WINDOW,
     &MockObserver::SetOnIsCapturingWindowChangedExpectation},
    {blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
     content::DesktopMediaID::Type::TYPE_WINDOW,
     &MockObserver::SetOnIsCapturingWindowChangedExpectation},
};

class MediaStreamCaptureIndicatorStreamTypeTest
    : public MediaStreamCaptureIndicatorTest,
      public testing::WithParamInterface<StreamTypeTestParam> {};

}  // namespace

TEST_P(MediaStreamCaptureIndicatorObserverMethodTest, AddAndRemoveDevice) {
  const ObserverMethodTestParam& param = GetParam();
  content::WebContents* source = web_contents();

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*(param.accessor_method))(web_contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(source, CreateFakeDevice(param));

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*(param.observer_method))(source, true);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  EXPECT_TRUE((indicator()->*(param.accessor_method))(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());

  // Removing the stream device should cause the observer to be notified that
  // the observed property is now set to false.
  (observer()->*(param.observer_method))(source, false);
  ui.reset();
  EXPECT_FALSE((indicator()->*(param.accessor_method))(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());
}

// TODO(crbug.com/40071631): re-enable once the bug is fixed.
TEST_P(MediaStreamCaptureIndicatorObserverMethodTest,
       DISABLED_StopMediaCapturing) {
  const ObserverMethodTestParam& param = GetParam();
  const auto media_tpy =
      MediaStreamCaptureIndicator::GetMediaType(param.stream_type);
  content::WebContents* source = web_contents();

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*(param.accessor_method))(web_contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(source, CreateFakeDevice(param));
  auto stop_callback = base::BindLambdaForTesting([&]() { ui.reset(); });

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*(param.observer_method))(source, true);
  ui->OnStarted(std::move(stop_callback),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  EXPECT_TRUE((indicator()->*(param.accessor_method))(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());

  // StopMediaCapturing calls the stop_callback which calls ui.reset; which will
  // notify the observer_method that the capturing is stopped.
  (observer()->*(param.observer_method))(source, false);
  indicator()->StopMediaCapturing(source, media_tpy);

  EXPECT_FALSE((indicator()->*(param.accessor_method))(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());
}

TEST_P(MediaStreamCaptureIndicatorObserverMethodTest, CloseActiveWebContents) {
  const ObserverMethodTestParam& param = GetParam();
  content::WebContents* source = web_contents();

  // Create and start the fake stream device.
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(source, CreateFakeDevice(param));
  (observer()->*(param.observer_method))(source, true);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  ::testing::Mock::VerifyAndClear(observer());

  // Deleting the WebContents should cause the observer to be notified that the
  // observed property is now set to false.
  (observer()->*(param.observer_method))(source, false);
  DeleteContents();
  ::testing::Mock::VerifyAndClear(observer());
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaStreamCaptureIndicatorObserverMethodTest,
                         testing::ValuesIn(kObserverMethodTestParams));

TEST_P(MediaStreamCaptureIndicatorStreamTypeTest,
       CheckIsDeviceCapturingDisplay) {
  const blink::mojom::MediaStreamType& video_stream_type =
      GetParam().video_stream_type;
  const content::DesktopMediaID::Type& media_type = GetParam().media_type;

  content::WebContents* source = web_contents();
  blink::mojom::StreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui = GetDevicesForDesktopCapture(
      content::MediaStreamRequest(
          /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
          /*url_origin=*/url::Origin(),
          /*user_gesture=*/false,
          blink::MediaStreamRequestType::MEDIA_GENERATE_STREAM,
          /*requested_audio_device_ids=*/{},
          /*requested_video_device_ids=*/{"fake_device"},
          blink::mojom::MediaStreamType::NO_SERVICE, video_stream_type,
          /*disable_local_echo=*/false,
          /*request_pan_tilt_zoom_permission=*/false,
          /*captured_surface_control_active=*/false),
      source, content::DesktopMediaID(media_type, /*id=*/0),
      /*capture_audio=*/false, /*disable_local_echo=*/false,
      /*suppress_local_audio_playback=*/false,
      /*display_notification=*/false, /*application_title=*/u"",
      /*captured_surface_control_active=*/false, devices);
  ASSERT_EQ(devices.video_device->type, video_stream_type);

  (observer()->*(GetParam().observer_method))(source, 2);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaStreamCaptureIndicatorStreamTypeTest,
                         testing::ValuesIn(kStreamTypeTestParams));
