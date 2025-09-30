// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/async_memory_consumer_registration.h"

#include <string_view>
#include <utility>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
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
  explicit TestAsyncMemoryConsumer(std::string_view consumer_id,
                                   MemoryConsumerTraits traits)
      : async_memory_consumer_registration_(consumer_id, traits, this) {}

  MOCK_METHOD(void, OnUpdateMemoryLimit, (), (override));
  MOCK_METHOD(void, OnReleaseMemory, (), (override));

  void ExpectOnUpdateMemoryLimitCall(RepeatingClosure quit_run_loop) {
    EXPECT_CALL(*this, OnUpdateMemoryLimit())
        .WillOnce(test::RunClosure(quit_run_loop));
  }

  void ExpectOnReleaseMemoryCall(RepeatingClosure quit_run_loop) {
    EXPECT_CALL(*this, OnReleaseMemory())
        .WillOnce(test::RunClosure(quit_run_loop));
  }

 private:
  AsyncMemoryConsumerRegistration async_memory_consumer_registration_;
};

class AsyncMemoryConsumerRegistrationTest : public testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(AsyncMemoryConsumerRegistrationTest, RegisterOnAnotherSequence) {
  TestMemoryConsumerRegistry registry;

  auto async_task_runner = ThreadPool::CreateSequencedTaskRunner({});

  SequenceBound<TestAsyncMemoryConsumer> consumer(async_task_runner, "consumer",
                                                  MemoryConsumerTraits{});

  ASSERT_TRUE(test::RunUntil([&]() { return registry.size() == 1u; }));

  {
    RunLoop run_loop;
    consumer.AsyncCall(&TestAsyncMemoryConsumer::ExpectOnUpdateMemoryLimitCall)
        .WithArgs(run_loop.QuitClosure());
    registry.NotifyUpdateMemoryLimit(22);
    run_loop.Run();
  }

  {
    RunLoop run_loop;
    consumer.AsyncCall(&TestAsyncMemoryConsumer::ExpectOnReleaseMemoryCall)
        .WithArgs(run_loop.QuitClosure());
    registry.NotifyReleaseMemory();
    run_loop.Run();
  }

  consumer.Reset();
  ASSERT_TRUE(test::RunUntil([&]() { return registry.size() == 0u; }));
}

}  // namespace base
