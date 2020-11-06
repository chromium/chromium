// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {

MATCHER(UniquePtrMatches, negation ? "do not match" : "match") {
  return std::get<0>(arg).get() == std::get<1>(arg);
}

EventsMetricsManager::ScopedMonitor::DoneCallback CreateSimpleDoneCallback(
    std::unique_ptr<EventMetrics> metrics) {
  return base::BindOnce(
      [](std::unique_ptr<EventMetrics> metrics, bool handled) {
        std::unique_ptr<EventMetrics> result =
            handled ? std::move(metrics) : nullptr;
        return result;
      },
      std::move(metrics));
}

}  // namespace

#define EXPECT_SCOPED(statements) \
  {                               \
    SCOPED_TRACE("");             \
    statements;                   \
  }

using ::testing::IsEmpty;
using ::testing::Message;
using ::testing::UnorderedPointwise;

class EventsMetricsManagerTest : public testing::Test {
 public:
  EventsMetricsManagerTest() = default;
  ~EventsMetricsManagerTest() override = default;

 protected:
  base::TimeTicks now() const { return now_; }

  base::TimeTicks AdvanceNowByMs(int ms) {
    now_ += base::TimeDelta::FromMilliseconds(ms);
    return now_;
  }

  EventsMetricsManager manager_;
  base::TimeTicks now_ = base::TimeTicks::Now();
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
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt,
                            AdvanceNowByMs(1), base::nullopt),
       Behavior::kDoNotSave},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // inside its monitor scope.
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt,
                            AdvanceNowByMs(1), base::nullopt),
       Behavior::kSaveInsideScope},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // after its monitor scope is finished.
      {EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt,
                            AdvanceNowByMs(1), base::nullopt),
       Behavior::kSaveOutsideScope},

      // A non-interesting event type for which SaveActiveEventMetrics() is
      // called inside its monitor scope.
      {EventMetrics::Create(ui::ET_MOUSE_MOVED, base::nullopt,
                            AdvanceNowByMs(1), base::nullopt),
       Behavior::kSaveInsideScope},
  };
  EXPECT_NE(events[0].first, nullptr);
  EXPECT_NE(events[1].first, nullptr);
  EXPECT_NE(events[2].first, nullptr);
  EXPECT_EQ(events[3].first, nullptr);

  // Out of the above events, only those with an interesting event type, for
  // which SaveActiveEventMetrics() is called inside its monitor scope, are
  // expected to be saved.
  const EventMetrics* expected_saved_events[] = {
      events[1].first.get(),
  };

  for (auto& event : events) {
    {
      auto monitor = manager_.GetScopedMonitor(
          CreateSimpleDoneCallback(std::move(event.first)));
      if (event.second == Behavior::kSaveInsideScope)
        manager_.SaveActiveEventMetrics();
      // Ending the scope destroys the |monitor|.
    }
    if (event.second == Behavior::kSaveOutsideScope)
      manager_.SaveActiveEventMetrics();
  }

  // Check saved event metrics are as expected.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
              UnorderedPointwise(UniquePtrMatches(), expected_saved_events));

  // The first call to TakeSavedEventsMetrics() should remove events metrics
  // from the manager, so the second call should return empty list.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(), IsEmpty());
}

// Tests that metrics for nested event loops are handled properly in a few
// different configurations.
TEST_F(EventsMetricsManagerTest, NestedEventsMetrics) {
  struct {
    ui::EventType type;
    base::TimeTicks timestamp;
  } events[] = {
      {ui::ET_MOUSE_PRESSED, AdvanceNowByMs(1)},
      {ui::ET_MOUSE_RELEASED, AdvanceNowByMs(1)},
  };

  struct {
    // Index of event to use for the outer scope. -1 if no event should be used.
    int outer_event;

    // Whether to save the outer scope metrics before starting the inner scope.
    bool save_outer_metrics_before_inner;

    // Index of event to use for the inner scope. -1 if no event should be used.
    int inner_event;

    // Whether to save the inner scope metrics.
    bool save_inner_metrics;

    // Whether to save the outer scope metrics after the inner scope ended.
    bool save_outer_metrics_after_inner;
  } configs[] = {
      // Config #0.
      {
          /*outer_metrics=*/0,
          /*save_outer_metrics_before_inner=*/true,
          /*inner_metrics=*/1,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
      },

      // Config #1.
      {
          /*outer_metrics=*/0,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/1,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #2.
      {
          /*outer_metrics=*/0,
          /*save_outer_metrics_before_inner=*/true,
          /*inner_metrics=*/1,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #3.
      {
          /*outer_metrics=*/0,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/-1,
          /*save_inner_metrics=*/false,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #4.
      {
          /*outer_metrics=*/-1,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/0,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
      },
  };

  for (size_t i = 0; i < base::size(configs); i++) {
    auto& config = configs[i];
    std::vector<const EventMetrics*> expected_saved_metrics;

    {  // Start outer scope.
      std::unique_ptr<EventMetrics> outer_metrics;
      if (config.outer_event != -1) {
        auto& event = events[config.outer_event];
        outer_metrics = EventMetrics::Create(event.type, base::nullopt,
                                             event.timestamp, base::nullopt);
        DCHECK_NE(outer_metrics, nullptr);
        expected_saved_metrics.push_back(outer_metrics.get());
      }
      auto outer_monitor = manager_.GetScopedMonitor(
          CreateSimpleDoneCallback(std::move(outer_metrics)));
      if (config.save_outer_metrics_before_inner)
        manager_.SaveActiveEventMetrics();

      {  // Start inner scope.
        std::unique_ptr<EventMetrics> inner_metrics;
        if (config.inner_event != -1) {
          auto& event = events[config.inner_event];
          inner_metrics = EventMetrics::Create(event.type, base::nullopt,
                                               event.timestamp, base::nullopt);
          DCHECK_NE(inner_metrics, nullptr);
          expected_saved_metrics.push_back(inner_metrics.get());
        }
        auto inner_monitor = manager_.GetScopedMonitor(
            CreateSimpleDoneCallback(std::move(inner_metrics)));
        if (config.save_inner_metrics)
          manager_.SaveActiveEventMetrics();
      }  // End inner scope

      if (config.save_outer_metrics_after_inner)
        manager_.SaveActiveEventMetrics();
    }  // End outer scope.

    SCOPED_TRACE(Message() << "Config #" << i);
    EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
                UnorderedPointwise(UniquePtrMatches(), expected_saved_metrics));
  }
}

}  // namespace cc
