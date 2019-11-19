// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

class DesktopCaptureAccessHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  DesktopCaptureAccessHandlerTest() {}
  ~DesktopCaptureAccessHandlerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto picker_factory = std::make_unique<FakeDesktopMediaPickerFactory>();
    picker_factory_ = picker_factory.get();
    access_handler_ = std::make_unique<DesktopCaptureAccessHandler>(
        std::move(picker_factory));
  }

  void ProcessRequest(
      const content::DesktopMediaID& fake_desktop_media_id_response,
      blink::mojom::MediaStreamRequestResult* request_result,
      blink::MediaStreamDevices* devices_result,
      blink::MediaStreamRequestType request_type,
      bool request_audio) {
    FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
        {false /* expect_screens */, false /* expect_windows*/,
         true /* expect_tabs */, request_audio /* expect_audio */,
         fake_desktop_media_id_response /* selected_source */}};
    picker_factory_->SetTestFlags(test_flags, base::size(test_flags));
    blink::mojom::MediaStreamType audio_type =
        request_audio ? blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE
                      : blink::mojom::MediaStreamType::NO_SERVICE;
    content::MediaStreamRequest request(
        0, 0, 0, GURL("http://origin/"), false, request_type, std::string(),
        std::string(), audio_type,
        blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, false);

    base::RunLoop wait_loop;
    content::MediaResponseCallback callback = base::BindOnce(
        [](base::RunLoop* wait_loop,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::MediaStreamDevices* devices_result,
           const blink::MediaStreamDevices& devices,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          *request_result = result;
          *devices_result = devices;
          wait_loop->Quit();
        },
        &wait_loop, request_result, devices_result);
    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   nullptr /* extension */);
    wait_loop.Run();
    EXPECT_TRUE(test_flags[0].picker_created);

    access_handler_.reset();
    EXPECT_TRUE(test_flags[0].picker_deleted);
  }

  void NotifyWebContentsDestroyed() {
    access_handler_->Observe(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(web_contents()),
        content::NotificationDetails());
  }

  const DesktopCaptureAccessHandler::RequestsQueues& GetRequestQueues() {
    return access_handler_->pending_requests_;
  }

 protected:
  FakeDesktopMediaPickerFactory* picker_factory_;
  std::unique_ptr<DesktopCaptureAccessHandler> access_handler_;
};

TEST_F(DesktopCaptureAccessHandlerTest,
       ChangeSourceWithoutAudioRequestPermissionGiven) {
  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  ProcessRequest(content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId),
                 &result, &devices, blink::MEDIA_DEVICE_UPDATE,
                 false /*request_audio*/);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
            devices[0].type);
}

TEST_F(DesktopCaptureAccessHandlerTest,
       ChangeSourceWithAudioRequestPermissionGiven) {
  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  ProcessRequest(content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId,
                                         true /* audio_share */),
                 &result, &devices, blink::MEDIA_DEVICE_UPDATE,
                 true /* request_audio */);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(2u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
            devices[0].type);
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
            devices[1].type);
}

TEST_F(DesktopCaptureAccessHandlerTest, ChangeSourcePermissionDenied) {
  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  ProcessRequest(content::DesktopMediaID(), &result, &devices,
                 blink::MEDIA_DEVICE_UPDATE, false /*request audio*/);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_EQ(0u, devices.size());
}

