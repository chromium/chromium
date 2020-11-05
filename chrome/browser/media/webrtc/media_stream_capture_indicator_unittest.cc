// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

#include "base/bind.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace {

class LenientMockObserver : public MediaStreamCaptureIndicator::Observer {
 public:
  LenientMockObserver() = default;
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
                                                bool is_capturing_window) {
    EXPECT_CALL(*this,
                OnIsCapturingWindowChanged(contents, is_capturing_window));
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

  DISALLOW_COPY_AND_ASSIGN(LenientMockObserver);
};
using MockObserver = testing::StrictMock<LenientMockObserver>;

typedef void (MockObserver::*MockObserverSetExpectationsMethod)(
    content::WebContents* web_contents,
    bool value);
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
    portal_token_ = content::WebContentsTester::For(web_contents())
                        ->CreatePortal(CreateTestWebContents());
  }

  void TearDown() override {
    indicator_->RemoveObserver(observer());
    observer_.reset();
    indicator_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MediaStreamCaptureIndicator* indicator() { return indicator_.get(); }
  content::WebContents* portal_contents() {
    return content::WebContentsTester::For(web_contents())
        ->GetPortalContents(portal_token_);
  }
  MockObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<MockObserver> observer_;
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  blink::PortalToken portal_token_;
};

struct ObserverMethodTestParam {
  blink::mojom::MediaStreamType stream_type;
  base::Optional<media::mojom::DisplayMediaInformation> display_media_info;
  MockObserverSetExpectationsMethod observer_method;
  AccessorMethod accessor_method;
};

ObserverMethodTestParam kObserverMethodTestParams[] = {
    {blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
     /*display_media_info=*/base::nullopt,
     &MockObserver::SetOnIsCapturingVideoChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingVideo},
    {blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
     /*display_media_info=*/base::nullopt,
     &MockObserver::SetOnIsCapturingAudioChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingAudio},
    {blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
     /*display_media_info=*/base::nullopt,
     &MockObserver::SetOnIsBeingMirroredChangedExpectation,
     &MediaStreamCaptureIndicator::IsBeingMirrored},
    {blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
     /*display_media_info=*/base::nullopt,
     &MockObserver::SetOnIsCapturingWindowChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingWindow},
    {blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
     media::mojom::DisplayMediaInformation(
         media::mojom::DisplayCaptureSurfaceType::MONITOR,
         /*logical_surface=*/true,
         media::mojom::CursorCaptureType::NEVER),
     &MockObserver::SetOnIsCapturingDisplayChangedExpectation,
     &MediaStreamCaptureIndicator::IsCapturingDisplay},
};

class MediaStreamCaptureIndicatorObserverMethodTest
    : public MediaStreamCaptureIndicatorTest,
      public testing::WithParamInterface<
          std::tuple<ObserverMethodTestParam, bool>> {};

blink::MediaStreamDevice CreateFakeDevice(
    const ObserverMethodTestParam& param) {
  blink::MediaStreamDevice device(param.stream_type, "fake_device",
                                  "fake_device");
  if (param.display_media_info)
    device.display_media_info = param.display_media_info->Clone();

  return device;
}

}  // namespace

TEST_P(MediaStreamCaptureIndicatorObserverMethodTest, AddAndRemoveDevice) {
  const ObserverMethodTestParam& param = std::get<0>(GetParam());
  bool is_portal = std::get<1>(GetParam());
  content::WebContents* source = is_portal ? portal_contents() : web_contents();

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*(param.accessor_method))(web_contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(source, {CreateFakeDevice(param)});

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*(param.observer_method))(source, true);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback(),
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

TEST_P(MediaStreamCaptureIndicatorObserverMethodTest, CloseActiveWebContents) {
  const ObserverMethodTestParam& param = std::get<0>(GetParam());
  bool is_portal = std::get<1>(GetParam());
  content::WebContents* source = is_portal ? portal_contents() : web_contents();

  // Create and start the fake stream device.
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(source, {CreateFakeDevice(param)});
  (observer()->*(param.observer_method))(source, true);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  ::testing::Mock::VerifyAndClear(observer());

  // Deleting the WebContents should cause the observer to be notified that the
  // observed property is now set to false.
  (observer()->*(param.observer_method))(source, false);
  DeleteContents();
  ::testing::Mock::VerifyAndClear(observer());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaStreamCaptureIndicatorObserverMethodTest,
    testing::Combine(testing::ValuesIn(kObserverMethodTestParams),
                     testing::Bool()));
