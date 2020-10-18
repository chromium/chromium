// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/operations_controller.h"

#include <atomic>
#include <cstdint>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

class ScopedShutdown {
 public:
  ScopedShutdown(OperationsController* controller) : controller_(*controller) {}
  ~ScopedShutdown() { controller_.ShutdownAndWaitForZeroOperations(); }

 private:
  OperationsController& controller_;
};

TEST(OperationsControllerTest, CanBeDestroyedWithoutWaiting) {
  OperationsController controller;
}

TEST(OperationsControllerTest, CanShutdownIfNotStarted) {
  OperationsController controller;

  controller.ShutdownAndWaitForZeroOperations();
}

TEST(OperationsControllerTest, FailsToBeginWhenNotStarted) {
  OperationsController controller;

  auto operation_token = controller.TryBeginOperation();

  EXPECT_FALSE(operation_token);
}

TEST(OperationsControllerTest, CanShutdownAfterTryCallsIfNotStarted) {
  OperationsController controller;
  auto operation_token = controller.TryBeginOperation();
  ASSERT_FALSE(operation_token);

  controller.ShutdownAndWaitForZeroOperations();
}

TEST(OperationsControllerTest,
     StartAcceptingOperationsReturnsFalseIfNoRejectedBeginAttempts) {
  OperationsController controller;
  ScopedShutdown cleanup(&controller);

  EXPECT_FALSE(controller.StartAcceptingOperations());
}

TEST(OperationsControllerTest,
     StartAcceptingOperationsReturnsTrueIfFailedBeginAttempts) {
  OperationsController controller;
  ScopedShutdown cleanup(&controller);

  auto operation_token = controller.TryBeginOperation();
  ASSERT_FALSE(operation_token);

  EXPECT_TRUE(controller.StartAcceptingOperations());
}

TEST(OperationsControllerTest, SuccesfulBeginReturnsValidScopedObject) {
  OperationsController controller;
  ScopedShutdown cleanup(&controller);
  controller.StartAcceptingOperations();

  auto operation_token = controller.TryBeginOperation();

  EXPECT_TRUE(operation_token);
}

TEST(OperationsControllerTest, BeginFailsAfterShutdown) {
  OperationsController controller;
  controller.StartAcceptingOperations();

  controller.ShutdownAndWaitForZeroOperations();
  auto operation_token = controller.TryBeginOperation();

  EXPECT_FALSE(operation_token);
}

TEST(OperationsControllerTest, ScopedOperationsControllerIsMoveConstructible) {
  OperationsController controller;
  ScopedShutdown cleanup(&controller);

  controller.StartAcceptingOperations();
  auto operation_token_1 = controller.TryBeginOperation();
  auto operation_token_2 = std::move(operation_token_1);

  EXPECT_FALSE(operation_token_1);
  EXPECT_TRUE(operation_token_2);
}

// Dummy SimpleThread implementation that periodically begins and ends
// operations until one of them fails.
class TestThread : public SimpleThread {
 public:
  explicit TestThread(OperationsController* ref_controller,
                      std::atomic<bool>* started,
                      std::atomic<int32_t>* thread_counter)
      : SimpleThread("TestThread"),
        controller_(*ref_controller),
        started_(*started),
        thread_counter_(*thread_counter) {}
  void Run() override {
    thread_counter_.fetch_add(1, std::memory_order_relaxed);
    while (true) {
      PlatformThread::YieldCurrentThread();
      bool was_started = started_.load(std::memory_order_relaxed);
      std::vector<OperationsController::OperationToken> tokens;
      for (int i = 0; i < 100; ++i) {
        tokens.push_back(controller_.TryBeginOperation());
      }
      if (!was_started)
        continue;
      if (ranges::any_of(tokens, [](const auto& token) { return !token; })) {
        break;
      }
    }
  }

 private:
  OperationsController& controller_;
  std::atomic<bool>& started_;
  std::atomic<int32_t>& thread_counter_;
};

TEST(OperationsControllerTest, BeginsFromMultipleThreads) {
  constexpr int32_t kNumThreads = 10;
  for (int32_t i = 0; i < 10; ++i) {
    OperationsController ref_controller;
    std::atomic<bool> started(false);
    std::atomic<int32_t> running_threads(0);
    std::vector<std::unique_ptr<TestThread>> threads;
    for (int j = 0; j < kNumThreads; ++j) {
      threads.push_back(std::make_unique<TestThread>(&ref_controller, &started,
                                                     &running_threads));
      threads.back()->Start();
    }

    // Make sure all threads are running.
    while (running_threads.load(std::memory_order_relaxed) != kNumThreads) {
      PlatformThread::YieldCurrentThread();
    }

    // Wait a bit before starting to try to introduce races.
    constexpr TimeDelta kRaceInducingTimeout = TimeDelta::FromMicroseconds(50);
    PlatformThread::Sleep(kRaceInducingTimeout);

    ref_controller.StartAcceptingOperations();
    // Signal threads to terminate on TryBeginOperation() failures
    started.store(true, std::memory_order_relaxed);

    // Let the test run for a while before shuting down.
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(5));
    ref_controller.ShutdownAndWaitForZeroOperations();
    for (const auto& t : threads) {
      t->Join();
    }
  }
}

}  // namespace
}  // namespace internal
}  // namespace base
