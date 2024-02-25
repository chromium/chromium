// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
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

blink::MediaStreamDevices CreateFakeDevices(
    blink::mojom::MediaStreamType type) {
  blink::MediaStreamDevices devices;
  devices.reserve(3);
  for (size_t i = 0; i < devices.capacity(); ++i) {
    devices.emplace_back(type, "id_" + base::NumberToString(i),
                         "name " + base::NumberToString(i));
  }
  return devices;
}

std::vector<std::string> GetIds(const blink::MediaStreamDevices& devices,
                                size_t start_index) {
  CHECK_LT(start_index, devices.size());
  std::vector<std::string> device_ids;
  for (auto it = devices.begin() + start_index; it != devices.end(); ++it) {
    device_ids.push_back(it->id);
  }
  return device_ids;
}

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

  void UpdateAudioDevicePreferenceRanking(
      const typename blink::MediaStreamDevices::const_iterator
          preferred_device_iter) {
    CHECK(profile()->GetPrefs());
    media_prefs::UpdateAudioDevicePreferenceRanking(
        *profile()->GetPrefs(), preferred_device_iter,
        dispatcher_->GetAudioCaptureDevices());
  }

  void UpdateVideoDevicePreferenceRanking(
      const typename blink::MediaStreamDevices::const_iterator
          preferred_device_iter) {
    CHECK(profile()->GetPrefs());
    media_prefs::UpdateVideoDevicePreferenceRanking(
        *profile()->GetPrefs(), preferred_device_iter,
        dispatcher_->GetVideoCaptureDevices());
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

TEST_F(MediaCaptureDevicesDispatcherTest,
       GetPreferredAudioDeviceForBrowserContext) {
  const auto kFakeAudioDevices =
      CreateFakeDevices(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  dispatcher_->SetTestAudioCaptureDevices(kFakeAudioDevices);
  UpdateAudioDevicePreferenceRanking(
      dispatcher_->GetAudioCaptureDevices().end() - 1);
  UpdateAudioDevicePreferenceRanking(
      dispatcher_->GetAudioCaptureDevices().begin());
  // Ranking at this point is [device_0, device_2, device_1].

  // Exclude the first device from the eligible list to exercise filtering.
  const auto eligible_device_ids = GetIds(kFakeAudioDevices, 1);
  const auto preferred_device =
      dispatcher_->GetPreferredAudioDeviceForBrowserContext(
          browser_context(), eligible_device_ids);
  ASSERT_TRUE(preferred_device->IsSameDevice(kFakeAudioDevices.back()));
}

TEST_F(
    MediaCaptureDevicesDispatcherTest,
    GetPreferredAudioDeviceForBrowserContext_EmptyEligibleDevicesReturnsDefault) {
  const auto kFakeAudioDevices =
      CreateFakeDevices(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  dispatcher_->SetTestAudioCaptureDevices(kFakeAudioDevices);
  UpdateAudioDevicePreferenceRanking(
      dispatcher_->GetAudioCaptureDevices().end() - 1);
  UpdateAudioDevicePreferenceRanking(
      dispatcher_->GetAudioCaptureDevices().begin());
  // Ranking at this point is [device_0, device_2, device_1].

  // Pass empty eligible devices to disable filtering.
  const auto preferred_device =
      dispatcher_->GetPreferredAudioDeviceForBrowserContext(browser_context(),
                                                            {});
  // Preferred device should be the most preferred device in the ranking without
  // any filtering. That device is device_0.
  ASSERT_TRUE(preferred_device->IsSameDevice(kFakeAudioDevices.front()));
}

TEST_F(MediaCaptureDevicesDispatcherTest,
       GetPreferredVideoDeviceForBrowserContext) {
  const auto kFakeVideoDevices =
      CreateFakeDevices(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  dispatcher_->SetTestVideoCaptureDevices(kFakeVideoDevices);
  UpdateVideoDevicePreferenceRanking(
      dispatcher_->GetVideoCaptureDevices().end() - 1);
  UpdateVideoDevicePreferenceRanking(
      dispatcher_->GetVideoCaptureDevices().begin());
  // Ranking at this point is [device_0, device_2, device_1].

  // Exclude the first device from the eligible list to exercise filtering.
  const auto eligible_device_ids = GetIds(kFakeVideoDevices, 1);
  const auto preferred_device =
      dispatcher_->GetPreferredVideoDeviceForBrowserContext(
          browser_context(), eligible_device_ids);
  ASSERT_TRUE(preferred_device->IsSameDevice(kFakeVideoDevices.back()));
}

TEST_F(
    MediaCaptureDevicesDispatcherTest,
    GetPreferredVideoDeviceForBrowserContext_EmptyEligibleDevicesReturnsDefault) {
  const auto kFakeVideoDevices =
      CreateFakeDevices(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  dispatcher_->SetTestVideoCaptureDevices(kFakeVideoDevices);
  UpdateVideoDevicePreferenceRanking(
      dispatcher_->GetVideoCaptureDevices().end() - 1);
  UpdateVideoDevicePreferenceRanking(
      dispatcher_->GetVideoCaptureDevices().begin());
  // Ranking at this point is [device_0, device_2, device_1].

  // Exclude the first device from the eligible list to exercise filtering.
  const auto preferred_device =
      dispatcher_->GetPreferredVideoDeviceForBrowserContext(browser_context(),
                                                            {});
  ASSERT_TRUE(preferred_device->IsSameDevice(kFakeVideoDevices.front()));
}
