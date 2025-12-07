// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

class MemoryPressureListenerTest : public testing::Test {
 protected:
  void ExpectNotification(void (*notification_function)(MemoryPressureLevel),
                          MemoryPressureLevel level) {
    EXPECT_CALL(listener_, OnMemoryPressure(level)).Times(1);
    notification_function(level);
  }

  void ExpectNoNotification(void (*notification_function)(MemoryPressureLevel),
                            MemoryPressureLevel level) {
    EXPECT_CALL(listener_, OnMemoryPressure(testing::_)).Times(0);
    notification_function(level);
  }

 private:
  RegisteredMockMemoryPressureListener listener_;
};

TEST_F(MemoryPressureListenerTest, NotifyMemoryPressure) {
  // Memory pressure notifications are not suppressed by default.
  EXPECT_FALSE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());
  ExpectNotification(&MemoryPressureListenerRegistry::NotifyMemoryPressure,
                     MEMORY_PRESSURE_LEVEL_MODERATE);
  ExpectNotification(
      &MemoryPressureListenerRegistry::SimulatePressureNotification,
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // Enable suppressing memory pressure notifications.
  MemoryPressureListenerRegistry::SetNotificationsSuppressed(true);
  EXPECT_TRUE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());
  ExpectNoNotification(&MemoryPressureListenerRegistry::NotifyMemoryPressure,
                       MEMORY_PRESSURE_LEVEL_MODERATE);
  ExpectNotification(
      &MemoryPressureListenerRegistry::SimulatePressureNotification,
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // Disable suppressing memory pressure notifications.
  MemoryPressureListenerRegistry::SetNotificationsSuppressed(false);
  EXPECT_FALSE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());
  ExpectNotification(&MemoryPressureListenerRegistry::NotifyMemoryPressure,
                     MEMORY_PRESSURE_LEVEL_CRITICAL);
  ExpectNotification(
      &MemoryPressureListenerRegistry::SimulatePressureNotification,
      MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST_F(MemoryPressureListenerTest, SyncCallbackDeletesListener) {
  base::test::SingleThreadTaskEnvironment task_env;

  auto listener_to_be_deleted =
      std::make_unique<RegisteredMockAsyncMemoryPressureListener>();
  EXPECT_CALL(*listener_to_be_deleted, OnMemoryPressure(testing::_)).Times(0);

  auto deleter_listener =
      std::make_unique<RegisteredMockMemoryPressureListener>();
  EXPECT_CALL(*deleter_listener, OnMemoryPressure(testing::_)).WillOnce([&]() {
    listener_to_be_deleted.reset();
  });

  // This should trigger the sync callback in |deleter_listener|, which will
  // delete |listener_to_be_deleted|.
  MemoryPressureListener::NotifyMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace base
