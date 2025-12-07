// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_events_sender.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

constexpr char kTestLanguage[] = "zh-TW";
constexpr int64_t kTestLanguageValue = -1735828230;

}  // namespace

class CameraAppEventsSenderTest : public testing::Test {
 protected:
  CameraAppEventsSenderTest() {}
  CameraAppEventsSenderTest(const CameraAppEventsSenderTest&) = delete;
  CameraAppEventsSenderTest& operator=(const CameraAppEventsSenderTest&) =
      delete;
  ~CameraAppEventsSenderTest() override = default;

  void SetUp() override {
    events_sender_ = std::make_unique<CameraAppEventsSender>(kTestLanguage);

    metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    metrics_recorder_->Initialize();
  }

  void TearDown() override {
    metrics_recorder_.reset();
    events_sender_.reset();
  }

 protected:
  std::unique_ptr<CameraAppEventsSender> events_sender_;

  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      metrics_recorder_;
};

TEST_F(CameraAppEventsSenderTest, StartSession) {
  auto params = ash::camera_app::mojom::StartSessionEventParams::New();
  params->launch_type = ash::camera_app::mojom::LaunchType::kAssistant;

  cros_events::CameraApp_StartSession expected_event;
  expected_event.SetLaunchType(
      static_cast<cros_events::CameraAppLaunchType>(params->launch_type));
  expected_event.SetLanguage(kTestLanguageValue);

  events_sender_->SendStartSessionEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, Capture) {
  auto params = ash::camera_app::mojom::CaptureEventParams::New();
  params->mode = ash::camera_app::mojom::Mode::kVideo;
  params->facing = ash::camera_app::mojom::Facing::kExternal;
  params->is_mirrored = true;
  params->grid_type = ash::camera_app::mojom::GridType::kGolden;
  params->timer_type = ash::camera_app::mojom::TimerType::k10Seconds;
  params->shutter_type = ash::camera_app::mojom::ShutterType::kVolumeKey;
  params->android_intent_result_type =
      ash::camera_app::mojom::AndroidIntentResultType::kCanceled;
  params->is_window_maximized = true;
  params->is_window_portrait = true;
  params->resolution_width = 1080;
  params->resolution_height = 1920;
  params->resolution_level = ash::camera_app::mojom::ResolutionLevel::kFullHD;
  params->aspect_ratio_set = ash::camera_app::mojom::AspectRatioSet::k16To9;
  params->zoom_ratio = 1;

  auto video_details = ash::camera_app::mojom::VideoDetails::New();
  video_details->is_muted = true;
  video_details->fps = 30;
  video_details->ever_paused = true;
  video_details->duration = 10000;

  auto timelapse_video_details =
      ash::camera_app::mojom::TimelapseVideoDetails::New();
  timelapse_video_details->timelapse_speed = 10;

  auto record_type_details =
      ash::camera_app::mojom::RecordTypeDetails::NewTimelapseVideoDetails(
          timelapse_video_details.Clone());
  video_details->record_type_details = std::move(record_type_details);

  auto capture_details =
      ash::camera_app::mojom::CaptureDetails::NewVideoDetails(
          video_details.Clone());
  params->capture_details = std::move(capture_details);

  cros_events::CameraApp_Capture expected_event;
  expected_event.SetMode(static_cast<cros_events::CameraAppMode>(params->mode))
      .SetFacing(static_cast<cros_events::CameraAppFacing>(params->facing))
      .SetIsMirrored(static_cast<int64_t>(params->is_mirrored))
      .SetGridType(
          static_cast<cros_events::CameraAppGridType>(params->grid_type))
      .SetTimerType(
          static_cast<cros_events::CameraAppTimerType>(params->timer_type))
      .SetShutterType(
          static_cast<cros_events::CameraAppShutterType>(params->shutter_type))
      .SetAndroidIntentResultType(
          static_cast<cros_events::CameraAppAndroidIntentResultType>(
              params->android_intent_result_type))
      .SetIsWindowMaximized(static_cast<int64_t>(params->is_window_maximized))
      .SetIsWindowPortrait(static_cast<int64_t>(params->is_window_portrait))
      .SetResolutionWidth(static_cast<int64_t>(params->resolution_width))
      .SetResolutionHeight(static_cast<int64_t>(params->resolution_height))
      .SetResolutionLevel(static_cast<cros_events::CameraAppResolutionLevel>(
          params->resolution_level))
      .SetAspectRatioSet(static_cast<cros_events::CameraAppAspectRatioSet>(
          params->aspect_ratio_set))
      .SetIsVideoSnapshot(static_cast<int64_t>(false))
      .SetIsMuted(static_cast<int64_t>(video_details->is_muted))
      .SetFps(static_cast<int64_t>(video_details->fps))
      .SetEverPaused(static_cast<int64_t>(video_details->ever_paused))
      .SetDuration(static_cast<int64_t>(video_details->duration))
      .SetRecordType(static_cast<cros_events::CameraAppRecordType>(
          ash::camera_app::mojom::RecordType::kTimelapse))
      .SetGifResultType(static_cast<cros_events::CameraAppGifResultType>(
          ash::camera_app::mojom::GifResultType::kNotGif))
      .SetTimelapseSpeed(
          static_cast<int64_t>(timelapse_video_details->timelapse_speed))
      .SetZoomRatio(static_cast<double>(params->zoom_ratio));

  events_sender_->SendCaptureEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, AndroidIntent) {
  auto params = ash::camera_app::mojom::AndroidIntentEventParams::New();
  params->mode = ash::camera_app::mojom::Mode::kVideo;
  params->should_handle_result = true;
  params->should_downscale = true;
  params->is_secure = true;

  cros_events::CameraApp_AndroidIntent expected_event;
  expected_event.SetMode(static_cast<cros_events::CameraAppMode>(params->mode))
      .SetShouldHandleResult(static_cast<int64_t>(params->should_handle_result))
      .SetShouldDownscale(static_cast<int64_t>(params->should_downscale))
      .SetIsSecure(static_cast<int64_t>(params->is_secure));

  events_sender_->SendAndroidIntentEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, OpenPTZPanel) {
  auto params = ash::camera_app::mojom::OpenPTZPanelEventParams::New();
  params->support_pan = true;
  params->support_tilt = true;
  params->support_zoom = true;

  cros_events::CameraApp_OpenPTZPanel expected_event;
  expected_event.SetSupportPan(static_cast<int64_t>(params->support_pan))
      .SetSupportTilt(static_cast<int64_t>(params->support_tilt))
      .SetSupportZoom(static_cast<int64_t>(params->support_zoom));

  events_sender_->SendOpenPTZPanelEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, DocScanAction) {
  auto params = ash::camera_app::mojom::DocScanActionEventParams::New();
  params->action_type = ash::camera_app::mojom::DocScanActionType::kFix;

  cros_events::CameraApp_DocScanAction expected_event;
  expected_event.SetActionType(
      static_cast<cros_events::CameraAppDocScanActionType>(
          params->action_type));

  events_sender_->SendDocScanActionEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, DocScanResult) {
  auto params = ash::camera_app::mojom::DocScanResultEventParams::New();
  params->result_type = ash::camera_app::mojom::DocScanResultType::kShare;
  params->fix_types_mask =
      static_cast<uint32_t>(ash::camera_app::mojom::DocScanFixType::kCorner);
  params->fix_count = 1;
  params->page_count = 1;

  cros_events::CameraApp_DocScanResult expected_event;
  expected_event
      .SetResultType(static_cast<cros_events::CameraAppDocScanResultType>(
          params->result_type))
      .SetFixTypes(static_cast<int64_t>(params->fix_types_mask))
      .SetFixCount(static_cast<int64_t>(params->fix_count))
      .SetPageCount(static_cast<int64_t>(params->page_count));

  events_sender_->SendDocScanResultEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, OpenCamera) {
  constexpr char kTestCameraModuleId[] = "foo:bar";

  auto params = ash::camera_app::mojom::OpenCameraEventParams::New();
  auto usb_camera = ash::camera_app::mojom::UsbCameraModule::New();
  usb_camera->id = kTestCameraModuleId;
  auto camera_module =
      ash::camera_app::mojom::CameraModule::NewUsbCamera(usb_camera.Clone());
  params->camera_module = camera_module.Clone();

  cros_events::CameraApp_OpenCamera expected_event;
  expected_event.SetCameraModuleId(kTestCameraModuleId);

  events_sender_->SendOpenCameraEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, LowStorageAction) {
  auto params = ash::camera_app::mojom::LowStorageActionEventParams::New();
  params->action_type =
      ash::camera_app::mojom::LowStorageActionType::kShowWarningMessage;

  cros_events::CameraApp_LowStorageAction expected_event;
  expected_event.SetActionType(
      static_cast<cros_events::CameraAppLowStorageActionType>(
          params->action_type));

  events_sender_->SendLowStorageActionEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, BarcodeDetected) {
  auto params = ash::camera_app::mojom::BarcodeDetectedEventParams::New();
  params->content_type = ash::camera_app::mojom::BarcodeContentType::kWiFi;
  params->wifi_security_type = ash::camera_app::mojom::WifiSecurityType::kWpa;

  cros_events::CameraApp_BarcodeDetected expected_event;
  expected_event
      .SetContentType(static_cast<cros_events::CameraAppBarcodeContentType>(
          params->content_type))
      .SetWifiSecurityType(static_cast<cros_events::CameraAppWifiSecurityType>(
          params->wifi_security_type));

  events_sender_->SendBarcodeDetectedEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, Perf) {
  auto params = ash::camera_app::mojom::PerfEventParams::New();
  params->event_type =
      ash::camera_app::mojom::PerfEventType::kVideoCapturePostProcessingSaving;
  params->duration = 10000;
  params->facing = ash::camera_app::mojom::Facing::kUnknown;
  params->resolution_width = 1920;
  params->resolution_height = 1080;
  params->page_count = 10;
  params->pressure = ash::camera_app::mojom::Pressure::kFair;

  cros_events::CameraApp_Perf expected_event;
  expected_event
      .SetEventType(
          static_cast<cros_events::CameraAppPerfEventType>(params->event_type))
      .SetDuration(static_cast<int64_t>(params->duration))
      .SetFacing(static_cast<cros_events::CameraAppFacing>(params->facing))
      .SetResolutionWidth(static_cast<int64_t>(params->resolution_width))
      .SetResolutionHeight(static_cast<int64_t>(params->resolution_height))
      .SetPageCount(static_cast<int64_t>(params->page_count))
      .SetPressure(
          static_cast<cros_events::CameraAppPressure>(params->pressure));

  events_sender_->SendPerfEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

TEST_F(CameraAppEventsSenderTest, UnsupportedProtocol) {
  cros_events::CameraApp_UnsupportedProtocol expected_event;

  events_sender_->SendUnsupportedProtocolEvent();
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
}

TEST_F(CameraAppEventsSenderTest, EndSession) {
  // To send a end session event, a start session event should be sent first.
  auto start_session_params =
      ash::camera_app::mojom::StartSessionEventParams::New();
  start_session_params->launch_type =
      ash::camera_app::mojom::LaunchType::kAssistant;
  events_sender_->SendStartSessionEvent(std::move(start_session_params));

  // The end session event will be sent when the mojo connection dropped.
  events_sender_->OnMojoDisconnected();

  // [0]: Start Session Event.
  // [1]: End Session Event.
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 2U);
  auto& received_event = events[1];

  cros_events::CameraApp_EndSession expected_event;
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
}

