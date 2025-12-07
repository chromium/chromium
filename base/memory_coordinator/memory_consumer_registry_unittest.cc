// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_registry.h"

#include <optional>

#include "base/memory_coordinator/mock_memory_consumer.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using testing::_;

class MockMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  MockMemoryConsumerRegistry() = default;

  ~MockMemoryConsumerRegistry() override { NotifyDestruction(); }

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

TEST(MemoryConsumerRegistryTest, MemoryConsumerRegistration) {
  MockMemoryConsumer consumer;

  ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry> registry;

  std::optional<MemoryConsumerRegistration> registration;

  const char kObserverId[] = "observer";

  EXPECT_CALL(registry.Get(), OnMemoryConsumerAdded(kObserverId, _, _));
  registration.emplace(std::string_view(kObserverId), MemoryConsumerTraits{},
                       &consumer);

  EXPECT_CALL(registry.Get(), OnMemoryConsumerRemoved(kObserverId, _));
  registration.reset();
}

TEST(MemoryConsumerRegistryTest,
     MemoryConsumerRegistration_CheckUnregister_Fail) {
  MockMemoryConsumer consumer;

  auto registry = std::make_optional<
      ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry>>();

  std::optional<MemoryConsumerRegistration> registration;

  const char kObserverId[] = "observer";

  EXPECT_CALL(registry->Get(), OnMemoryConsumerAdded(kObserverId, _, _));
  registration.emplace(std::string_view(kObserverId), MemoryConsumerTraits{},
                       &consumer);

  EXPECT_CHECK_DEATH(registry.reset());

  EXPECT_CALL(registry->Get(), OnMemoryConsumerRemoved(kObserverId, _));
}

TEST(MemoryConsumerRegistryTest,
     MemoryConsumerRegistration_CheckUnregister_Disabled) {
  MockMemoryConsumer consumer;

  auto registry = std::make_optional<
      ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry>>();

  const char kObserverId[] = "observer";
  std::optional<MemoryConsumerRegistration> registration;

  EXPECT_CALL(registry->Get(), OnMemoryConsumerAdded(kObserverId, _, _));
  registration.emplace(std::string_view(kObserverId), MemoryConsumerTraits{},
                       &consumer,
                       MemoryConsumerRegistration::CheckUnregister::kDisabled);

  EXPECT_CALL(registry->Get(), OnMemoryConsumerRemoved(kObserverId, _));
  registry.reset();
}

}  // namespace base