TEST_F(DesktopCaptureAccessHandlerTest,
       ChangeSourceUpdateMediaRequestStateWithClosing) {
  const int render_process_id = 0;
  const int render_frame_id = 0;
  const int page_request_id = 0;
  const blink::mojom::MediaStreamType stream_type =
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
      {false /* expect_screens */, false /* expect_windows*/,
       true /* expect_tabs */, false /* expect_audio */,
       content::DesktopMediaID(), true /* cancelled */}};
  picker_factory_->SetTestFlags(test_flags, base::size(test_flags));
  content::MediaStreamRequest request(
      render_process_id, render_frame_id, page_request_id,
      GURL("http://origin/"), false, blink::MEDIA_DEVICE_UPDATE, std::string(),
      std::string(), blink::mojom::MediaStreamType::NO_SERVICE, stream_type,
      false);
  content::MediaResponseCallback callback;
  access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                 nullptr /* extension */);
  EXPECT_TRUE(test_flags[0].picker_created);
  EXPECT_EQ(1u, GetRequestQueues().size());
  auto queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(1u, queue_it->second.size());

  access_handler_->UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, stream_type,
      content::MEDIA_REQUEST_STATE_CLOSING);
  EXPECT_EQ(1u, GetRequestQueues().size());
  queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(0u, queue_it->second.size());
  EXPECT_TRUE(test_flags[0].picker_deleted);
  access_handler_.reset();
}

TEST_F(DesktopCaptureAccessHandlerTest, ChangeSourceWebContentsDestroyed) {
  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
      {false /* expect_screens */, false /* expect_windows*/,
       true /* expect_tabs */, false /* expect_audio */,
       content::DesktopMediaID(), true /* cancelled */}};
  picker_factory_->SetTestFlags(test_flags, base::size(test_flags));
  content::MediaStreamRequest request(
      0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_DEVICE_UPDATE,
      std::string(), std::string(), blink::mojom::MediaStreamType::NO_SERVICE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, false);
  content::MediaResponseCallback callback;
  access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                 nullptr /* extension */);
  EXPECT_TRUE(test_flags[0].picker_created);
  EXPECT_EQ(1u, GetRequestQueues().size());
  auto queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(1u, queue_it->second.size());

  NotifyWebContentsDestroyed();
  EXPECT_EQ(0u, GetRequestQueues().size());
  access_handler_.reset();
}

TEST_F(DesktopCaptureAccessHandlerTest, ChangeSourceMultipleRequests) {
  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
      {false /* expect_screens */, false /* expect_windows*/,
       true /* expect_tabs */, false /* expect_audio */,
       content::DesktopMediaID(
           content::DesktopMediaID::TYPE_SCREEN,
           content::DesktopMediaID::kFakeId) /* selected_source */},
      {false /* expect_screens */, false /* expect_windows*/,
       true /* expect_tabs */, false /* expect_audio */,
       content::DesktopMediaID(
           content::DesktopMediaID::TYPE_WINDOW,
           content::DesktopMediaID::kNullId) /* selected_source */}};
  const size_t kTestFlagCount = 2;
  picker_factory_->SetTestFlags(test_flags, kTestFlagCount);

  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  base::RunLoop wait_loop[kTestFlagCount];
  for (size_t i = 0; i < kTestFlagCount; ++i) {
    content::MediaStreamRequest request(
        0, 0, 0, GURL("http://origin/"), false, blink::MEDIA_DEVICE_UPDATE,
        std::string(), std::string(), blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, false);
    content::MediaResponseCallback callback = base::BindOnce(
        [](base::RunLoop* wait_loop,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::MediaStreamDevices* devices_result,
           const blink::MediaStreamDevices& devices,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          *request_result = result;
          *devices_result = devices;
          wait_loop->Quit();
        },
        &wait_loop[i], &result, &devices);
    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   nullptr /* extension */);
  }
  wait_loop[0].Run();
  EXPECT_TRUE(test_flags[0].picker_created);
  EXPECT_TRUE(test_flags[0].picker_deleted);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
            devices[0].type);

  blink::MediaStreamDevice first_device = devices[0];
  EXPECT_TRUE(test_flags[1].picker_created);
  EXPECT_FALSE(test_flags[1].picker_deleted);
  wait_loop[1].Run();
  EXPECT_TRUE(test_flags[1].picker_deleted);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
            devices[0].type);
  EXPECT_FALSE(devices[0].IsSameDevice(first_device));

  access_handler_.reset();
}
