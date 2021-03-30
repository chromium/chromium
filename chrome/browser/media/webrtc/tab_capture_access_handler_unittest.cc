// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_content_manager.h"
#endif

class TabCaptureAccessHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabCaptureAccessHandlerTest() = default;
  ~TabCaptureAccessHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    access_handler_ = std::make_unique<TabCaptureAccessHandler>();
  }

  void ProcessRequest(
      const content::DesktopMediaID& fake_desktop_media_id_response,
      blink::mojom::MediaStreamRequestResult* request_result,
      blink::MediaStreamDevices* devices_result) {
    content::MediaStreamRequest request(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), /*page_request_id=*/0,
        GURL("http://origin/"), /*user_gesture=*/false,
        blink::MEDIA_GENERATE_STREAM,
        /*requested_audio_device_id=*/std::string(),
        /*requested_video_device_id=*/std::string(),
        blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false);

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
                                   /*extension=*/nullptr);
    wait_loop.Run();

    access_handler_.reset();
  }

 protected:
  std::unique_ptr<TabCaptureAccessHandler> access_handler_;
};

TEST_F(TabCaptureAccessHandlerTest, PermissionGiven) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL("http://origin/"), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
            devices[0].type);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabCaptureAccessHandlerTest, DlpRestricted) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentManagerForTesting scoped_dlp_content_manager_(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, IsScreenCaptureRestricted(source))
      .Times(1)
      .WillOnce(testing::Return(true));

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL("http://origin/"), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_EQ(0u, devices.size());
}
#endif
