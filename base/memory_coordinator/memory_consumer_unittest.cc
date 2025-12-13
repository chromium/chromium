// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
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

}  // namespace base
