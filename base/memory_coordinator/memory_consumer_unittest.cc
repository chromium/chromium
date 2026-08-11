// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr MemoryConsumerTraits kTestTraits(
    MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    MemoryConsumerTraits::ReleaseMemoryCost::kFreesPagesWithoutTraversal,
    MemoryConsumerTraits::InformationRetention::kLossless,
    MemoryConsumerTraits::ExecutionType::kSynchronous);

}  // namespace

TEST(MemoryConsumerTest, MemoryConsumerRegistration) {
  TestMemoryConsumerRegistry test_registry;

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("consumer", kTestTraits, &consumer);

  EXPECT_CALL(consumer, OnReleaseMemory());
  test_registry.NotifyReleaseMemory();
}

TEST(MemoryConsumerTest, UpdateMemoryLimit) {
  TestMemoryConsumerRegistry test_registry;

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("consumer", kTestTraits, &consumer);

  // Initial limit value of 100.
  EXPECT_EQ(consumer.memory_limit(), 100);
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 1.0);

  // Try a couple values.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  test_registry.NotifyUpdateMemoryLimit(20);

  EXPECT_EQ(consumer.memory_limit(), 20);
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 0.2);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  test_registry.NotifyUpdateMemoryLimit(150);

  EXPECT_EQ(consumer.memory_limit(), 150);
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 1.5);
}

TEST(MemoryConsumerTest, ScaleByMemoryLimit) {
  TestMemoryConsumerRegistry test_registry;

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("consumer", kTestTraits, &consumer);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(4);

  // Default limit is 100.
  EXPECT_EQ(ScaleByMemoryLimit(100, consumer.memory_limit()), 100);
  EXPECT_EQ(ScaleByMemoryLimit(100u, consumer.memory_limit()), 100u);

  // Test at 50%
  test_registry.NotifyUpdateMemoryLimit(50);
  EXPECT_EQ(ScaleByMemoryLimit(100, consumer.memory_limit()), 50);
  EXPECT_EQ(ScaleByMemoryLimit(100u, consumer.memory_limit()), 50u);

  // Test truncation for integer types (15% of 10 is 1.5, which truncates to 1)
  test_registry.NotifyUpdateMemoryLimit(15);
  EXPECT_EQ(ScaleByMemoryLimit(10, consumer.memory_limit()), 1);
  EXPECT_EQ(ScaleByMemoryLimit(10u, consumer.memory_limit()), 1u);

  // Test zero limit
  test_registry.NotifyUpdateMemoryLimit(0);
  EXPECT_EQ(ScaleByMemoryLimit(100, consumer.memory_limit()), 0);
  EXPECT_EQ(ScaleByMemoryLimit(100u, consumer.memory_limit()), 0u);

  // Test large limits (scaling up) and saturation
  test_registry.NotifyUpdateMemoryLimit(200);
  EXPECT_EQ(ScaleByMemoryLimit(100, consumer.memory_limit()), 200);
  EXPECT_EQ(ScaleByMemoryLimit(100u, consumer.memory_limit()), 200u);
  EXPECT_EQ(ScaleByMemoryLimit<int8_t>(100, consumer.memory_limit()), 127);
}

#if !BUILDFLAG(IS_IOS)
TEST(MemoryConsumerTest, RegistrationWithoutRegistryAllowedInTests) {
  MockMemoryConsumer consumer;
  // This would have crashed previously because the global registry is not
  // initialized and the check is enabled by default.
  MemoryConsumerRegistration registration("consumer", kTestTraits, &consumer);
  // Expecting no crash in test environment.
}
#endif

class MockPassiveMemoryConsumer : public PassiveMemoryConsumer {
 public:
  MockPassiveMemoryConsumer() = default;
  ~MockPassiveMemoryConsumer() override = default;
};

TEST(MemoryConsumerTraitsTest, ActiveConsumerWithActiveTraits) {
  TestMemoryConsumerRegistry test_registry;
  MockMemoryConsumer consumer;
  constexpr MemoryConsumerTraits kActiveTraits(
      MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
      MemoryConsumerTraits::ReleaseMemoryCost::kFreesPagesWithoutTraversal,
      MemoryConsumerTraits::InformationRetention::kLossless,
      MemoryConsumerTraits::ExecutionType::kSynchronous);
  MemoryConsumerRegistration registration("consumer", kActiveTraits, &consumer);
}

TEST(MemoryConsumerTraitsTest, PassiveConsumerWithPassiveTraits) {
  TestMemoryConsumerRegistry test_registry;
  MockPassiveMemoryConsumer consumer;
  constexpr MemoryConsumerTraits kPassiveTraits(
      MemoryConsumerTraits::ConsumerType::kPassive);
  MemoryConsumerRegistration registration("consumer", kPassiveTraits,
                                          &consumer);
}

