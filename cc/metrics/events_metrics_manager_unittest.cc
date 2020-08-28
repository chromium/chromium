// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>
#include <vector>

#include "cc/metrics/event_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {
base::TimeTicks TimeAtMs(int ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(ms);
}
}  // namespace

class EventsMetricsManagerTest : public testing::Test {
 public:
  EventsMetricsManagerTest() = default;
  ~EventsMetricsManagerTest() override = default;

 protected:
  EventsMetricsManager manager_;
};

// Tests that EventMetrics are saved only if they have an event type we are
// interested in, and SaveActiveEventMetrics() is called inside their
// corresponding monitor's scope.
TEST_F(EventsMetricsManagerTest, EventsMetricsSaved) {
  enum class Behavior {
    kDoNotSave,
    kSaveInsideScope,
    kSaveOutsideScope,
  };

  std::pair<std::unique_ptr<EventMetrics>, Behavior> events[] = {
      // An interesting event type for which SaveActiveEventMetrics() is not
      // called.
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt, TimeAtMs(0),
                            base::nullopt),
       Behavior::kDoNotSave},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // inside its monitor scope.
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt, TimeAtMs(1),
                            base::nullopt),
       Behavior::kSaveInsideScope},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // after
      // its monitor scope is finished.
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt, TimeAtMs(2),
                            base::nullopt),
       Behavior::kSaveOutsideScope},

      // A non-interesting event type for which SaveActiveEventMetrics() is
      // called inside its monitor scope.
      {EventMetrics::Create(ui::ET_MOUSE_MOVED, base::nullopt, TimeAtMs(3),
                            base::nullopt),
       Behavior::kSaveInsideScope},
  };
  EXPECT_NE(events[0].first, nullptr);
  EXPECT_NE(events[1].first, nullptr);
  EXPECT_NE(events[2].first, nullptr);
  EXPECT_EQ(events[3].first, nullptr);

  // Out of the above events, only those with an interesting event type, for
  // which SaveActiveEventMetrics() is called inside its monitor scope, are
  // expected to be saved.
  std::vector<EventMetrics> expected_saved_events = {
      *events[1].first,
  };

  for (auto& event : events) {
    {
      auto monitor = manager_.GetScopedMonitor(std::move(event.first));
      if (event.second == Behavior::kSaveInsideScope)
        manager_.SaveActiveEventMetrics();
      // Ending the scope destroys the |monitor|.
    }
    if (event.second == Behavior::kSaveOutsideScope)
      manager_.SaveActiveEventMetrics();
  }

  // Check saved event metrics are as expected.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
              testing::ContainerEq(expected_saved_events));

  // The first call to TakeSavedEventsMetrics() should remove events metrics
  // from the manager, so the second call should return empty list.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(), testing::IsEmpty());
}

}  // namespace cc
