// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/multi_memory_consumer.h"

#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::_;

class MockMultiMemoryConsumer : public MultiMemoryConsumer {
 public:
  MockMultiMemoryConsumer() = default;
  ~MockMultiMemoryConsumer() override = default;

  MOCK_METHOD(void, OnReleaseMemory, (std::string_view name), (override));
  MOCK_METHOD(void,
              OnUpdateMemoryLimit,
              (std::string_view name, int limit),
              (override));
};

}  // namespace

TEST(MultiMemoryConsumerTest, MultiMemoryConsumerRegistration) {
  TestMemoryConsumerRegistry test_registry;

  MockMultiMemoryConsumer consumer;
  MultiMemoryConsumerRegistration registration(
      {{"intervention_a"}, {"intervention_b"}}, &consumer);

  // Verify initial limits.
  EXPECT_EQ(registration.GetMemoryLimit("intervention_a"), 100);
  EXPECT_EQ(registration.GetMemoryLimit("intervention_b"), 100);

  // Update limit. Both interventions are registered, so both should be updated.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit("intervention_a", 50));
  EXPECT_CALL(consumer, OnUpdateMemoryLimit("intervention_b", 50));
  test_registry.NotifyUpdateMemoryLimit(50);

  EXPECT_EQ(registration.GetMemoryLimit("intervention_a"), 50);
  EXPECT_EQ(registration.GetMemoryLimit("intervention_b"), 50);

  // Release memory.
  EXPECT_CALL(consumer, OnReleaseMemory("intervention_a"));
  EXPECT_CALL(consumer, OnReleaseMemory("intervention_b"));
  test_registry.NotifyReleaseMemory();
}

TEST(MultiMemoryConsumerTest, AsyncMultiMemoryConsumerRegistration) {
  base::test::TaskEnvironment task_environment;

  TestMemoryConsumerRegistry test_registry;

  MockMultiMemoryConsumer consumer;
  AsyncMultiMemoryConsumerRegistration registration(
      {{"intervention_a"}, {"intervention_b"}}, &consumer);

  // Wait for Init task to run on main thread.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return test_registry.size() == 2u; }));

  // Verify initial limits.
  EXPECT_EQ(registration.GetMemoryLimit("intervention_a"), 100);
  EXPECT_EQ(registration.GetMemoryLimit("intervention_b"), 100);

  // Update limit. Both interventions should be updated.
  int called_count = 0;
  EXPECT_CALL(consumer, OnUpdateMemoryLimit("intervention_a", 50))
      .WillOnce([&]() { called_count++; });
  EXPECT_CALL(consumer, OnUpdateMemoryLimit("intervention_b", 50))
      .WillOnce([&]() { called_count++; });
  test_registry.NotifyUpdateMemoryLimit(50);

  // In async case, the notification is posted back to our thread.
  ASSERT_TRUE(base::test::RunUntil([&]() { return called_count == 2; }));

  EXPECT_EQ(registration.GetMemoryLimit("intervention_a"), 50);
  EXPECT_EQ(registration.GetMemoryLimit("intervention_b"), 50);

  // Release memory.
  int released_count = 0;
  EXPECT_CALL(consumer, OnReleaseMemory("intervention_a")).WillOnce([&]() {
    released_count++;
  });
  EXPECT_CALL(consumer, OnReleaseMemory("intervention_b")).WillOnce([&]() {
    released_count++;
  });
  test_registry.NotifyReleaseMemory();

  ASSERT_TRUE(base::test::RunUntil([&]() { return released_count == 2; }));
}

TEST(MultiMemoryConsumerTest, DuplicateInterventionsCheck) {
  MockMultiMemoryConsumer consumer;
  EXPECT_CHECK_DEATH({
    MultiMemoryConsumerRegistration registration(
        {{"intervention_a"}, {"intervention_a"}}, &consumer);
  });
}

// A test registry that synchronously notifies the consumer with a non-default
// limit during registration.
class SyncNotifyingMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  SyncNotifyingMemoryConsumerRegistry() { MemoryConsumerRegistry::Set(this); }
  ~SyncNotifyingMemoryConsumerRegistry() override {
    NotifyDestruction();
    MemoryConsumerRegistry::Set(nullptr);
  }

  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             std::optional<MemoryConsumerTraits> traits,
                             RegisteredMemoryConsumer consumer) override {
    consumer.UpdateMemoryLimitNoNotification(50);
  }

  void OnMemoryConsumerRemoved(uint32_t consumer_id,
                               RegisteredMemoryConsumer consumer) override {}
};

TEST(MultiMemoryConsumerTest, NoNotificationDuringConstruction) {
  SyncNotifyingMemoryConsumerRegistry registry;
  MockMultiMemoryConsumer consumer;

  // We expect NO calls on the mock during construction.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit(_, _)).Times(0);
  EXPECT_CALL(consumer, OnReleaseMemory(_)).Times(0);

  MultiMemoryConsumerRegistration registration({{"intervention_a"}}, &consumer);

  // But the limit should still be correctly stored and queryable.
  EXPECT_EQ(registration.GetMemoryLimit("intervention_a"), 50);
}

}  // namespace base
