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

std::unique_ptr<EventMetrics> CloneMetrics(const EventMetrics& metrics) {
  return std::make_unique<EventMetrics>(metrics);
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

using ::testing::Each;
using ::testing::Message;
using ::testing::NotNull;
using ::testing::UnorderedElementsAreArray;

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
  std::vector<EventMetrics> expected_saved_events = {
      *events[1].first,
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
              UnorderedElementsAreArray(expected_saved_events));

  // The first call to TakeSavedEventsMetrics() should remove events metrics
  // from the manager, so the second call should return empty list.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(), testing::IsEmpty());
}

// Tests that metrics for nested event loops are handled properly in a few
// different configurations.
TEST_F(EventsMetricsManagerTest, NestedEventsMetrics) {
  const std::unique_ptr<EventMetrics> events[] = {
      EventMetrics::Create(ui::ET_MOUSE_PRESSED, base::nullopt,
                           AdvanceNowByMs(1), base::nullopt),
      EventMetrics::Create(ui::ET_MOUSE_RELEASED, base::nullopt,
                           AdvanceNowByMs(1), base::nullopt),
  };
  EXPECT_THAT(events, Each(NotNull()));

  struct {
    // Metrics to use for the outer scope.
    std::unique_ptr<EventMetrics> outer_metrics;

    // Whether to save the outer scope metrics before starting the inner scope.
    bool save_outer_metrics_before_inner;

    // Metrics to use for the inner scope.
    std::unique_ptr<EventMetrics> inner_metrics;

    // Whether to save the inner scope metrics.
    bool save_inner_metrics;

    // Whether to save the outer scope metrics after the inner scope ended.
    bool save_outer_metrics_after_inner;

    // List of metrics expected to be saved.
    std::vector<EventMetrics> expected_saved_metrics;
  } configs[] = {
      // Config #0.
      {
          /*outer_metrics=*/CloneMetrics(*events[0]),
          /*save_outer_metrics_before_inner=*/true,
          /*inner_metrics=*/CloneMetrics(*events[1]),
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
          /*expected_saved_metrics=*/{*events[0], *events[1]},
      },

      // Config #1.
      {
          /*outer_metrics=*/CloneMetrics(*events[0]),
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/CloneMetrics(*events[1]),
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
          /*expected_saved_metrics=*/{*events[0], *events[1]},
      },

      // Config #2.
      {
          /*outer_metrics=*/CloneMetrics(*events[0]),
          /*save_outer_metrics_before_inner=*/true,
          /*inner_metrics=*/CloneMetrics(*events[1]),
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
          /*expected_saved_metrics=*/{*events[0], *events[1]},
      },

      // Config #3.
      {
          /*outer_metrics=*/CloneMetrics(*events[0]),
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/nullptr,
          /*save_inner_metrics=*/false,
          /*save_outer_metrics_after_inner=*/true,
          /*expected_saved_metrics=*/{*events[0]},
      },

      // Config #4.
      {
          /*outer_metrics=*/nullptr,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_metrics=*/CloneMetrics(*events[0]),
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
          /*expected_saved_metrics=*/{*events[0]},
      },
  };

  for (size_t i = 0; i < base::size(configs); i++) {
    auto& config = configs[i];
    {
      auto outer_monitor = manager_.GetScopedMonitor(
          CreateSimpleDoneCallback(std::move(config.outer_metrics)));
      if (config.save_outer_metrics_before_inner)
        manager_.SaveActiveEventMetrics();
      {
        auto inner_monitor = manager_.GetScopedMonitor(
            CreateSimpleDoneCallback(std::move(config.inner_metrics)));
        if (config.save_inner_metrics)
          manager_.SaveActiveEventMetrics();
      }
      if (config.save_outer_metrics_after_inner)
        manager_.SaveActiveEventMetrics();
    }
    SCOPED_TRACE(Message() << "Config #" << i);
    EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
                UnorderedElementsAreArray(config.expected_saved_metrics));
  }
}

}  // namespace cc
