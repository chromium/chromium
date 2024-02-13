// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_metrics.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::holding_space_metrics {
namespace {

// Aliases.
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Ref;

// Mocks -----------------------------------------------------------------------

class MockObserver : public Observer {
 public:
  MOCK_METHOD(void,
              OnHoldingSpaceItemActionRecorded,
              (const std::vector<const HoldingSpaceItem*>& items,
               ItemAction action,
               EventSource event_source),
              (override));
  MOCK_METHOD(void,
              OnHoldingSpacePodActionRecorded,
              (PodAction action),
              (override));
};

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceMetricsTest = testing::Test;

// Verifies that observers are notified of `OnHoldingSpaceItemActionRecorded()`.
TEST_F(HoldingSpaceMetricsTest, NotifiesOnHoldingSpaceItemActionRecorded) {
  // Set up `observation`.
  NiceMock<MockObserver> observer;
  ScopedObservation observation(&observer);

  // Set expectations.
  const std::vector<const HoldingSpaceItem*> expected_items;
  const ItemAction expected_action = ItemAction::kPin;
  const EventSource expected_event_source = EventSource::kTest;
  EXPECT_CALL(observer, OnHoldingSpaceItemActionRecorded(
                            Ref(expected_items), Eq(expected_action),
                            Eq(expected_event_source)));

  // Trigger event.
  RecordItemAction(expected_items, expected_action, expected_event_source);
}

// Verifies that observers are notified of `OnHoldingSpacePodActionRecorded()`.
TEST_F(HoldingSpaceMetricsTest, NotifiesOnHoldingSpacePodActionRecorded) {
  // Set up `observation`.
  NiceMock<MockObserver> observer;
  ScopedObservation observation(&observer);

  // Set expectations.
  const PodAction expected_action = PodAction::kShowBubble;
  EXPECT_CALL(observer, OnHoldingSpacePodActionRecorded(Eq(expected_action)));

  // Trigger event.
  RecordPodAction(expected_action);
}

}  // namespace ash::holding_space_metrics
