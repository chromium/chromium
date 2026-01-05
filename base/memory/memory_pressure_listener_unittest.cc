// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

using testing::_;

TEST(MemoryPressureListenerTest, NotifyMemoryPressure) {
  MemoryPressureListenerRegistry registry;
  RegisteredMockMemoryPressureListener listener;

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MemoryPressureListenerTest, MemoryPressureSuppressionToken) {
  MemoryPressureListenerRegistry registry;
  RegisteredMockMemoryPressureListener listener;

  // Memory pressure notifications are not suppressed by default.
  EXPECT_FALSE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // Suppress memory pressure notifications.
  EXPECT_CALL(listener, OnMemoryPressure(_)).Times(0);
  std::optional<MemoryPressureSuppressionToken> token(std::in_place);
  EXPECT_TRUE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());

  // Can still change the memory pressure level with
  // `SimulatePressureNotification()`.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_NONE));
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_NONE);

  // Enable notifications again.
  token.reset();

  // Notifications still work as expected.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
}

TEST(MemoryPressureListenerTest, SyncCallbackDeletesListener) {
  MemoryPressureListenerRegistry registry;
  test::SingleThreadTaskEnvironment task_env;

  auto listener_to_be_deleted =
      std::make_unique<RegisteredMockAsyncMemoryPressureListener>();
  EXPECT_CALL(*listener_to_be_deleted, OnMemoryPressure(_)).Times(0);

  auto deleter_listener =
      std::make_unique<RegisteredMockMemoryPressureListener>();
  EXPECT_CALL(*deleter_listener, OnMemoryPressure(_)).WillOnce([&]() {
    listener_to_be_deleted.reset();
  });

  // This should trigger the sync callback in |deleter_listener|, which will
  // delete |listener_to_be_deleted|.
  MemoryPressureListener::NotifyMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace base
