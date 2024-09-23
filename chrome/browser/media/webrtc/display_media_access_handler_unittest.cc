// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/display_media_access_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

class DisplayMediaAccessHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  DisplayMediaAccessHandlerTest() = default;
  ~DisplayMediaAccessHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto picker_factory = std::make_unique<FakeDesktopMediaPickerFactory>();
    picker_factory_ = picker_factory.get();
    access_handler_ = std::make_unique<DisplayMediaAccessHandler>(
        std::move(picker_factory), false /* display_notification */);
  }

  content::WebContentsMediaCaptureId GetWebContentsMediaCaptureId() {
    return content::WebContentsMediaCaptureId(
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(), 1);
  }

  FakeDesktopMediaPickerFactory::TestFlags MakePickerTestFlags(
      bool request_audio) {
    return FakeDesktopMediaPickerFactory::TestFlags(
        {.expect_screens = true,
         .expect_windows = true,
         .expect_tabs = true,
         .expect_current_tab = false,
         .expect_audio = request_audio,
         .selected_source =
             content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                     content::DesktopMediaID::kFakeId)});
  }

  content::MediaStreamRequest MakeRequest(bool request_audio) {
    return content::MediaStreamRequest(
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
        web_contents()->GetPrimaryMainFrame()->GetRoutingID(), 0,
        url::Origin::Create(GURL("http://origin/")), false,
        blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
        /*requested_video_device_ids=*/{},
        request_audio ? blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE
                      : blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*captured_surface_control_active=*/false);
  }

  content::MediaStreamRequest MakeMediaDeviceUpdateRequest(bool request_audio) {
    content::MediaStreamRequest request =
        MakeRequest(request_audio /* request_audio */);
    request.request_type = blink::MEDIA_DEVICE_UPDATE;
    request.requested_video_device_ids = {
        GetWebContentsMediaCaptureId().ToString()};
    return request;
  }

  content::MediaStreamRequest MakeExcludeSelfBrowserSurfaceRequest(
      bool exclude_self_browser_surface) {
    content::MediaStreamRequest request = MakeRequest(/*request_audio=*/false);
    request.exclude_self_browser_surface = exclude_self_browser_surface;
    return request;
  }

  content::MediaStreamRequest MakeExcludeMonitorTypeSurfacesRequest(
      bool exclude_monitor_type_surfaces) {
    content::MediaStreamRequest request = MakeRequest(/*request_audio=*/false);
    request.exclude_monitor_type_surfaces = exclude_monitor_type_surfaces;
    return request;
  }

  content::MediaResponseCallback MakeCallback(
      base::RunLoop* wait_loop,
      blink::mojom::MediaStreamRequestResult* request_result,
      blink::mojom::StreamDevices& devices_result) {
    return base::BindOnce(
        [](base::RunLoop* wait_loop,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::mojom::StreamDevices* devices_result,
           const blink::mojom::StreamDevicesSet& stream_devices_set,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          *request_result = result;
          if (result == blink::mojom::MediaStreamRequestResult::OK) {
            ASSERT_EQ(stream_devices_set.stream_devices.size(), 1u);
            *devices_result = *stream_devices_set.stream_devices[0];
          } else {
            ASSERT_TRUE(stream_devices_set.stream_devices.empty());
            *devices_result = blink::mojom::StreamDevices();
          }
          wait_loop->Quit();
        },
        wait_loop, request_result, &devices_result);
  }

  void HandleRequest(const content::MediaStreamRequest& request,
                     base::RunLoop* wait_loop,
                     blink::mojom::MediaStreamRequestResult* request_result,
                     blink::mojom::StreamDevices& devices_result) {
    access_handler_->HandleRequest(
        web_contents(), request,
        MakeCallback(wait_loop, request_result, devices_result),
        nullptr /* extension */);
  }

  void SetTestFlags(
      std::vector<FakeDesktopMediaPickerFactory::TestFlags> test_flags_vector) {
    test_flags_ = std::move(test_flags_vector);
    picker_factory_->SetTestFlags(&test_flags_[0], test_flags_.size());
  }

  void ProcessRequest(
      const content::DesktopMediaID& fake_desktop_media_id_response,
      blink::mojom::MediaStreamRequestResult* request_result,
      blink::mojom::StreamDevices& devices_result,
      bool request_audio,
      bool expect_result = true) {
    SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                   true /* expect_tabs */, /* expect_current_tab, */ false,
                   request_audio,
                   fake_desktop_media_id_response /* selected_source */}});

    content::MediaStreamRequest request = MakeRequest(request_audio);

    base::RunLoop wait_loop;
    content::MediaResponseCallback callback;
    if (expect_result) {
      callback = MakeCallback(&wait_loop, request_result, devices_result);
    } else {
      base::MockCallback<content::MediaResponseCallback> mock_callback =
          base::MockCallback<content::MediaResponseCallback>();
      EXPECT_CALL(mock_callback, Run).Times(0);
      callback = mock_callback.Get();
    }

    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   nullptr /* extension */);
    if (expect_result) {
      wait_loop.Run();
    } else {
      wait_loop.RunUntilIdle();
    }

    EXPECT_TRUE(test_flags_[0].picker_created);

    picker_factory_ = nullptr;
    access_handler_.reset();
    EXPECT_TRUE(test_flags_[0].picker_deleted);
  }

  void NotifyWebContentsDestroyed() {
    access_handler_->WebContentsDestroyed(web_contents());
  }

  bool IsWebContentsExcluded() const {
    return picker_factory_->IsWebContentsExcluded();
  }

  DesktopMediaPicker::Params GetParams() {
    return picker_factory_->picker()->GetParams();
  }

  const DisplayMediaAccessHandler::RequestsQueues& GetRequestQueues() {
    return access_handler_->pending_requests_;
  }

  void ChangeSourceRequestTest(
      bool with_audio,
      blink::mojom::MediaStreamRequestResult expected_result,
      size_t expected_number_of_devices) {
    blink::mojom::MediaStreamRequestResult result;
    blink::mojom::StreamDevices devices;
    SetTestFlags({MakePickerTestFlags(with_audio /*request_audio*/)});

    base::RunLoop wait_loop;
    HandleRequest(MakeMediaDeviceUpdateRequest(with_audio /* request_audio */),
                  &wait_loop, &result, devices);
    wait_loop.Run();
    EXPECT_FALSE(test_flags_[0].picker_created);

    picker_factory_ = nullptr;
    access_handler_.reset();
    EXPECT_EQ(expected_result, result);

    ASSERT_EQ(expected_number_of_devices, blink::CountDevices(devices));
    if (expected_number_of_devices >= 1) {
      EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                devices.video_device.value().type);
    }
    if (expected_number_of_devices >= 2) {
      EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                devices.audio_device.value().type);
    }
  }

  std::vector<FakeDesktopMediaPickerFactory::TestFlags> test_flags_;

 protected:
  // `access_handler` owns `picker_factory` and must outlive it.
  std::unique_ptr<DisplayMediaAccessHandler> access_handler_;
  raw_ptr<FakeDesktopMediaPickerFactory> picker_factory_;
};

