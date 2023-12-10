// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

using testing::_;

namespace {

class MockMediaAccessHandler : public MediaAccessHandler {
 public:
  explicit MockMediaAccessHandler(
      const blink::mojom::MediaStreamType supported_type)
      : supported_type_(supported_type) {}

  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType type,
                          const extensions::Extension* extension) override {
    return supported_type_ == type;
  }

  MOCK_METHOD4(CheckMediaAccessPermission,
               bool(content::RenderFrameHost* render_frame_host,
                    const url::Origin& security_origin,
                    blink::mojom::MediaStreamType type,
                    const extensions::Extension* extension));
  MOCK_METHOD4(HandleRequest,
               void(content::WebContents* web_contents,
                    const content::MediaStreamRequest& request,
                    content::MediaResponseCallback callback,
                    const extensions::Extension* extension));
  MOCK_METHOD4(UpdateVideoScreenCaptureStatus,
               void(int render_process_id,
                    int render_frame_id,
                    int page_request_id,
                    bool is_secure));

  const blink::mojom::MediaStreamType supported_type_;
};

}  // namespace

class MediaCaptureDevicesDispatcherTest
    : public ChromeRenderViewHostTestHarness {
 public:
  MediaCaptureDevicesDispatcherTest() = default;
  ~MediaCaptureDevicesDispatcherTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    dispatcher_ = MediaCaptureDevicesDispatcher::GetInstance();
  }

  std::vector<std::unique_ptr<MediaAccessHandler>>& media_access_handlers() {
    return dispatcher_->media_access_handlers_;
  }

  void UpdateVideoScreenCaptureStatus(
      const blink::mojom::MediaStreamType type) {
    dispatcher_->UpdateVideoScreenCaptureStatus(0, 0, 0, type, false);
  }

 protected:
  raw_ptr<MediaCaptureDevicesDispatcher> dispatcher_;
};

TEST_F(MediaCaptureDevicesDispatcherTest,
       DISABLED_LoopsAllMediaAccessHandlers) {
  media_access_handlers().clear();

  // Add two handlers.
  auto stream_type1 = blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  auto mock1 = std::make_unique<MockMediaAccessHandler>(stream_type1);
  MockMediaAccessHandler* handler1 = mock1.get();
  media_access_handlers().push_back(std::move(mock1));
  auto stream_type2 = blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  auto mock2 = std::make_unique<MockMediaAccessHandler>(stream_type2);
  MockMediaAccessHandler* handler2 = mock2.get();
  media_access_handlers().push_back(std::move(mock2));

  // Expect both to be called.
  EXPECT_CALL(*handler1, UpdateVideoScreenCaptureStatus(_, _, _, _));
  UpdateVideoScreenCaptureStatus(stream_type1);
  EXPECT_CALL(*handler2, UpdateVideoScreenCaptureStatus(_, _, _, _));
  UpdateVideoScreenCaptureStatus(stream_type2);
}
