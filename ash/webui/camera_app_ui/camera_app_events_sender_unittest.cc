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

}  // namespace ash
