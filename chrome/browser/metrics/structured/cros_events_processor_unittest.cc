// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/cros_events_processor.h"

#include <fstream>

#include "base/time/time.h"
#include "components/metrics/structured/structured_events.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured::cros_event {

namespace {

constexpr char kResetCounterTestPath[] = "reset_count";

class CrOSEventsProcessorTest : public testing::Test {
 protected:
  void SetUp() override {
    std::string test_path = ::testing::TempDir() + kResetCounterTestPath;

    // This is near equivalent to
    // platform2/metrics/structured/reset_counter_updater.cc
    std::ofstream is(test_path);
    is << test_reset_counter_;
    is.close();

    processor_ = std::make_unique<CrOSEventsProcessor>(test_path.c_str());
  }

  std::unique_ptr<cros_event::CrOSEventsProcessor> processor_;
  int64_t test_reset_counter_ = 45;
};

}  // namespace

TEST_F(CrOSEventsProcessorTest, ResetCounterLoadedProperly) {
  events::v2::cr_os_events::Test1 test_event;
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(100));
  processor_->OnEventsRecord(&test_event);

  EXPECT_EQ(test_event.event_sequence_metadata().reset_counter,
            test_reset_counter_);
}

}  // namespace metrics::structured::cros_event