TEST_F(DisplayMediaAccessHandlerTest, PermissionGiven) {
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  ProcessRequest(content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId),
                 &result, devices, false /* request_audio */);
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            result);
  return;
#endif

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, blink::CountDevices(devices));
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            devices.video_device.value().type);
  EXPECT_TRUE(devices.video_device.value().display_media_info);
}

#if BUILDFLAG(IS_MAC)
TEST_F(DisplayMediaAccessHandlerTest, WindowPermissionGiven) {
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  content::DesktopMediaID desktop_media_id(content::DesktopMediaID::TYPE_WINDOW,
                                           content::DesktopMediaID::kFakeId);

  // Requests with a window_id will skip macOS screen share permission checks.
  desktop_media_id.window_id = content::DesktopMediaID::kFakeId;
  ProcessRequest(desktop_media_id, &result, devices, false /* request_audio */);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, blink::CountDevices(devices));
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            devices.video_device.value().type);
  EXPECT_TRUE(devices.video_device.value().display_media_info);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(DisplayMediaAccessHandlerTest, PermissionGivenToRequestWithAudio) {
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  content::DesktopMediaID fake_media_id(content::DesktopMediaID::TYPE_SCREEN,
                                        content::DesktopMediaID::kFakeId,
                                        true /* audio_share */);
  ProcessRequest(fake_media_id, &result, devices, true /* request_audio */);
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            result);
  return;
