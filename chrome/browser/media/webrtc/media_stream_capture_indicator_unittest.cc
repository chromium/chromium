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

  // Test a MediaStreamCaptureIndicator accessor and ensure that the
  // corresponding observer method gets called.
  void TestObserverMethod(
      blink::mojom::MediaStreamType stream_type,
      base::Optional<media::mojom::DisplayMediaInformationPtr>
          display_media_info,
      MockObserverSetExpectationsMethod observer_method,
      AccessorMethod accessor_method);
  void TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType stream_type,
      base::Optional<media::mojom::DisplayMediaInformationPtr>
          display_media_info,
      MockObserverSetExpectationsMethod observer_method,
      AccessorMethod accessor_method);

 private:
  std::unique_ptr<MockObserver> observer_;
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  blink::PortalToken portal_token_;
};

}  // namespace

void MediaStreamCaptureIndicatorTest::TestObserverMethod(
    blink::mojom::MediaStreamType stream_type,
    base::Optional<media::mojom::DisplayMediaInformationPtr> display_media_info,
    MockObserverSetExpectationsMethod observer_set_expectations_method,
    AccessorMethod accessor_method) {
  // Create the fake stream device.
  blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");
  device.display_media_info = std::move(display_media_info);

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*accessor_method)(web_contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(web_contents(), {device});

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*observer_set_expectations_method)(web_contents(), true);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
  EXPECT_TRUE((indicator()->*accessor_method)(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());

  // Removing the stream device should cause the observer to be notified that
  // the observed property is now set to false.
  (observer()->*observer_set_expectations_method)(web_contents(), false);
  ui.reset();
  EXPECT_FALSE((indicator()->*accessor_method)(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());
}

void MediaStreamCaptureIndicatorTest::TestObserverMethodWithPortal(
    blink::mojom::MediaStreamType stream_type,
    base::Optional<media::mojom::DisplayMediaInformationPtr> display_media_info,
    MockObserverSetExpectationsMethod observer_set_expectations_method,
    AccessorMethod accessor_method) {
  // Create the fake stream device.
  blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");
  device.display_media_info = std::move(display_media_info);

  // By default all accessors should return false as there's no stream device.
  EXPECT_FALSE((indicator()->*accessor_method)(web_contents()));
  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(portal_contents(), {device});

  // Make sure that the observer gets called and that the corresponding accessor
  // gets called when |OnStarted| is called.
  (observer()->*observer_set_expectations_method)(portal_contents(), true);
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback());
  EXPECT_TRUE((indicator()->*accessor_method)(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());

  // Removing the stream device should cause the observer to be notified that
  // the observed property is now set to false.
  (observer()->*observer_set_expectations_method)(portal_contents(), false);
  ui.reset();
  EXPECT_FALSE((indicator()->*accessor_method)(web_contents()));
  ::testing::Mock::VerifyAndClear(observer());
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingVideo) {
  TestObserverMethod(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                     /*display_media_info=*/base::nullopt,
                     &MockObserver::SetOnIsCapturingVideoChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingVideo);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingVideo_Portal) {
  TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &MockObserver::SetOnIsCapturingVideoChangedExpectation,
      &MediaStreamCaptureIndicator::IsCapturingVideo);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingAudio) {
  TestObserverMethod(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     /*display_media_info=*/base::nullopt,
                     &MockObserver::SetOnIsCapturingAudioChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingAudio);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingAudio_Portal) {
  TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &MockObserver::SetOnIsCapturingAudioChangedExpectation,
      &MediaStreamCaptureIndicator::IsCapturingAudio);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsBeingMirrored) {
  TestObserverMethod(blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
                     /*display_media_info=*/base::nullopt,
                     &MockObserver::SetOnIsBeingMirroredChangedExpectation,
                     &MediaStreamCaptureIndicator::IsBeingMirrored);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsBeingMirrored_Portal) {
  TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &MockObserver::SetOnIsBeingMirroredChangedExpectation,
      &MediaStreamCaptureIndicator::IsBeingMirrored);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingWindow) {
  TestObserverMethod(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
                     /*display_media_info=*/base::nullopt,
                     &MockObserver::SetOnIsCapturingWindowChangedExpectation,
                     &MediaStreamCaptureIndicator::IsCapturingWindow);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingWindow_Portal) {
  TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &MockObserver::SetOnIsCapturingWindowChangedExpectation,
      &MediaStreamCaptureIndicator::IsCapturingWindow);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingDisplay) {
  TestObserverMethod(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::MONITOR,
          /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER),
      &MockObserver::SetOnIsCapturingDisplayChangedExpectation,
      &MediaStreamCaptureIndicator::IsCapturingDisplay);
}

TEST_F(MediaStreamCaptureIndicatorTest, IsCapturingDisplay_Portal) {
  TestObserverMethodWithPortal(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::MONITOR,
          /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER),
      &MockObserver::SetOnIsCapturingDisplayChangedExpectation,
      &MediaStreamCaptureIndicator::IsCapturingDisplay);
}
