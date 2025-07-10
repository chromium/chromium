// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_registry.h"

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using testing::_;

class MockMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  MOCK_METHOD(void,
              OnMemoryConsumerAdded,
              (std::string_view observer_id,
               base::MemoryConsumerTraits traits,
               RegisteredMemoryConsumer consumer),
              (override));
  MOCK_METHOD(void,
              OnMemoryConsumerRemoved,
              (std::string_view observer_id, RegisteredMemoryConsumer consumer),
              (override));
};

}  // namespace

TEST(MemoryConsumerRegistryTest, AddAndRemoveMemoryConsumer) {
  MockMemoryConsumer consumer;

  MockMemoryConsumerRegistry registry;

  const char kObserverId[] = "observer";

  EXPECT_CALL(registry, OnMemoryConsumerAdded(kObserverId, _, _));
  registry.AddMemoryConsumer(kObserverId, {}, &consumer);

  EXPECT_CALL(registry, OnMemoryConsumerRemoved(kObserverId, _));
  registry.RemoveMemoryConsumer(kObserverId, &consumer);
}

}  // namespace base