#endif
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(2u, blink::CountDevices(devices));
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            devices.video_device.value().type);
  EXPECT_TRUE(devices.video_device.value().display_media_info);
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
            devices.audio_device.value().type);
  EXPECT_TRUE(devices.audio_device.value().input.IsValid());
}

TEST_F(DisplayMediaAccessHandlerTest, PermissionDenied) {
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  ProcessRequest(content::DesktopMediaID(), &result, devices,
                 true /* request_audio */);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_EQ(0u, blink::CountDevices(devices));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(DisplayMediaAccessHandlerTest, DlpRestricted) {
  const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId);

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/false);
      });

  blink::mojom::MediaStreamRequestResult result =
      blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
  blink::mojom::StreamDevices devices;
  ProcessRequest(media_id, &result, devices, /*request_audio=*/false);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_EQ(0u, blink::CountDevices(devices));
}

TEST_F(DisplayMediaAccessHandlerTest, DlpNotRestricted) {
  const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId);

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

  blink::mojom::MediaStreamRequestResult result =
      blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
  blink::mojom::StreamDevices devices;
  ProcessRequest(media_id, &result, devices, /*request_audio=*/false);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, blink::CountDevices(devices));
}

TEST_F(DisplayMediaAccessHandlerTest, DlpWebContentsDestroyed) {
  const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId);

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

  blink::mojom::MediaStreamRequestResult result =
      blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
  blink::mojom::StreamDevices devices;
  ProcessRequest(media_id, &result, devices, /*request_audio=*/false,
                 /*expect_result=*/false);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, result);
  EXPECT_EQ(0u, blink::CountDevices(devices));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(DisplayMediaAccessHandlerTest, UpdateMediaRequestStateWithClosing) {
  const int render_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  const int render_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetRoutingID();
  const int page_request_id = 0;
  const blink::mojom::MediaStreamType video_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  const blink::mojom::MediaStreamType audio_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 true /* expect_audio */, content::DesktopMediaID(),
                 true /* cancelled */}});
  content::MediaStreamRequest request(
      render_process_id, render_frame_id, page_request_id,
      url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{}, audio_stream_type, video_stream_type,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  content::MediaResponseCallback callback;
  access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                 nullptr /* extension */);
  EXPECT_TRUE(test_flags_[0].picker_created);
  EXPECT_EQ(1u, GetRequestQueues().size());
  auto queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(1u, queue_it->second.size());

  access_handler_->UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, video_stream_type,
      content::MEDIA_REQUEST_STATE_CLOSING);
  EXPECT_EQ(1u, GetRequestQueues().size());
  queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(0u, queue_it->second.size());
  EXPECT_TRUE(test_flags_[0].picker_deleted);
}

TEST_F(DisplayMediaAccessHandlerTest, CorrectHostAsksForPermissions) {
  const int render_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  const int render_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetRoutingID();
  const int page_request_id = 0;
  const blink::mojom::MediaStreamType video_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  const blink::mojom::MediaStreamType audio_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 true /* expect_audio */, content::DesktopMediaID(),
                 true /* cancelled */}});
  content::MediaStreamRequest request(
      render_process_id, render_frame_id, page_request_id,
      url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{}, audio_stream_type, video_stream_type,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  content::MediaResponseCallback callback;
  content::WebContents* test_web_contents = web_contents();
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("blob:http://127.0.0.1:8000/says: www.google.com"),
          test_web_contents);
  navigation->Commit();
  access_handler_->HandleRequest(test_web_contents, request,
                                 std::move(callback), nullptr /* extension */);
  DesktopMediaPicker::Params params = GetParams();
  access_handler_->UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, video_stream_type,
      content::MEDIA_REQUEST_STATE_CLOSING);
  EXPECT_EQ(u"http://127.0.0.1:8000", params.app_name);
}