TEST(MemoryConsumerTraitsTest, PassiveConsumerTraitsValidation) {
  // 1. Default passive traits
  constexpr MemoryConsumerTraits kDefaultPassive(
      MemoryConsumerTraits::ConsumerType::kPassive);
  EXPECT_EQ(kDefaultPassive.consumer_type,
            MemoryConsumerTraits::ConsumerType::kPassive);
  EXPECT_EQ(kDefaultPassive.supports_memory_limit,
            MemoryConsumerTraits::SupportsMemoryLimit::kYes);
  EXPECT_EQ(kDefaultPassive.in_process, MemoryConsumerTraits::InProcess::kNA);
  EXPECT_EQ(kDefaultPassive.release_gc_references,
            MemoryConsumerTraits::ReleaseGCReferences::kNo);

  // Non-customizable traits for passive should have default passive values
  EXPECT_EQ(kDefaultPassive.estimated_memory_usage,
            MemoryConsumerTraits::EstimatedMemoryUsage::kNA);
  EXPECT_EQ(kDefaultPassive.release_memory_cost,
            MemoryConsumerTraits::ReleaseMemoryCost::kNA);
  EXPECT_EQ(kDefaultPassive.information_retention,
            MemoryConsumerTraits::InformationRetention::kNA);
  EXPECT_EQ(kDefaultPassive.execution_type,
            MemoryConsumerTraits::ExecutionType::kSynchronous);
  EXPECT_EQ(kDefaultPassive.recreate_memory_cost,
            MemoryConsumerTraits::RecreateMemoryCost::kNA);
  EXPECT_EQ(kDefaultPassive.garbage_collects_v8_heap,
            MemoryConsumerTraits::GarbageCollectsV8Heap::kNo);
  EXPECT_EQ(kDefaultPassive.is_stateful,
            MemoryConsumerTraits::IsStateful::kYes);

  // 2. Custom passive traits
  constexpr MemoryConsumerTraits kCustomPassive(
      MemoryConsumerTraits::ConsumerType::kPassive,
      MemoryConsumerTraits::SupportsMemoryLimit::kNo,
      MemoryConsumerTraits::InProcess::kYes,
      MemoryConsumerTraits::ReleaseGCReferences::kYes);
  EXPECT_EQ(kCustomPassive.consumer_type,
            MemoryConsumerTraits::ConsumerType::kPassive);
  EXPECT_EQ(kCustomPassive.supports_memory_limit,
            MemoryConsumerTraits::SupportsMemoryLimit::kNo);
  EXPECT_EQ(kCustomPassive.in_process, MemoryConsumerTraits::InProcess::kYes);
  EXPECT_EQ(kCustomPassive.release_gc_references,
            MemoryConsumerTraits::ReleaseGCReferences::kYes);

  // Non-customizable traits should still be default passive values
  EXPECT_EQ(kCustomPassive.estimated_memory_usage,
            MemoryConsumerTraits::EstimatedMemoryUsage::kNA);
  EXPECT_EQ(kCustomPassive.release_memory_cost,
            MemoryConsumerTraits::ReleaseMemoryCost::kNA);
  EXPECT_EQ(kCustomPassive.information_retention,
            MemoryConsumerTraits::InformationRetention::kNA);
  EXPECT_EQ(kCustomPassive.execution_type,
            MemoryConsumerTraits::ExecutionType::kSynchronous);
  EXPECT_EQ(kCustomPassive.recreate_memory_cost,
            MemoryConsumerTraits::RecreateMemoryCost::kNA);
  EXPECT_EQ(kCustomPassive.garbage_collects_v8_heap,
            MemoryConsumerTraits::GarbageCollectsV8Heap::kNo);
  EXPECT_EQ(kCustomPassive.is_stateful, MemoryConsumerTraits::IsStateful::kYes);
}

#if defined(GTEST_HAS_DEATH_TEST)
TEST(MemoryConsumerTraitsTest, ActiveConsumerWithPassiveTraitsFails) {
  TestMemoryConsumerRegistry test_registry;
  MockMemoryConsumer consumer;
  constexpr MemoryConsumerTraits kPassiveTraits(
      MemoryConsumerTraits::ConsumerType::kPassive);
  EXPECT_CHECK_DEATH_WITH(
      {
        MemoryConsumerRegistration registration("consumer", kPassiveTraits,
                                                &consumer);
      },
      "Active MemoryConsumer registered with Passive traits");
}

#endif

}  // namespace base
