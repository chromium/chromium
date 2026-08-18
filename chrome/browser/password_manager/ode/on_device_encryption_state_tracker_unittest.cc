// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/on_device_encryption_state_tracker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class TestOnDeviceEncryptionStateTracker
    : public OnDeviceEncryptionStateTracker {
 public:
  TestOnDeviceEncryptionStateTracker() = default;
  ~TestOnDeviceEncryptionStateTracker() override = default;

  using OnDeviceEncryptionStateTracker::SetState;
};

class MockObserver : public OnDeviceEncryptionStateTracker::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnDeviceEncryptionStateChanged,
              (OnDeviceEncryptionStateTracker * tracker,
               OnDeviceEncryptionState previous_state,
               OnDeviceEncryptionState new_state),
              (override));
  MOCK_METHOD(void,
              OnDeviceEncryptionStateTrackerShuttingDown,
              (OnDeviceEncryptionStateTracker * tracker),
              (override));
};

TEST(OnDeviceEncryptionStateTrackerTest, InitialStateIsNotAvailable) {
  TestOnDeviceEncryptionStateTracker tracker;
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST(OnDeviceEncryptionStateTrackerTest, NotifiesObserverOnStateChange) {
  TestOnDeviceEncryptionStateTracker tracker;
  MockObserver observer;
  tracker.AddObserver(&observer);

  // Transition from kOnDeviceEncryptionStateNotAvailable to kDeviceReady.
  EXPECT_CALL(observer,
              OnDeviceEncryptionStateChanged(
                  &tracker,
                  OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable,
                  OnDeviceEncryptionState::kDeviceReady));
  tracker.SetState(OnDeviceEncryptionState::kDeviceReady);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);

  // Transition from kDeviceReady to kDeviceNotReady.
  EXPECT_CALL(observer, OnDeviceEncryptionStateChanged(
                            &tracker, OnDeviceEncryptionState::kDeviceReady,
                            OnDeviceEncryptionState::kDeviceNotReady));
  tracker.SetState(OnDeviceEncryptionState::kDeviceNotReady);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceNotReady);

  tracker.RemoveObserver(&observer);
}

TEST(OnDeviceEncryptionStateTrackerTest, DoesNotNotifyOnDuplicateState) {
  TestOnDeviceEncryptionStateTracker tracker;
  MockObserver observer;
  tracker.AddObserver(&observer);

  // Initial state is already kOnDeviceEncryptionStateNotAvailable, setting it
  // again is a no-op.
  EXPECT_CALL(observer, OnDeviceEncryptionStateChanged).Times(0);
  tracker.SetState(
      OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  EXPECT_CALL(observer,
              OnDeviceEncryptionStateChanged(
                  &tracker,
                  OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable,
                  OnDeviceEncryptionState::kDeviceReady))
      .Times(1);
  tracker.SetState(OnDeviceEncryptionState::kDeviceReady);
  // Duplicate state should not notify.
  tracker.SetState(OnDeviceEncryptionState::kDeviceReady);

  tracker.RemoveObserver(&observer);
}

TEST(OnDeviceEncryptionStateTrackerTest, DoesNotNotifyAfterObserverRemoved) {
  TestOnDeviceEncryptionStateTracker tracker;
  MockObserver observer;
  tracker.AddObserver(&observer);
  tracker.RemoveObserver(&observer);

  EXPECT_CALL(observer, OnDeviceEncryptionStateChanged).Times(0);
  tracker.SetState(OnDeviceEncryptionState::kDeviceReady);
}

TEST(OnDeviceEncryptionStateTrackerTest, NotifiesShuttingDownOnDestruction) {
  MockObserver observer;
  {
    TestOnDeviceEncryptionStateTracker tracker;
    tracker.AddObserver(&observer);

    EXPECT_CALL(observer, OnDeviceEncryptionStateTrackerShuttingDown(&tracker));
  }
}

TEST(OnDeviceEncryptionStateTrackerTest,
     ObserverCanRemoveSelfDuringShuttingDown) {
  MockObserver observer1;
  MockObserver observer2;
  {
    TestOnDeviceEncryptionStateTracker tracker;
    tracker.AddObserver(&observer1);
    tracker.AddObserver(&observer2);

    EXPECT_CALL(observer1, OnDeviceEncryptionStateTrackerShuttingDown(&tracker))
        .WillOnce([&tracker, &observer1](OnDeviceEncryptionStateTracker*) {
          tracker.RemoveObserver(&observer1);
        });
    EXPECT_CALL(observer2, OnDeviceEncryptionStateTrackerShuttingDown(&tracker))
        .WillOnce([&tracker, &observer2](OnDeviceEncryptionStateTracker*) {
          tracker.RemoveObserver(&observer2);
        });
  }
}

}  // namespace

}  // namespace password_manager
