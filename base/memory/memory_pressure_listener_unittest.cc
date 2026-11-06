// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include <optional>

#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/mock_memory_pressure_listener.h"
#include "base/memory_coordinator/memory_pressure_source.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

using testing::_;

TEST(MemoryPressureListenerTest, NotifyMemoryPressure) {
  MemoryPressureSource source;
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
  MemoryPressureSource source;
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
  MemoryPressureSource source;
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

TEST(MemoryPressureListenerTest, SubscribeWhileSuppressed) {
  MemoryPressureSource source;
  MemoryPressureListenerRegistry registry;
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);

  std::optional<MemoryPressureSuppressionToken> token(std::in_place);
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  // A new listener is initialized with the simulated level, not the real one.
  RegisteredMockMemoryPressureListener listener;
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // Lifting the suppression notifies the real level.
  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL));
  token.reset();
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MemoryPressureListenerTest, MemoryLimit) {
  MemoryPressureSource source;
  MemoryPressureListenerRegistry registry;
  RegisteredMockMemoryPressureListener listener;

  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(listener.GetMemoryLimit(), MemoryLimit::Default());

  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.GetMemoryLimit(), MemoryLimit::FromPercent(50));

  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);

  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.GetMemoryLimit(), MemoryLimit::FromPercent(0));
}

TEST(MemoryPressureListenerTest, RepeatedNotifications) {
  MemoryPressureSource source;
  MemoryPressureListenerRegistry registry;
  MockMemoryPressureListener listener;
  MemoryPressureListenerRegistration registration(
      MemoryPressureListenerTag::kTest, &listener);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE))
      .Times(2);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL))
      .Times(2);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MemoryPressureListenerTest, IgnoreRepeatedNotifications) {
  MemoryPressureSource source;
  MemoryPressureListenerRegistry registry;
  MockMemoryPressureListener listener;
  MemoryPressureListenerRegistration registration(
      MemoryPressureListenerTag::kTest, &listener,
      /*ignore_repeated_notifications=*/true);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE))
      .Times(1);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  EXPECT_CALL(listener, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL))
      .Times(1);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MemoryPressureListenerTest, SuppressionTokenFromNonMainThread) {
  test::TaskEnvironment task_env;
  MemoryPressureSource source;
  MemoryPressureListenerRegistry registry;

  const PlatformThreadRef main_thread_ref = PlatformThread::CurrentRef();

  MockMemoryPressureListener listener;
  MemoryPressureListenerRegistration registration(
      MemoryPressureListenerTag::kTest, &listener);

  // The listener must only ever be notified on the main thread. If any part of
  // the suppression path ran on the background thread, this would fire
  // off-thread and fail the test.
  EXPECT_CALL(listener, OnMemoryPressure(_))
      .WillRepeatedly([&](MemoryPressureLevel) {
        EXPECT_TRUE(PlatformThread::CurrentRef() == main_thread_ref);
      });

  // Baseline pressure level, notified synchronously on the main thread.
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // A regular base::Thread does not register itself as the process main thread,
  // so the main-thread default handle stays pointed at the test's main thread.
  // This mimics the in-process Renderer thread that owns MemoryPurgeManager.
  Thread renderer_thread("renderer");
  ASSERT_TRUE(renderer_thread.Start());

  // The suppression token lives across two renderer-thread tasks so that the
  // process main thread can change the pressure level while suppressed.
  std::optional<MemoryPressureSuppressionToken> token;

  // Create the token on the renderer thread. This increases the suppression
  // count; the captured "simulated" level is the current MODERATE.
  renderer_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_FALSE(SingleThreadTaskRunner::GetMainThreadDefault()
                         ->BelongsToCurrentThread());
        token.emplace();
      }));
  renderer_thread.FlushForTesting();
  // Wait for the marshaled increase to run on the main thread.
  ASSERT_TRUE(test::RunUntil([]() {
    return MemoryPressureListenerRegistry::AreNotificationsSuppressed();
  }));

  // Change the level while suppressed. No notification is sent now, but the new
  // CRITICAL level is remembered and differs from the captured MODERATE.
  MemoryPressureListenerRegistry::NotifyMemoryPressure(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(listener.memory_pressure_level(), MEMORY_PRESSURE_LEVEL_MODERATE);

  // Destroy the token on the renderer thread. Lifting suppression must resend
  // the remembered CRITICAL level, and that notification must run on the main
  // thread, not on the renderer thread.
  renderer_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_FALSE(SingleThreadTaskRunner::GetMainThreadDefault()
                         ->BelongsToCurrentThread());
        token.reset();
      }));
  renderer_thread.FlushForTesting();
  // Wait for the marshaled decrease and resulting notification to run on the
  // main thread.
  ASSERT_TRUE(test::RunUntil([&listener]() {
    return listener.memory_pressure_level() == MEMORY_PRESSURE_LEVEL_CRITICAL;
  }));

  EXPECT_FALSE(MemoryPressureListenerRegistry::AreNotificationsSuppressed());
}

}  // namespace base