TEST_F(DisplayMediaAccessHandlerTest, CorrectHostAsksForPermissionsNormalURLs) {
  const int render_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  const int render_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetRoutingID();
  const int page_request_id = 0;
  const blink::mojom::MediaStreamType video_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  const blink::mojom::MediaStreamType audio_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 true /* expect_audio */, content::DesktopMediaID(),
                 true /* cancelled */}});
  content::MediaStreamRequest request(
      render_process_id, render_frame_id, page_request_id,
      url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{}, audio_stream_type, video_stream_type,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  content::MediaResponseCallback callback;
  content::WebContents* test_web_contents = web_contents();
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://www.google.com"), test_web_contents);
  navigation->Commit();
  access_handler_->HandleRequest(test_web_contents, request,
                                 std::move(callback), nullptr /* extension */);
  DesktopMediaPicker::Params params = GetParams();
  access_handler_->UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, video_stream_type,
      content::MEDIA_REQUEST_STATE_CLOSING);
  EXPECT_EQ(u"www.google.com", params.app_name);
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(DisplayMediaAccessHandlerTest, IsolatedWebAppNameAsksForPermissions) {
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  const std::string app_name("Test IWA Name");
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> iwa =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .SetName(app_name)
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kDisplayCapture))
          .BuildBundle();
  iwa->TrustSigningKey();
  iwa->FakeInstallPageState(profile());
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       iwa->Install(profile()));
  web_app::SimulateIsolatedWebAppNavigation(web_contents(),
                                            url_info.origin().GetURL());

  const int render_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  const int render_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetRoutingID();
  const int page_request_id = 0;
  const blink::mojom::MediaStreamType video_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  const blink::mojom::MediaStreamType audio_stream_type =
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 true /* expect_audio */, content::DesktopMediaID(),
                 true /* cancelled */}});
  content::MediaStreamRequest request(
      render_process_id, render_frame_id, page_request_id,
      url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{}, audio_stream_type, video_stream_type,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  content::MediaResponseCallback callback;
  access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                 nullptr /* extension */);
  DesktopMediaPicker::Params params = GetParams();
  access_handler_->UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, video_stream_type,
      content::MEDIA_REQUEST_STATE_CLOSING);
  EXPECT_EQ(base::UTF8ToUTF16(app_name), params.app_name);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DisplayMediaAccessHandlerTest, WebContentsDestroyed) {
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 false /* expect_audio */, content::DesktopMediaID(),
                 true /* cancelled */}});
  content::MediaStreamRequest request(
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
      web_contents()->GetPrimaryMainFrame()->GetRoutingID(), 0,
      url::Origin::Create(GURL("http://origin/")), false,
      blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
      /*requested_video_device_ids=*/{},
      blink::mojom::MediaStreamType::NO_SERVICE,
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
      /*disable_local_echo=*/false, /*request_pan_tilt_zoom_permission=*/false,
      /*captured_surface_control_active=*/false);
  content::MediaResponseCallback callback;
  access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                 nullptr /* extension */);
  EXPECT_TRUE(test_flags_[0].picker_created);
  EXPECT_EQ(1u, GetRequestQueues().size());
  auto queue_it = GetRequestQueues().find(web_contents());
  EXPECT_TRUE(queue_it != GetRequestQueues().end());
  EXPECT_EQ(1u, queue_it->second.size());

  NotifyWebContentsDestroyed();
  EXPECT_EQ(0u, GetRequestQueues().size());
}

TEST_F(DisplayMediaAccessHandlerTest, MultipleRequests) {
  SetTestFlags({{true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 false /* expect_audio */,
                 content::DesktopMediaID(
                     content::DesktopMediaID::TYPE_SCREEN,
                     content::DesktopMediaID::kFakeId) /* selected_source */},
                {true /* expect_screens */, true /* expect_windows*/,
                 true /* expect_tabs */, false /* expect_current_tab */,
                 false /* expect_audio */,
                 content::DesktopMediaID(
                     content::DesktopMediaID::TYPE_WINDOW,
                     content::DesktopMediaID::kNullId) /* selected_source */}});
  const size_t kTestFlagCount = 2;

  blink::mojom::MediaStreamRequestResult result;
  blink::MediaStreamDevices devices;
  base::RunLoop wait_loop[kTestFlagCount];
  for (size_t i = 0; i < kTestFlagCount; ++i) {
    content::MediaStreamRequest request(
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
        web_contents()->GetPrimaryMainFrame()->GetRoutingID(), 0,
        url::Origin::Create(GURL("http://origin/")), false,
        blink::MEDIA_GENERATE_STREAM, /*requested_audio_device_ids=*/{},
        /*requested_video_device_ids=*/{},
        blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*captured_surface_control_active=*/false);
    content::MediaResponseCallback callback = base::BindOnce(
        [](base::RunLoop* wait_loop,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::MediaStreamDevices* devices_result,
           const blink::mojom::StreamDevicesSet& stream_devices_set,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          *request_result = result;
          if (result == blink::mojom::MediaStreamRequestResult::OK) {
            ASSERT_EQ(stream_devices_set.stream_devices.size(), 1u);
            *devices_result =
                blink::ToMediaStreamDevicesList(stream_devices_set);
          } else {
            ASSERT_TRUE(stream_devices_set.stream_devices.empty());
          }
          wait_loop->Quit();
        },
        &wait_loop[i], &result, &devices);
    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   nullptr /* extension */);
  }
  wait_loop[0].Run();
  EXPECT_TRUE(test_flags_[0].picker_created);
  EXPECT_TRUE(test_flags_[0].picker_deleted);
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            result);
  return;
