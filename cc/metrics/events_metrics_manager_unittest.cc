// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <array>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
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

using ::testing::IsEmpty;
using ::testing::Message;
using ::testing::Pointwise;

class EventsMetricsManagerTest : public testing::Test {
 public:
  EventsMetricsManagerTest() = default;
  ~EventsMetricsManagerTest() override = default;

 protected:
  std::unique_ptr<EventMetrics> CreateEventMetrics(ui::EventType type) {
    auto [event_time, arrived_in_browser_main_timestamp] =
        NextEventTimestamps();
    return EventMetrics::CreateForTesting(type, event_time,
                                          arrived_in_browser_main_timestamp,
                                          &test_tick_clock_, std::nullopt);
  }

  std::unique_ptr<ScrollEventMetrics> CreateScrollEventMetrics(
      ui::EventType type,
      bool is_inertial) {
    auto [event_time, arrived_in_browser_main_timestamp] =
        NextEventTimestamps();
    return ScrollEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial, event_time,
        arrived_in_browser_main_timestamp, &test_tick_clock_);
  }

  std::unique_ptr<ScrollEventMetrics> CreateScrollUpdateEventMetrics(
      ui::EventType type,
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type) {
    auto [event_time, arrived_in_browser_main_timestamp] =
        NextEventTimestamps();
    return ScrollUpdateEventMetrics::CreateForTesting(
        type, ui::ScrollInputType::kTouchscreen, is_inertial,
        scroll_update_type,
        /* delta= */ 4.2f, event_time, arrived_in_browser_main_timestamp,
        &test_tick_clock_,
        /* trace_id= */ std::nullopt);
  }

  std::tuple<base::TimeTicks, base::TimeTicks> NextEventTimestamps() {
    test_tick_clock_.Advance(base::Microseconds(10));
    base::TimeTicks event_time = test_tick_clock_.NowTicks();
    test_tick_clock_.Advance(base::Microseconds(5));
    base::TimeTicks arrived_in_browser_main_timestamp =
        test_tick_clock_.NowTicks();
    test_tick_clock_.Advance(base::Microseconds(10));
    return {event_time, arrived_in_browser_main_timestamp};
  }

  EventsMetricsManager manager_;
  base::SimpleTestTickClock test_tick_clock_;
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

  auto events = std::to_array<
      std::pair<std::unique_ptr<EventMetrics>, Behavior>>({
      // An interesting event type for which SaveActiveEventMetrics() is not
      // called.
      {CreateEventMetrics(ui::EventType::kMousePressed), Behavior::kDoNotSave},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // inside its monitor scope.
      {CreateEventMetrics(ui::EventType::kMousePressed),
       Behavior::kSaveInsideScope},

      // An interesting event type for which SaveActiveEventMetrics() is called
      // after its monitor scope is finished.
      {CreateEventMetrics(ui::EventType::kMousePressed),
       Behavior::kSaveOutsideScope},

      // A non-interesting event type for which SaveActiveEventMetrics() is
      // called inside its monitor scope.
      {CreateEventMetrics(ui::EventType::kMouseEntered),
       Behavior::kSaveInsideScope},
  });
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
              Pointwise(UniquePtrMatches(), expected_saved_events));

  // The first call to TakeSavedEventsMetrics() should remove events metrics
  // from the manager, so the second call should return empty list.
  EXPECT_THAT(manager_.TakeSavedEventsMetrics(), IsEmpty());
}

