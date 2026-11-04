// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr MemoryConsumerTraits kTestTraits(
    MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
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
  EXPECT_EQ(consumer.memory_limit(), MemoryLimit::Default());
  EXPECT_EQ(consumer.memory_limit().percent(), 100);
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 1.0);

  // Try a couple values.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  test_registry.NotifyUpdateMemoryLimit(MemoryLimit::FromPercent(20));

  EXPECT_EQ(consumer.memory_limit(), MemoryLimit::FromPercent(20));
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 0.2);

  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  test_registry.NotifyUpdateMemoryLimit(MemoryLimit::FromPercent(150));

  EXPECT_EQ(consumer.memory_limit(), MemoryLimit::FromPercent(150));
  EXPECT_DOUBLE_EQ(consumer.memory_limit_ratio(), 1.5);
}

TEST(MemoryConsumerTest, DisabledMemoryConsumerIsNotRegistered) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMemoryConsumerModeOverrides,
      {{"disabled_consumer", "disabled"}, {"stateless_consumer", "stateless"}});

  TestMemoryConsumerRegistry test_registry;
  test_registry.OnFeatureListInitialized();

  // A disabled consumer is not registered, and keeps the default memory limit.
  MockMemoryConsumer disabled_consumer;
  MemoryConsumerRegistration disabled_registration(
      "disabled_consumer", kTestTraits, &disabled_consumer);
  EXPECT_EQ(test_registry.size(), 0u);
  EXPECT_CALL(disabled_consumer, OnUpdateMemoryLimit()).Times(0);
  test_registry.NotifyUpdateMemoryLimit(MemoryLimit::FromPercent(20));
  EXPECT_EQ(disabled_consumer.memory_limit(), MemoryLimit::Default());

  // Consumers with a non-disabled override are registered normally.
  MockMemoryConsumer stateless_consumer;
  MemoryConsumerRegistration stateless_registration(
      "stateless_consumer", kTestTraits, &stateless_consumer);
  EXPECT_EQ(test_registry.size(), 1u);
}

// The "disabled" override also applies to the consumers that registered before
// OnFeatureListInitialized(): they are removed from the registry.
TEST(MemoryConsumerTest, EarlyDisabledMemoryConsumerIsRemoved) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMemoryConsumerModeOverrides,
      {{"early_consumer", "disabled"},
       {"early_stateless_consumer", "stateless"}});

  TestMemoryConsumerRegistry test_registry;

  // Registered before the overrides are applied.
  MockMemoryConsumer early_consumer;
  MemoryConsumerRegistration early_registration("early_consumer", kTestTraits,
                                                &early_consumer);
  MockMemoryConsumer early_stateless_consumer;
  MemoryConsumerRegistration early_stateless_registration(
      "early_stateless_consumer", kTestTraits, &early_stateless_consumer);

  // Registered and unregistered before the overrides are applied.
  {
    MockMemoryConsumer transient_consumer;
    MemoryConsumerRegistration transient_registration(
        "early_consumer", kTestTraits, &transient_consumer);
    EXPECT_EQ(test_registry.size(), 3u);
  }
  EXPECT_EQ(test_registry.size(), 2u);

  test_registry.OnFeatureListInitialized();

  // Only the disabled consumer is removed. The "stateless" override still
  // applies to the other early consumer.
  EXPECT_EQ(test_registry.size(), 1u);
  EXPECT_FALSE(ShouldUseStatefulMemoryPressure("early_stateless_consumer"));

  EXPECT_CALL(early_consumer, OnUpdateMemoryLimit()).Times(0);
  EXPECT_CALL(early_stateless_consumer, OnUpdateMemoryLimit());
  test_registry.NotifyUpdateMemoryLimit(MemoryLimit::FromPercent(20));
  EXPECT_EQ(early_consumer.memory_limit(), MemoryLimit::Default());

  // A new instance of the disabled consumer is not registered either.
  MockMemoryConsumer late_consumer;
  MemoryConsumerRegistration late_registration("early_consumer", kTestTraits,
                                               &late_consumer);
  EXPECT_EQ(test_registry.size(), 1u);
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
      MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
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
  EXPECT_EQ(kDefaultPassive.release_memory_data_access,
            MemoryConsumerTraits::ReleaseMemoryDataAccess::kNA);
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
  EXPECT_EQ(kDefaultPassive.recovery_behavior,
            MemoryConsumerTraits::RecoveryBehavior::kNA);
  EXPECT_EQ(kDefaultPassive.release_memory_cpu_cost,
            MemoryConsumerTraits::ReleaseMemoryCpuCost::kNA);

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
  EXPECT_EQ(kCustomPassive.release_memory_data_access,
            MemoryConsumerTraits::ReleaseMemoryDataAccess::kNA);
  EXPECT_EQ(kCustomPassive.information_retention,
            MemoryConsumerTraits::InformationRetention::kNA);
  EXPECT_EQ(kCustomPassive.execution_type,
            MemoryConsumerTraits::ExecutionType::kSynchronous);
  EXPECT_EQ(kCustomPassive.recreate_memory_cost,
            MemoryConsumerTraits::RecreateMemoryCost::kNA);
  EXPECT_EQ(kCustomPassive.garbage_collects_v8_heap,
            MemoryConsumerTraits::GarbageCollectsV8Heap::kNo);
  EXPECT_EQ(kCustomPassive.is_stateful, MemoryConsumerTraits::IsStateful::kYes);
  EXPECT_EQ(kCustomPassive.recovery_behavior,
            MemoryConsumerTraits::RecoveryBehavior::kNA);
  EXPECT_EQ(kCustomPassive.release_memory_cpu_cost,
            MemoryConsumerTraits::ReleaseMemoryCpuCost::kNA);
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
