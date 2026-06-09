// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MemoryConsumerTest, MemoryConsumerRegistration) {
  TestMemoryConsumerRegistry test_registry;

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("consumer", {}, &consumer);

  EXPECT_CALL(consumer, OnReleaseMemory());
  test_registry.NotifyReleaseMemory();
}

TEST(MemoryConsumerTest, UpdateMemoryLimit) {
  TestMemoryConsumerRegistry test_registry;

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("consumer", {}, &consumer);

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
  MemoryConsumerRegistration registration("consumer", {}, &consumer);

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
  MemoryConsumerRegistration registration("consumer", {}, &consumer);
  // Expecting no crash in test environment.
}
#endif

}  // namespace base
