// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_events_sender.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCameraAppCrosEvents},
        /*disabled_features=*/{});

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CameraAppEventsSenderTest, StartSession) {
  auto params = ash::camera_app::mojom::StartSessionEventParams::New();
  params->launch_type = ash::camera_app::mojom::LaunchType::kAssistant;

  cros_events::CameraApp_StartSession expected_event;
  expected_event.SetLaunchType(static_cast<int64_t>(params->launch_type));
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
  expected_event.SetMode(static_cast<int64_t>(params->mode))
      .SetFacing(static_cast<int64_t>(params->facing))
      .SetIsMirrored(static_cast<int64_t>(params->is_mirrored))
      .SetGridType(static_cast<int64_t>(params->grid_type))
      .SetTimerType(static_cast<int64_t>(params->timer_type))
      .SetShutterType(static_cast<int64_t>(params->shutter_type))
      .SetAndroidIntentResultType(
          static_cast<int64_t>(params->android_intent_result_type))
      .SetIsWindowMaximized(static_cast<int64_t>(params->is_window_maximized))
      .SetIsWindowPortrait(static_cast<int64_t>(params->is_window_portrait))
      .SetResolutionWidth(static_cast<int64_t>(params->resolution_width))
      .SetResolutionHeight(static_cast<int64_t>(params->resolution_height))
      .SetResolutionLevel(static_cast<int64_t>(params->resolution_level))
      .SetAspectRatioSet(static_cast<int64_t>(params->aspect_ratio_set))
      .SetIsVideoSnapshot(static_cast<int64_t>(false))
      .SetIsMuted(static_cast<int64_t>(video_details->is_muted))
      .SetFps(static_cast<int64_t>(video_details->fps))
      .SetEverPaused(static_cast<int64_t>(video_details->ever_paused))
      .SetDuration(static_cast<int64_t>(video_details->duration))
      .SetRecordType(
          static_cast<int64_t>(ash::camera_app::mojom::RecordType::kTimelapse))
      .SetGifResultType(
          static_cast<int64_t>(ash::camera_app::mojom::GifResultType::kNotGif))
      .SetTimelapseSpeed(
          static_cast<int64_t>(timelapse_video_details->timelapse_speed));

  events_sender_->SendCaptureEvent(std::move(params));

  const std::vector<metrics::structured::Event>& events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(events.size(), 1U);

  auto& received_event = events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
}

}  // namespace ash
