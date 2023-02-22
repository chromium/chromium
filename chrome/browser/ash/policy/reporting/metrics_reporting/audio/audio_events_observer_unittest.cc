// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"

#include <sys/types.h>

#include <utility>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class AudioEventsObserverTest : public ::testing::Test {
 public:
  AudioEventsObserverTest() = default;

  AudioEventsObserverTest(const AudioEventsObserverTest&) = delete;
  AudioEventsObserverTest& operator=(const AudioEventsObserverTest&) = delete;

  ~AudioEventsObserverTest() override = default;

  void SetUp() override { ::ash::cros_healthd::FakeCrosHealthd::Initialize(); }

  void TearDown() override { ::ash::cros_healthd::FakeCrosHealthd::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_F(AudioEventsObserverTest, SevereUnderrun) {
  AudioEventsObserver audio_observer;
  test::TestEvent<MetricData> result_metric_data;

  audio_observer.SetOnEventObservedCallback(result_metric_data.repeating_cb());
  audio_observer.SetReportingEnabled(true);

  ::ash::cros_healthd::mojom::AudioEventInfo info;
  info.state =
      ::ash::cros_healthd::mojom::AudioEventInfo::State::kSevereUnderrun;
  ::ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
      ::ash::cros_healthd::mojom::EventCategoryEnum::kAudio,
      ::ash::cros_healthd::mojom::EventInfo::NewAudioEventInfo(info.Clone()));

  const auto metric_data = result_metric_data.result();
  ASSERT_TRUE(metric_data.has_event_data());
  EXPECT_EQ(metric_data.event_data().type(),
            reporting::MetricEventType::AUDIO_SEVERE_UNDERRUN);
}

}  // namespace
}  // namespace reporting
