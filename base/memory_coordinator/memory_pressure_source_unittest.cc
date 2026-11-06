// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_pressure_source.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory_coordinator/scoped_memory_pressure_level_override.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;

class MemoryPressureSourceTest : public testing::Test {
 protected:
  MemoryPressureSourceTest() {
    subscription_ = source_.AddCallback(
        BindLambdaForTesting([this](MemoryPressureLevel level) {
          received_levels_.push_back(level);
        }));
  }

  test::TaskEnvironment task_environment_;
  MemoryPressureSource source_;
  std::vector<MemoryPressureLevel> received_levels_;
  CallbackListSubscription subscription_;
};

}  // namespace

TEST(MemoryPressureSourceStandaloneTest, Singleton) {
  EXPECT_FALSE(MemoryPressureSource::Exists());
  EXPECT_EQ(MemoryPressureSource::MaybeGet(), nullptr);
  {
    MemoryPressureSource source;
    EXPECT_TRUE(MemoryPressureSource::Exists());
    EXPECT_EQ(&MemoryPressureSource::Get(), &source);
    EXPECT_EQ(MemoryPressureSource::MaybeGet(), &source);
  }
  EXPECT_FALSE(MemoryPressureSource::Exists());
}

TEST(MemoryPressureSourceStandaloneTest, SetLevelWithoutSource) {
  // Does nothing, but doesn't crash.
  MemoryPressureSource::SetMemoryPressureLevelFromAnyThread(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  {
    MemoryPressureSuppressionToken token;
  }
}

TEST_F(MemoryPressureSourceTest, SetMemoryPressureLevel) {
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_NONE));
}

TEST_F(MemoryPressureSourceTest, RepeatedNotifications) {
  // Repeated NONE notifications are dropped.
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_THAT(received_levels_, IsEmpty());

  // Other repeated notifications are delivered.
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL));
}

TEST_F(MemoryPressureSourceTest, NoNotificationOnSubscription) {
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);

  std::vector<MemoryPressureLevel> late_levels;
  CallbackListSubscription late_subscription =
      source_.AddCallback(BindLambdaForTesting(
          [&](MemoryPressureLevel level) { late_levels.push_back(level); }));
  EXPECT_THAT(late_levels, IsEmpty());

  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(late_levels, ElementsAre(MEMORY_PRESSURE_LEVEL_CRITICAL));
}

TEST_F(MemoryPressureSourceTest, Unsubscribe) {
  subscription_ = {};
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, IsEmpty());
}

TEST_F(MemoryPressureSourceTest, SetMemoryPressureLevelFromAnyThread) {
  Thread thread("background");
  ASSERT_TRUE(thread.Start());

  const PlatformThreadRef main_thread_ref = PlatformThread::CurrentRef();
  CallbackListSubscription subscription =
      source_.AddCallback(BindLambdaForTesting([&](MemoryPressureLevel) {
        EXPECT_TRUE(PlatformThread::CurrentRef() == main_thread_ref);
      }));

  thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&MemoryPressureSource::SetMemoryPressureLevelFromAnyThread,
               MEMORY_PRESSURE_LEVEL_CRITICAL));
  thread.FlushForTesting();
  ASSERT_TRUE(test::RunUntil([&]() { return !received_levels_.empty(); }));
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_CRITICAL));
}

TEST_F(MemoryPressureSourceTest, SuppressionToken) {
  EXPECT_FALSE(MemoryPressureSource::AreNotificationsSuppressed());
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);

  std::optional<MemoryPressureSuppressionToken> token(std::in_place);
  EXPECT_TRUE(MemoryPressureSource::AreNotificationsSuppressed());

  // No notifications while suppressed, but the level is remembered.
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE));

  // Simulated notifications are still delivered, but not if repeated.
  MemoryPressureSource::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_NONE);
  MemoryPressureSource::SimulatePressureNotification(
      MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_NONE));

  // Lifting the suppression sends the last real level.
  token.reset();
  EXPECT_FALSE(MemoryPressureSource::AreNotificationsSuppressed());
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_NONE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL));
}

TEST_F(MemoryPressureSourceTest, SuppressionTokenNoChange) {
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  {
    MemoryPressureSuppressionToken token;
  }
  // The level didn't change while suppressed. No catch-up notification.
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE));
}

TEST_F(MemoryPressureSourceTest, SuppressionTokenFromNonMainThread) {
  const PlatformThreadRef main_thread_ref = PlatformThread::CurrentRef();
  CallbackListSubscription subscription =
      source_.AddCallback(BindLambdaForTesting([&](MemoryPressureLevel) {
        EXPECT_TRUE(PlatformThread::CurrentRef() == main_thread_ref);
      }));

  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);

  // A regular base::Thread does not register itself as the process main thread.
  // This mimics the in-process renderer thread that owns MemoryPurgeManager.
  Thread renderer_thread("renderer");
  ASSERT_TRUE(renderer_thread.Start());

  std::optional<MemoryPressureSuppressionToken> token;
  renderer_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { token.emplace(); }));
  renderer_thread.FlushForTesting();
  ASSERT_TRUE(test::RunUntil(
      []() { return MemoryPressureSource::AreNotificationsSuppressed(); }));

  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE));

  // Lifting the suppression must send the catch-up notification on the main
  // thread.
  renderer_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { token.reset(); }));
  renderer_thread.FlushForTesting();
  ASSERT_TRUE(test::RunUntil([&]() { return received_levels_.size() == 2u; }));
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL));
  EXPECT_FALSE(MemoryPressureSource::AreNotificationsSuppressed());
}

TEST_F(MemoryPressureSourceTest, ScopedOverride) {
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);

  std::optional<ScopedMemoryPressureLevelOverride> level_override(
      std::in_place);
  // No notification until a level is set.
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE));

  level_override->SetLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL));

  // The real level is recorded, but not notified.
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL));

  level_override->SetLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_MODERATE));

  // Destroying the override notifies the real level.
  level_override.reset();
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_MODERATE,
                                            MEMORY_PRESSURE_LEVEL_NONE));
}

TEST_F(MemoryPressureSourceTest, ScopedOverrideClearLevel) {
  ScopedMemoryPressureLevelOverride level_override;
  level_override.SetLevel(MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_CRITICAL));

  level_override.ClearLevel();
  EXPECT_FALSE(MemoryPressureSource::AreNotificationsSuppressed());
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_NONE));

  // The real level is notified again.
  source_.SetMemoryPressureLevel(MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_THAT(received_levels_, ElementsAre(MEMORY_PRESSURE_LEVEL_CRITICAL,
                                            MEMORY_PRESSURE_LEVEL_NONE,
                                            MEMORY_PRESSURE_LEVEL_MODERATE));
}

}  // namespace base