// Tests that metrics for nested event loops are handled properly in a few
// different configurations.
TEST_F(EventsMetricsManagerTest, NestedEventsMetrics) {
  struct Configs {
    // Type of event to use for the outer scope. `ui::EventType::kUnknown` if
    // no event should be used.
    ui::EventType outer_event_type;

    // Whether to save the outer scope metrics before starting the inner scope.
    bool save_outer_metrics_before_inner;

    // Type of event to use for the inner scope. `ui::EventType::kUnknown` if
    // no event should be used.
    ui::EventType inner_event_type;

    // Whether to save the inner scope metrics.
    bool save_inner_metrics;

    // Whether to save the outer scope metrics after the inner scope ended.
    bool save_outer_metrics_after_inner;
  };
  auto configs = std::to_array<Configs>({
      // Config #0.
      {
          /*outer_event_type=*/ui::EventType::kMousePressed,
          /*save_outer_metrics_before_inner=*/true,
          /*inner_event_type=*/ui::EventType::kMouseReleased,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
      },

      // Config #1.
      {
          /*outer_event_type=*/ui::EventType::kMousePressed,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_event_type=*/ui::EventType::kMouseReleased,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #2.
      {
          /*outer_event_type=*/ui::EventType::kMousePressed,
          /*save_outer_metrics_before_inner=*/true,
          /*inner_event_type=*/ui::EventType::kMouseReleased,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #3.
      {
          /*outer_event_type=*/ui::EventType::kMousePressed,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_event_type=*/ui::EventType::kUnknown,
          /*save_inner_metrics=*/false,
          /*save_outer_metrics_after_inner=*/true,
      },

      // Config #4.
      {
          /*outer_event_type=*/ui::EventType::kUnknown,
          /*save_outer_metrics_before_inner=*/false,
          /*inner_event_type=*/ui::EventType::kMousePressed,
          /*save_inner_metrics=*/true,
          /*save_outer_metrics_after_inner=*/false,
      },
  });

  for (size_t i = 0; i < std::size(configs); i++) {
    auto& config = configs[i];
    std::vector<const EventMetrics*> expected_saved_metrics;

    {  // Start outer scope.
      std::unique_ptr<EventMetrics> outer_metrics;
      std::optional<const EventMetrics*> expected_saved_outer_metrics;
      if (config.outer_event_type != ui::EventType::kUnknown) {
        outer_metrics = CreateEventMetrics(config.outer_event_type);
        DCHECK_NE(outer_metrics, nullptr);
        expected_saved_outer_metrics = outer_metrics.get();
      }
      auto outer_monitor = manager_.GetScopedMonitor(
          CreateSimpleDoneCallback(std::move(outer_metrics)));
      if (config.save_outer_metrics_before_inner)
        manager_.SaveActiveEventMetrics();

      {  // Start inner scope.
        std::unique_ptr<EventMetrics> inner_metrics;
        if (config.inner_event_type != ui::EventType::kUnknown) {
          inner_metrics = CreateEventMetrics(config.inner_event_type);
          DCHECK_NE(inner_metrics, nullptr);
          expected_saved_metrics.push_back(inner_metrics.get());
        }
        auto inner_monitor = manager_.GetScopedMonitor(
            CreateSimpleDoneCallback(std::move(inner_metrics)));
        if (config.save_inner_metrics)
          manager_.SaveActiveEventMetrics();
      }  // End inner scope

      if (expected_saved_outer_metrics) {
        expected_saved_metrics.push_back(*expected_saved_outer_metrics);
      }

      if (config.save_outer_metrics_after_inner)
        manager_.SaveActiveEventMetrics();
    }  // End outer scope.

    SCOPED_TRACE(Message() << "Config #" << i);
    EXPECT_THAT(manager_.TakeSavedEventsMetrics(),
                Pointwise(UniquePtrMatches(), expected_saved_metrics));
  }
}

TEST_F(EventsMetricsManagerTest,
       DropSavedEventMetricsExceptScrollEndsPreservesRegularScrollEnd) {
  auto events = std::to_array<std::unique_ptr<EventMetrics>>(
      {CreateScrollUpdateEventMetrics(
           ui::EventType::kGestureScrollUpdate,
           /* is_inertial= */ false,
           ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
       CreateScrollEventMetrics(ui::EventType::kGestureScrollEnd,
                                /* is_inertial= */ false)});
  EXPECT_EQ(events[0]->type(), EventMetrics::EventType::kGestureScrollUpdate);
  EXPECT_EQ(events[1]->type(), EventMetrics::EventType::kGestureScrollEnd);

  // Out of the above events, only `kGestureScrollEnd` should be preserved. This
  // is to ensure that Chrome emits per-scroll metrics.
  const EventMetrics* scroll_end_ptr = events[1].get();

  for (auto& event : events) {
    auto monitor =
        manager_.GetScopedMonitor(CreateSimpleDoneCallback(std::move(event)));
    manager_.SaveActiveEventMetrics();
  }

  manager_.DropSavedEventMetricsExceptScrollEnds();

  // Check that only `kGestureScrollEnd` was preserved.
  auto preserved_metrics = manager_.TakeSavedEventsMetrics();
  EXPECT_THAT(preserved_metrics, testing::SizeIs(1));
  EXPECT_EQ(preserved_metrics[0].get(), scroll_end_ptr);
  EXPECT_EQ(preserved_metrics[0]->type(),
            EventMetrics::EventType::kGestureScrollEnd);
}

TEST_F(EventsMetricsManagerTest,
       DropSavedEventMetricsExceptScrollEndsPreservesInertialScrollEnd) {
  auto events = std::to_array<std::unique_ptr<EventMetrics>>(
      {CreateScrollUpdateEventMetrics(
           ui::EventType::kGestureScrollUpdate,
           /* is_inertial= */ true,
           ScrollUpdateEventMetrics::ScrollUpdateType::kContinued),
       CreateScrollEventMetrics(ui::EventType::kGestureScrollEnd,
                                /* is_inertial= */ true)});
  EXPECT_EQ(events[0]->type(),
            EventMetrics::EventType::kInertialGestureScrollUpdate);
  EXPECT_EQ(events[1]->type(),
            EventMetrics::EventType::kInertialGestureScrollEnd);

  // Out of the above events, only `kInertialGestureScrollEnd` should be
  // preserved. This is to ensure that Chrome emits per-scroll metrics.
  const EventMetrics* scroll_end_ptr = events[1].get();

  for (auto& event : events) {
    auto monitor =
        manager_.GetScopedMonitor(CreateSimpleDoneCallback(std::move(event)));
    manager_.SaveActiveEventMetrics();
  }

  manager_.DropSavedEventMetricsExceptScrollEnds();

  // Check that only `kInertialGestureScrollEnd` was preserved.
  auto preserved_metrics = manager_.TakeSavedEventsMetrics();
  EXPECT_THAT(preserved_metrics, testing::SizeIs(1));
  EXPECT_EQ(preserved_metrics[0].get(), scroll_end_ptr);
  EXPECT_EQ(preserved_metrics[0]->type(),
            EventMetrics::EventType::kInertialGestureScrollEnd);
}

}  // namespace cc
