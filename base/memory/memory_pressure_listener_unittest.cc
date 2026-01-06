// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

using testing::_;

TEST(MemoryPressureListenerTest, NotifyMemoryPressure) {
  MemoryPressureListenerRegistry registry;
  RegisteredMockMemoryPressureListener listener;
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_NONE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MemoryPressureListenerTest, MemoryPressureSuppressionToken) {
  MemoryPressureListenerRegistry registry;
  RegisteredMockMemoryPressureListener listener;

  // Memory pressure notifications are not suppressed by default.
  EXPECT_FALSE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // Suppress memory pressure notifications.
  EXPECT_CALL(listener, OnMemoryPressure(_)).Times(0);
  std::optional<MemoryPressureSuppressionToken> token(std::in_place);
  EXPECT_TRUE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());

  // The level did not change.
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // Change to critical. No notifications while suppressed, but the CRITICAL
  // level will be remembered for later.
  EXPECT_CALL(listener, OnMemoryPressure(_)).Times(0);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // Can still change the memory pressure level with
  // `SimulatePressureNotification()`.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_NONE));
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_NONE);

  // Enable notifications again. The level is reverted to the last call to
  // `NotifyMemoryPressure()`.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL));
  token.reset();
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Notifications still work as expected.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);
}

TEST(MemoryPressureListenerTest, SubscribeDuringPressure) {
  MemoryPressureListenerRegistry registry;
  MockMemoryPressureListener listener;

  EXPECT_CALL(listener, OnMemoryPressure(_)).Times(0);

  // Simulate before registration.
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // When subscribing, the current memory pressure level is correctly
  // initialized on the registration object, without a `OnMemoryPressure()`
  // notification.
  MemoryPressureListenerRegistration registration(
      MemoryPressureListenerTag::kTest, &listener);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);
}

TEST(MemoryPressureListenerTest, AsyncMemoryPressureListenerRegistration) {
  MemoryPressureListenerRegistry registry;
  test::TaskEnvironment task_env;

  // Set initial pressure level.
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // The listener is initialized to MEMORY_PRESSURE_LEVEL_NONE.
  RegisteredMockAsyncMemoryPressureListener listener;
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_NONE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, DoNothing(), task_env.QuitClosure());
  task_env.RunUntilQuit();
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL));
  MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      MEMORY_PRESSURE_LEVEL_CRITICAL, task_env.QuitClosure());
  task_env.RunUntilQuit();
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
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
