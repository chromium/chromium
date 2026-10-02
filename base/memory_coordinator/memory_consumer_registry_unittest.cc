// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_registry.h"

#include <cstdint>
#include <optional>

#include "base/hash/hash.h"
#include "base/memory_coordinator/dummy_memory_consumer_registry.h"
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
              (uint32_t observer_id,
               std::string_view consumer_name,
               MemoryConsumerTraits traits,
               MemoryConsumer* consumer),
              (override));
  MOCK_METHOD(void,
              OnMemoryConsumerRemoved,
              (uint32_t observer_id, MemoryConsumer* consumer),
              (override));
};

constexpr MemoryConsumerTraits kTestTraits(
    MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    MemoryConsumerTraits::ReleaseMemoryCost::kFreesPagesWithoutTraversal,
    MemoryConsumerTraits::InformationRetention::kLossless,
    MemoryConsumerTraits::ExecutionType::kSynchronous);

}  // namespace

TEST(MemoryConsumerRegistryTest, AddAndRemoveMemoryConsumer) {
  MockMemoryConsumer consumer;

  MockMemoryConsumerRegistry registry;

  const char kObserverName[] = "observer";
  const uint32_t kObserverId = PersistentHash(kObserverName);

  EXPECT_CALL(registry,
              OnMemoryConsumerAdded(kObserverId, kObserverName, _, _));
  registry.AddMemoryConsumer(kObserverName, kTestTraits, &consumer);

  EXPECT_CALL(registry, OnMemoryConsumerRemoved(kObserverId, _));
  registry.RemoveMemoryConsumer(kObserverName, &consumer);
}

TEST(MemoryConsumerRegistryTest, CallbacksSuppressedDuringRegistration) {
  // A registry that synchronously pushes a limit and a release request to the
  // consumer while it is being registered.
  class SyncNotifyingRegistry : public MemoryConsumerRegistry {
   public:
    ~SyncNotifyingRegistry() override { NotifyDestruction(); }

    void OnMemoryConsumerAdded(uint32_t consumer_id,
                               std::string_view consumer_name,
                               MemoryConsumerTraits traits,
                               MemoryConsumer* consumer) override {
      NotifyUpdateMemoryLimit(consumer, MemoryLimit::FromPercent(50));
      NotifyReleaseMemory(consumer);
    }
    void OnMemoryConsumerRemoved(uint32_t consumer_id,
                                 MemoryConsumer* consumer) override {}

    void UpdateLimit(MemoryConsumer* consumer, MemoryLimit memory_limit) {
      NotifyUpdateMemoryLimit(consumer, memory_limit);
    }
    void Release(MemoryConsumer* consumer) { NotifyReleaseMemory(consumer); }
  };

  SyncNotifyingRegistry registry;
  MockMemoryConsumer consumer;

  // No callbacks during registration, but the limit is applied.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit()).Times(0);
  EXPECT_CALL(consumer, OnReleaseMemory()).Times(0);
  registry.AddMemoryConsumer("observer", kTestTraits, &consumer);
  EXPECT_EQ(consumer.memory_limit(), MemoryLimit::FromPercent(50));
  testing::Mock::VerifyAndClearExpectations(&consumer);

  // Callbacks are delivered normally once registration is complete.
  EXPECT_CALL(consumer, OnUpdateMemoryLimit());
  registry.UpdateLimit(&consumer, MemoryLimit::FromPercent(25));
  EXPECT_EQ(consumer.memory_limit(), MemoryLimit::FromPercent(25));
  EXPECT_CALL(consumer, OnReleaseMemory());
  registry.Release(&consumer);

  registry.RemoveMemoryConsumer("observer", &consumer);
}

TEST(MemoryConsumerRegistryTest, MemoryConsumerRegistration) {
  MockMemoryConsumer consumer;

  ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry> registry;

  std::optional<MemoryConsumerRegistration> registration;

  const char kObserverName[] = "observer";
  const uint32_t kObserverId = PersistentHash(kObserverName);

  EXPECT_CALL(registry.Get(),
              OnMemoryConsumerAdded(kObserverId, kObserverName, _, _));
  registration.emplace(std::string_view(kObserverName), kTestTraits, &consumer);

  EXPECT_CALL(registry.Get(), OnMemoryConsumerRemoved(kObserverId, _));
  registration.reset();
}

TEST(MemoryConsumerRegistryTest,
     MemoryConsumerRegistration_CheckUnregister_Fail) {
  MockMemoryConsumer consumer;

  auto registry = std::make_optional<
      ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry>>();

  std::optional<MemoryConsumerRegistration> registration;

  const char kObserverName[] = "observer";
  const uint32_t kObserverId = PersistentHash(kObserverName);

  EXPECT_CALL(registry->Get(),
              OnMemoryConsumerAdded(kObserverId, kObserverName, _, _));
  registration.emplace(std::string_view(kObserverName), kTestTraits, &consumer);

  EXPECT_CHECK_DEATH(registry.reset());

  EXPECT_CALL(registry->Get(), OnMemoryConsumerRemoved(kObserverId, _));
}

TEST(MemoryConsumerRegistryTest,
     MemoryConsumerRegistration_CheckUnregister_Disabled) {
  MockMemoryConsumer consumer;

  auto registry = std::make_optional<
      ScopedMemoryConsumerRegistry<MockMemoryConsumerRegistry>>();

  const char kObserverName[] = "observer";
  const uint32_t kObserverId = PersistentHash(kObserverName);
  std::optional<MemoryConsumerRegistration> registration;

  EXPECT_CALL(registry->Get(),
              OnMemoryConsumerAdded(kObserverId, kObserverName, _, _));
  registration.emplace(std::string_view(kObserverName), kTestTraits, &consumer,
                       MemoryConsumerRegistration::CheckUnregister::kDisabled);

  EXPECT_CALL(registry->Get(), OnMemoryConsumerRemoved(kObserverId, _));
  registry.reset();
}

TEST(MemoryConsumerRegistryTest, DummyMemoryConsumerRegistry) {
  ScopedMemoryConsumerRegistry<DummyMemoryConsumerRegistry> registry;
  EXPECT_TRUE(MemoryConsumerRegistry::Exists());

  MockMemoryConsumer consumer;
  MemoryConsumerRegistration registration("observer", kTestTraits, &consumer);
}

}  // namespace base
