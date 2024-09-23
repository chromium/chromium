// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
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
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kOrigin[] = "https://origin/";
constexpr blink::mojom::MediaStreamRequestResult kInvalidResult =
    blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
}  // namespace

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
      blink::mojom::StreamDevices* devices_result,
      bool expect_result = true) {
    content::MediaStreamRequest request(
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
        web_contents()->GetPrimaryMainFrame()->GetRoutingID(),
        /*page_request_id=*/0, url::Origin::Create(GURL(kOrigin)),
        /*user_gesture=*/false, blink::MEDIA_GENERATE_STREAM,
        /*requested_audio_device_ids=*/{},
        /*requested_video_device_ids=*/{},
        blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*captured_surface_control_active=*/false);

    base::RunLoop wait_loop;
    content::MediaResponseCallback callback = base::BindOnce(
        [](base::RunLoop* wait_loop, bool expect_result,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::mojom::StreamDevices* devices_result,
           const blink::mojom::StreamDevicesSet& stream_devices_set,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          DCHECK(!devices_result->audio_device);
          DCHECK(!devices_result->video_device);
          *request_result = result;
          if (result == blink::mojom::MediaStreamRequestResult::OK) {
            ASSERT_EQ(stream_devices_set.stream_devices.size(), 1u);
            *devices_result = *stream_devices_set.stream_devices[0];
          } else {
            ASSERT_TRUE(stream_devices_set.stream_devices.empty());
            *devices_result = blink::mojom::StreamDevices();
          }
          if (!expect_result) {
            FAIL() << "MediaResponseCallback should not be called.";
          }
          wait_loop->Quit();
        },
        &wait_loop, expect_result, request_result, devices_result);
    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   /*extension=*/nullptr);
    if (expect_result) {
      wait_loop.Run();
    } else {
      wait_loop.RunUntilIdle();
    }

    access_handler_.reset();
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }
  int main_frame_id() { return main_frame()->GetRoutingID(); }
  int process_id() { return main_frame()->GetProcess()->GetID(); }

 protected:
  std::unique_ptr<TabCaptureAccessHandler> access_handler_;
};

TEST_F(TabCaptureAccessHandlerTest, PermissionGiven) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents()->GetPrimaryMainFrame()->GetRoutingID()));

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, process_id(), main_frame_id());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::mojom::StreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_TRUE(devices.video_device.has_value());
  EXPECT_FALSE(devices.audio_device.has_value());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
            devices.video_device.value().type);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(TabCaptureAccessHandlerTest, DlpRestricted) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents()->GetPrimaryMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/false);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, process_id(), main_frame_id());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::mojom::StreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_FALSE(devices.video_device.has_value());
  EXPECT_FALSE(devices.audio_device.has_value());
}

TEST_F(TabCaptureAccessHandlerTest, DlpNotRestricted) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents()->GetPrimaryMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/true);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, process_id(), main_frame_id());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::mojom::StreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_TRUE(devices.video_device.has_value());
  EXPECT_FALSE(devices.audio_device.has_value());
}

TEST_F(TabCaptureAccessHandlerTest, DlpWebContentsDestroyed) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents()->GetPrimaryMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([&](const content::DesktopMediaID& media_id,
                    const std::u16string& application_title,
                    base::OnceCallback<void(bool)> callback) {
        DeleteContents();
        std::move(callback).Run(/*should_proceed=*/true);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, process_id(), main_frame_id());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::mojom::StreamDevices devices;
  ProcessRequest(source, &result, &devices, /*expect_result=*/false);

  EXPECT_EQ(kInvalidResult, result);
  EXPECT_FALSE(devices.video_device.has_value());
  EXPECT_FALSE(devices.audio_device.has_value());
}

#endif  // BUILDFLAG(IS_CHROMEOS)