#endif
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            devices[0].type);

  blink::MediaStreamDevice first_device = devices[0];
  EXPECT_TRUE(test_flags_[1].picker_created);
  EXPECT_FALSE(test_flags_[1].picker_deleted);
  wait_loop[1].Run();
  EXPECT_TRUE(test_flags_[1].picker_deleted);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            devices[0].type);
  EXPECT_FALSE(devices[0].IsSameDevice(first_device));
}

TEST_F(DisplayMediaAccessHandlerTest,
       ChangeSourceWithoutAudioRequestPermissionGiven) {
  ChangeSourceRequestTest(
      /*with_audio=*/false,
      /*expected_result=*/blink::mojom::MediaStreamRequestResult::OK,
      /*expected_number_of_devices=*/1u);
}

TEST_F(DisplayMediaAccessHandlerTest,
       ChangeSourceWithAudioRequestPermissionGiven) {
  blink::MediaStreamDevices devices;
  ChangeSourceRequestTest(
      /*with_audio=*/true,
      /*expected_result=*/blink::mojom::MediaStreamRequestResult::OK,
      /*expected_number_of_devices=*/2u);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(DisplayMediaAccessHandlerTest, ChangeSourceDlpRestricted) {
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId, GetWebContentsMediaCaptureId());

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/false);
      });

  ChangeSourceRequestTest(
      /*with_audio=*/false,
      /*expected_result=*/
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
      /*expected_number_of_devices=*/0u);
}

TEST_F(DisplayMediaAccessHandlerTest, ChangeSourceDlpNotRestricted) {
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId, GetWebContentsMediaCaptureId());

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

  ChangeSourceRequestTest(
      /*with_audio=*/false,
      /*expected_result=*/
      blink::mojom::MediaStreamRequestResult::OK,
      /*expected_number_of_devices=*/1u);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(DisplayMediaAccessHandlerTest, ChangeSourceWithPendingPickerRequest) {
  SetTestFlags({MakePickerTestFlags(false /*request_audio*/),
                MakePickerTestFlags(false /*request_audio*/)});

  blink::mojom::MediaStreamRequestResult results[2];
  blink::mojom::StreamDevices devices[2];
  base::RunLoop wait_loop[2];

  HandleRequest(MakeRequest(false /* request_audio */), &wait_loop[0],
                &results[0], devices[0]);
  HandleRequest(MakeMediaDeviceUpdateRequest(false /* request_audio */),
                &wait_loop[1], &results[1], devices[1]);

  wait_loop[0].Run();
  EXPECT_TRUE(test_flags_[0].picker_created);
  EXPECT_TRUE(test_flags_[0].picker_deleted);
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            results[0]);
  return;
#endif
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, results[0]);
  EXPECT_FALSE(test_flags_[1].picker_created);
  EXPECT_FALSE(test_flags_[1].picker_deleted);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, results[1]);
}

TEST_F(DisplayMediaAccessHandlerTest,
       ChangeSourcePolicyViolationWithPendingPickerRequest) {
  SetTestFlags({MakePickerTestFlags(false /*request_audio*/),
                MakePickerTestFlags(false /*request_audio*/)});

  blink::mojom::MediaStreamRequestResult results[2];
  blink::mojom::StreamDevices devices[2];
  base::RunLoop wait_loop[2];

  HandleRequest(MakeRequest(false /* request_audio */), &wait_loop[0],
                &results[0], devices[0]);
  HandleRequest(MakeMediaDeviceUpdateRequest(false /* request_audio */),
                &wait_loop[1], &results[1], devices[1]);

  // Policy is changed after the requests are received, but before they are
  // processed in the call to wait_loop.Run() below.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);

  wait_loop[0].Run();
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            results[0]);
  return;
