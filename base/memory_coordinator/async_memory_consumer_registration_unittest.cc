// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/async_memory_consumer_registration.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestAsyncMemoryConsumer : public MemoryConsumer {
 public:
  explicit TestAsyncMemoryConsumer(std::string_view consumer_name,
                                   std::optional<MemoryConsumerTraits> traits)
      : async_memory_consumer_registration_(consumer_name, traits, this) {}

  MOCK_METHOD(void, OnUpdateMemoryLimit, (), (override));
  MOCK_METHOD(void, OnReleaseMemory, (), (override));

  void ExpectOnUpdateMemoryLimitCall() {
    EXPECT_CALL(*this, OnUpdateMemoryLimit());
  }

  void ExpectOnReleaseMemoryCall() { EXPECT_CALL(*this, OnReleaseMemory()); }

 private:
  AsyncMemoryConsumerRegistration async_memory_consumer_registration_;
};

class AsyncMemoryConsumerRegistrationTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(AsyncMemoryConsumerRegistrationTest, RegisterOnAnotherSequence) {
  TestMemoryConsumerRegistry registry;

  auto async_task_runner = ThreadPool::CreateSequencedTaskRunner({});

  SequenceBound<TestAsyncMemoryConsumer> consumer(async_task_runner, "consumer",
                                                  std::nullopt);

  ASSERT_TRUE(test::RunUntil([&]() { return registry.size() == 1u; }));

  {
    consumer.AsyncCall(&TestAsyncMemoryConsumer::ExpectOnUpdateMemoryLimitCall);
    registry.NotifyUpdateMemoryLimitAsync(22, task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  {
    consumer.AsyncCall(&TestAsyncMemoryConsumer::ExpectOnReleaseMemoryCall);
    registry.NotifyReleaseMemoryAsync(task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  consumer.Reset();
  ASSERT_TRUE(test::RunUntil([&]() { return registry.size() == 0u; }));
}

TEST_F(AsyncMemoryConsumerRegistrationTest, DestroyRegistryBeforeAsyncCleanup) {
  std::optional<TestMemoryConsumerRegistry> registry(std::in_place);

  auto async_task_runner = ThreadPool::CreateSequencedTaskRunner({});

  SequenceBound<TestAsyncMemoryConsumer> consumer(async_task_runner, "consumer",
                                                  std::nullopt);

  // Wait for registration.
  ASSERT_TRUE(test::RunUntil([&]() { return registry->size() == 1u; }));

  // Destroy the consumer. This sets the shared atomic flag synchronously from
  // the background thread's perspective, then posts a DeleteSoon task to the
  // main thread.
  consumer.Reset();

  // Use ThreadPoolInstance::FlushForTesting() to avoid running any tests that
  // are posted on the main thread.
  ThreadPoolInstance::Get()->FlushForTesting();

  // Immediately destroy the registry. Since the unregistration task has not
  // run yet, the destruction observer will be triggered. It should see that
  // the parent has been destroyed and skip the CHECK.
  registry.reset();
}

}  // namespace base
