// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/cros_events_processor.h"

#include "base/time/time.h"
#include "chrome/browser/metrics/structured/structured_metric_prefs.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured::cros_event {

namespace {

class CrOSEventsProcessorTest : public testing::Test {
 protected:
  void SetUp() override {
    processor_ = std::make_unique<CrOSEventsProcessor>(&test_pref_service_);
    CrOSEventsProcessor::RegisterLocalStatePrefs(test_pref_service_.registry());
  }

  void Initialize(int64_t last_uptime, int reset_counter) {
    test_pref_service_.SetInteger(kEventSequenceResetCounter, reset_counter);
    test_pref_service_.SetInt64(kEventSequenceLastSystemUptime, last_uptime);
  }

  int GetResetCounter() {
    return test_pref_service_.GetInteger(kEventSequenceResetCounter);
  }

  int64_t GetLastSystemUptime() {
    return test_pref_service_.GetInt64(kEventSequenceLastSystemUptime);
  }

  TestingPrefServiceSimple test_pref_service_;
  std::unique_ptr<cros_event::CrOSEventsProcessor> processor_;
};

}  // namespace

TEST_F(CrOSEventsProcessorTest, CheckResetCounterUpdatedOnReset) {
  int64_t last_uptime = 20;
  int reset_counter = 10;

  Initialize(last_uptime, reset_counter);
  // Sets the current uptime to be less than the last uptime to emulate a reset
  // counter scenario.
  processor_->SetCurrentUptimeForTesting(last_uptime - 1);

  events::v2::cr_os_events::Test1 test_event;
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(last_uptime));
  processor_->OnEventsRecord(&test_event);

  EXPECT_EQ(test_event.event_sequence_metadata().reset_counter,
            reset_counter + 1);
  EXPECT_EQ(GetResetCounter(), reset_counter + 1);
  EXPECT_EQ(GetLastSystemUptime(), last_uptime - 1);
}

TEST_F(CrOSEventsProcessorTest, ResetCounterNotUpdated) {
  int64_t last_uptime = 20;
  int reset_counter = 10;

  Initialize(last_uptime, reset_counter);
  // Sets the current uptime to be greater than the last uptime to emulate a
  // scenario where reset counter should not be incremented.
  processor_->SetCurrentUptimeForTesting(last_uptime + 1);

  events::v2::cr_os_events::Test1 test_event;
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(last_uptime));
  processor_->OnEventsRecord(&test_event);

  EXPECT_EQ(test_event.event_sequence_metadata().reset_counter, reset_counter);
  EXPECT_EQ(GetResetCounter(), reset_counter);
  EXPECT_EQ(GetLastSystemUptime(), last_uptime + 1);
}

}  // namespace metrics::structured::cros_event