#endif
  EXPECT_FALSE(test_flags_[1].picker_created);
  EXPECT_FALSE(test_flags_[1].picker_deleted);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            results[1]);
}

TEST_F(DisplayMediaAccessHandlerTest,
       MalformedChangeSourceBetweenPickerRequests) {
  SetTestFlags({{MakePickerTestFlags(false /*request_audio*/)}});

  blink::mojom::MediaStreamRequestResult results[3];
  blink::mojom::StreamDevices devices[3];
  base::RunLoop wait_loop[3];

  HandleRequest(MakeRequest(false /* request_audio */), &wait_loop[0],
                &results[0], devices[0]);
  {
    content::MediaStreamRequest request =
        MakeMediaDeviceUpdateRequest(false /* request_audio */);
    request.requested_video_device_ids = {"MALFORMED"};
    HandleRequest(request, &wait_loop[1], &results[1], devices[1]);
  }
  HandleRequest(MakeMediaDeviceUpdateRequest(false /* request_audio */),
                &wait_loop[2], &results[2], devices[2]);

  wait_loop[0].Run();
  EXPECT_TRUE(test_flags_[0].picker_created);
  EXPECT_TRUE(test_flags_[0].picker_deleted);
// TODO(crbug.com/40802122): Fix screen-capture tests on macOS.
#if BUILDFLAG(IS_MAC)
  // On macOS, screen capture requires system permissions that are disabled by
  // default.
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
            results[0]);
  return;
#endif
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, results[0]);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::INVALID_STATE, results[1]);
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, results[2]);
}

class DisplayMediaAccessHandlerTestWithSelfBrowserSurface
    : public DisplayMediaAccessHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  DisplayMediaAccessHandlerTestWithSelfBrowserSurface()
      : exclude_self_browser_surface_(GetParam()) {}

  ~DisplayMediaAccessHandlerTestWithSelfBrowserSurface() override = default;

 protected:
  const bool exclude_self_browser_surface_;
};

INSTANTIATE_TEST_SUITE_P(_,
                         DisplayMediaAccessHandlerTestWithSelfBrowserSurface,
                         ::testing::Bool());

TEST_P(DisplayMediaAccessHandlerTestWithSelfBrowserSurface,
       CheckIsWebContentsExcluded) {
  SetTestFlags({{MakePickerTestFlags(/*request_audio=*/false)}});
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  base::RunLoop wait_loop;

  HandleRequest(
      MakeExcludeSelfBrowserSurfaceRequest(exclude_self_browser_surface_),
      &wait_loop, &result, devices);
  wait_loop.Run();
  EXPECT_EQ(exclude_self_browser_surface_, IsWebContentsExcluded());
}

class DisplayMediaAccessHandlerTestWithMonitorTypeSurfaces
    : public DisplayMediaAccessHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  DisplayMediaAccessHandlerTestWithMonitorTypeSurfaces()
      : exclude_monitor_type_surfaces_(GetParam()) {}

  ~DisplayMediaAccessHandlerTestWithMonitorTypeSurfaces() override = default;

 protected:
  const bool exclude_monitor_type_surfaces_;
};

INSTANTIATE_TEST_SUITE_P(_,
                         DisplayMediaAccessHandlerTestWithMonitorTypeSurfaces,
                         ::testing::Bool());

TEST_P(DisplayMediaAccessHandlerTestWithMonitorTypeSurfaces,
       CheckMonitorTypeSurfacesAreExcluded) {
  SetTestFlags({{/*expect_screens=*/!exclude_monitor_type_surfaces_,
                 /*expect_windows=*/true,
                 /*expect_tabs=*/true, /*expect_current_tab=*/false,
                 /*expect_audio=*/false, content::DesktopMediaID(),
                 /*cancelled=*/false}});
  blink::mojom::MediaStreamRequestResult result;
  blink::mojom::StreamDevices devices;
  base::RunLoop wait_loop;

  HandleRequest(
      MakeExcludeMonitorTypeSurfacesRequest(exclude_monitor_type_surfaces_),
      &wait_loop, &result, devices);
  wait_loop.Run();
}
