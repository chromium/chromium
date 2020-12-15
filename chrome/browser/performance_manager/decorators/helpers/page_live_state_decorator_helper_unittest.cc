// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"

#include "base/callback_helpers.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageLiveStateDecoratorHelperTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PageLiveStateDecoratorHelperTest() = default;
  ~PageLiveStateDecoratorHelperTest() override = default;
  PageLiveStateDecoratorHelperTest(
      const PageLiveStateDecoratorHelperTest& other) = delete;
  PageLiveStateDecoratorHelperTest& operator=(
      const PageLiveStateDecoratorHelperTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    helper_ = std::make_unique<PageLiveStateDecoratorHelper>();
    indicator_ = MediaCaptureDevicesDispatcher::GetInstance()
                     ->GetMediaStreamCaptureIndicator();
    auto contents = CreateTestWebContents();
    SetContents(std::move(contents));
  }

  void TearDown() override {
    DeleteContents();
    helper_.reset();
    indicator_.reset();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MediaStreamCaptureIndicator* indicator() { return indicator_.get(); }

  void EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType stream_type,
      base::Optional<media::mojom::DisplayMediaInformationPtr>
          display_media_info,
      bool (PageLiveStateDecorator::Data::*pm_getter)() const);

  // Forces deletion of the PageLiveStateDecoratorHelper.
  void ResetHelper() { helper_.reset(); }

 private:
  PerformanceManagerTestHarnessHelper pm_harness_;
  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  std::unique_ptr<PageLiveStateDecoratorHelper> helper_;
};

void PageLiveStateDecoratorHelperTest::EndToEndStreamPropertyTest(
    blink::mojom::MediaStreamType stream_type,
    base::Optional<media::mojom::DisplayMediaInformationPtr> display_media_info,
    bool (PageLiveStateDecorator::Data::*pm_getter)() const) {
  // By default all properties are set to false.
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      pm_getter, false);

  // Create the fake stream device and start it, this should set the property to
  // true.
  blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");
  device.display_media_info = std::move(display_media_info);

  std::unique_ptr<content::MediaStreamUI> ui =
      indicator()->RegisterMediaStream(web_contents(), {device});
  ui->OnStarted(base::OnceClosure(), content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      pm_getter, true);

  // Switch back to the default state.
  ui.reset();
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      pm_getter, false);
}

}  // namespace

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingVideoChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &PageLiveStateDecorator::Data::IsCapturingVideo);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingAudioChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &PageLiveStateDecorator::Data::IsCapturingAudio);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsBeingMirroredChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &PageLiveStateDecorator::Data::IsBeingMirrored);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingWindowChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      /*display_media_info=*/base::nullopt,
      &PageLiveStateDecorator::Data::IsCapturingWindow);
}

TEST_F(PageLiveStateDecoratorHelperTest, OnIsCapturingDisplayChanged) {
  EndToEndStreamPropertyTest(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::MONITOR,
          /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER),
      &PageLiveStateDecorator::Data::IsCapturingDisplay);
}

TEST_F(PageLiveStateDecoratorHelperTest, IsConnectedToBluetoothDevice) {
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, false);
  content::WebContentsTester::For(web_contents())
      ->TestIncrementBluetoothConnectedDeviceCount();
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, true);
  content::WebContentsTester::For(web_contents())
      ->TestDecrementBluetoothConnectedDeviceCount();
  testing::TestPageNodePropertyOnPMSequence(
      web_contents(), &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice, false);
}

// Create many WebContents to exercice the code that maintains the linked list
// of PageLiveStateDecoratorHelper::WebContentsObservers.
TEST_F(PageLiveStateDecoratorHelperTest, ManyPageNodes) {
  std::unique_ptr<content::WebContents> c1 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> c2 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> c3 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> c4 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> c5 = CreateTestWebContents();

  // Expect no crash when WebContentsObservers are destroyed.

  // This deletes WebContentsObservers associated with |c1|, |c3| and |c5|.
  c1.reset();
  c3.reset();
  c5.reset();

  // This deletes remaining WebContentsObservers.
  ResetHelper();
}

}  // namespace performance_manager