TEST_F(CameraAppEventsSenderTest, MemoryUsage) {
  // To send a memory usage event, a start session event should be sent first.
  auto start_session_params =
      ash::camera_app::mojom::StartSessionEventParams::New();
  start_session_params->launch_type =
      ash::camera_app::mojom::LaunchType::kAssistant;
  events_sender_->SendStartSessionEvent(std::move(start_session_params));

  // Updates the memory usage event to be brought with the end session event.
  auto params = ash::camera_app::mojom::MemoryUsageEventParams::New();
  params->behaviors_mask = static_cast<uint32_t>(
      ash::camera_app::mojom::UserBehavior::kRecordTimelapseVideo);
  params->memory_usage = 10000;
  events_sender_->UpdateMemoryUsageEventParams(params.Clone());

  // The memory usage event will be sent when the mojo connection dropped.
  events_sender_->OnMojoDisconnected();

  // [0]: Start Session Event.
  // [1]: End Session Event.
  // [2]: Memory usage Event.
  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 3U);
  auto& received_event = events[2];

  cros_events::CameraApp_MemoryUsage expected_event;
  expected_event.SetBehaviors(static_cast<int64_t>(params->behaviors_mask))
      .SetMemoryUsage(static_cast<int64_t>(params->memory_usage));

  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  auto& received_metrics = received_event.metric_values();
  auto& expected_metrics = expected_event.metric_values();
  for (auto it = received_metrics.begin(); it != received_metrics.end(); it++) {
    EXPECT_EQ(it->second, expected_metrics.at(it->first));
  }
}

TEST_F(CameraAppEventsSenderTest, Ocr) {
  auto params = ash::camera_app::mojom::OcrEventParams::New();
  params->event_type = ash::camera_app::mojom::OcrEventType::kCopyText;
  params->is_primary_language = true;
  params->line_count = 10;
  params->word_count = 20;

  cros_events::CameraApp_Ocr expected_event;
  expected_event
      .SetEventType(
          static_cast<cros_events::CameraAppOcrEventType>(params->event_type))
      .SetIsPrimaryLanguage(static_cast<int64_t>(true))
      .SetLineCount(static_cast<int64_t>(params->line_count))
      .SetWordCount(static_cast<int64_t>(params->word_count));

  events_sender_->SendOcrEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

}  // namespace ash
