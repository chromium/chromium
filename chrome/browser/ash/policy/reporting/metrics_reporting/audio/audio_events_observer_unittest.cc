// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"

#include <sys/types.h>

#include <utility>

#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
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

  void SetUp() override { ::chromeos::CrosHealthdClient::InitializeFake(); }

  void TearDown() override {
    ::chromeos::CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    ::chromeos::cros_healthd::ServiceConnection::GetInstance()
        ->FlushForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AudioEventsObserverTest, SevereUnderrun) {
  AudioEventsObserver audio_observer;
  test::TestEvent<MetricData> result_metric_data;

  audio_observer.SetOnEventObservedCallback(result_metric_data.cb());
  audio_observer.SetReportingEnabled(true);

  ::chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->EmitAudioSevereUnderrunEventForTesting();

  const auto metric_data = result_metric_data.result();
  ASSERT_TRUE(metric_data.has_event_data());
  EXPECT_EQ(metric_data.event_data().type(),
            reporting::MetricEventType::AUDIO_SEVERE_UNDERRUN);
}
}  // namespace
}  // namespace reporting
