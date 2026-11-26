// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/operations_controller.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {
namespace {

class ScopedShutdown {
 public:
  explicit ScopedShutdown(OperationsController* controller)
      : controller_(*controller) {}
  ~ScopedShutdown() { controller_->ShutdownAndWaitForZeroOperations(); }

 private:
  const raw_ref<OperationsController> controller_;
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

TEST(OperationsControllerTest, MultipleBeginsFailAfterShutdown) {
  OperationsController controller;
  controller.StartAcceptingOperations();

  controller.ShutdownAndWaitForZeroOperations();
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(controller.TryBeginOperation());
  }
}

TEST(OperationsControllerTest,
     MultipleBeginsFailAfterShutdownWaitsForOperation) {
  OperationsController controller;
  controller.StartAcceptingOperations();

  auto token = controller.TryBeginOperation();
  ASSERT_TRUE(token);

  class ShutdownThread : public SimpleThread {
   public:
    explicit ShutdownThread(OperationsController* controller)
        : SimpleThread("ShutdownThread"), controller_(*controller) {}
    void Run() override { controller_->ShutdownAndWaitForZeroOperations(); }

   private:
    const raw_ref<OperationsController> controller_;
  };

  ShutdownThread shutdown_thread(&controller);
  shutdown_thread.Start();
  // Wait until ShutdownAndWaitForZeroOperations() has marked the controller as
  // shutting down while `token` is still active.
  while (controller.TryBeginOperation()) {
    PlatformThread::YieldCurrentThread();
  }
  {
    auto release = std::move(token);
  }
  shutdown_thread.Join();

  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(controller.TryBeginOperation());
  }
}

TEST(OperationsControllerTest,
     ShutdownFromRejectingOperationsAfterMultipleRejectedAttempts) {
  OperationsController controller;
  for (int i = 0; i < 5; ++i) {
    ASSERT_FALSE(controller.TryBeginOperation());
  }

  controller.ShutdownAndWaitForZeroOperations();
  for (int i = 0; i < 5; ++i) {
    EXPECT_FALSE(controller.TryBeginOperation());
  }
}

TEST(OperationsControllerTest,
     ShutdownFromRejectingOperationsWithConcurrentTryBegin) {
  constexpr int kNumIterations = 50;
  constexpr int kNumWorkerThreads = 2;

  struct WorkerSync {
    WaitableEvent start{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent active{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent done{WaitableEvent::ResetPolicy::AUTOMATIC};
  };

  struct SharedState {
    raw_ptr<OperationsController> controller = nullptr;
    std::array<WorkerSync, kNumWorkerThreads> workers;
    std::atomic<bool> stop_iter{false};
    std::atomic<bool> quit{false};
  };

  class TryBeginThread : public SimpleThread {
   public:
    TryBeginThread(SharedState* state, size_t index)
        : SimpleThread("TryBeginThread"), state_(*state), index_(index) {}

    void Run() override {
      WorkerSync& sync = state_->workers[index_];
      while (true) {
        sync.start.Wait();
        if (state_->quit.load(std::memory_order_acquire)) {
          return;
        }
        OperationsController* controller = state_->controller;
        sync.active.Signal();
        while (!state_->stop_iter.load(std::memory_order_acquire)) {
          EXPECT_FALSE(controller->TryBeginOperation());
          PlatformThread::YieldCurrentThread();
        }
        sync.done.Signal();
      }
    }

   private:
    const raw_ref<SharedState> state_;
    const size_t index_;
  };

  SharedState state;
  std::vector<std::unique_ptr<TryBeginThread>> threads;
  threads.reserve(kNumWorkerThreads);
  for (size_t i = 0; i < kNumWorkerThreads; ++i) {
    threads.push_back(std::make_unique<TryBeginThread>(&state, i));
    threads.back()->Start();
  }

  for (int iter = 0; iter < kNumIterations; ++iter) {
    OperationsController controller;
    if (iter % 2 == 1) {
      // Seed rejected attempts before shutdown on half the iterations so
      // ShutdownAndWaitForZeroOperations() transitions from
      // kRejectingOperations with both zero and non-zero rejected counts
      // concurrently with TryBeginOperation().
      ASSERT_FALSE(controller.TryBeginOperation());
    }
    state.controller = &controller;
    state.stop_iter.store(false, std::memory_order_relaxed);
    for (auto& worker : state.workers) {
      worker.start.Signal();
    }
    for (auto& worker : state.workers) {
      worker.active.Wait();
    }

    controller.ShutdownAndWaitForZeroOperations();

    state.stop_iter.store(true, std::memory_order_release);
    for (auto& worker : state.workers) {
      worker.done.Wait();
    }
    state.controller = nullptr;
  }

  state.quit.store(true, std::memory_order_release);
  for (auto& worker : state.workers) {
    worker.start.Signal();
  }
  for (const auto& thread : threads) {
    thread->Join();
  }
}

// Regression test for crbug.com/590269808:
// Models TaskQueueImpl::GuardedTaskPoster where:
// - Thread P1 acquires an OperationToken and drops its own scoped_refptr to the
//   holder, relying on ShutdownAndWaitForZeroOperations() to keep the holder
//   alive until ~OperationToken() completes.
// - Owner thread W calls ShutdownAndWaitForZeroOperations() and immediately
//   releases its scoped_refptr to the holder.
// - Thread P2 holds its own scoped_refptr, calls TryBeginOperation() during
//   shutdown, and immediately releases its scoped_refptr once rejected.
TEST(OperationsControllerTest,
     ConcurrentShutdownAndTryBeginDoesNotUseAfterFree) {
  class RefCountedController
      : public RefCountedThreadSafe<RefCountedController> {
   public:
    OperationsController controller;

   private:
    friend class RefCountedThreadSafe<RefCountedController>;
    ~RefCountedController() = default;
  };

  struct SharedState {
    scoped_refptr<RefCountedController> p1_holder;
    scoped_refptr<RefCountedController> p2_holder;
    WaitableEvent p1_start{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent p2_start{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent p1_acquired{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent p1_done{WaitableEvent::ResetPolicy::AUTOMATIC};
    WaitableEvent p2_done{WaitableEvent::ResetPolicy::AUTOMATIC};
    std::atomic<int> w_shutting_down_iter{0};
    std::atomic<int> p1_releasing_iter{0};
    std::atomic<bool> quit{false};
  };

  class OperationHolderThread : public SimpleThread {
   public:
    explicit OperationHolderThread(SharedState* state)
        : SimpleThread("OperationHolderThread"), state_(*state) {}

    void Run() override {
      for (int iter = 1;; ++iter) {
        state_->p1_start.Wait();
        if (state_->quit.load(std::memory_order_acquire)) {
          return;
        }

        auto holder = std::move(state_->p1_holder);
        std::optional<OperationsController::OperationToken> token(
            holder->controller.TryBeginOperation());
        ASSERT_TRUE(*token);
        // Drop our scoped_refptr while holding the OperationToken.
        holder.reset();

        state_->p1_acquired.Signal();
        while (state_->w_shutting_down_iter.load(std::memory_order_acquire) <
               iter) {
          PlatformThread::YieldCurrentThread();
        }
        state_->p1_releasing_iter.store(iter, std::memory_order_release);
        token.reset();
        state_->p1_done.Signal();
      }
    }

   private:
    const raw_ref<SharedState> state_;
  };

  class ConcurrentPosterThread : public SimpleThread {
   public:
    explicit ConcurrentPosterThread(SharedState* state)
        : SimpleThread("ConcurrentPosterThread"), state_(*state) {}

    void Run() override {
      for (int iter = 1;; ++iter) {
        state_->p2_start.Wait();
        if (state_->quit.load(std::memory_order_acquire)) {
          return;
        }

        auto holder = std::move(state_->p2_holder);
        while (state_->p1_releasing_iter.load(std::memory_order_acquire) <
               iter) {
          PlatformThread::YieldCurrentThread();
        }
        while (holder->controller.TryBeginOperation()) {
          PlatformThread::YieldCurrentThread();
        }
        holder.reset();
        state_->p2_done.Signal();
      }
    }

   private:
    const raw_ref<SharedState> state_;
  };

  constexpr int kNumIterations = 200;
  SharedState state;
  OperationHolderThread p1(&state);
  ConcurrentPosterThread p2(&state);
  p1.Start();
  p2.Start();

  for (int iter = 1; iter <= kNumIterations; ++iter) {
    auto holder = MakeRefCounted<RefCountedController>();
    holder->controller.StartAcceptingOperations();
    state.p1_holder = holder;
    state.p2_holder = holder;
    state.p1_start.Signal();
    state.p2_start.Signal();

    state.p1_acquired.Wait();

    state.w_shutting_down_iter.store(iter, std::memory_order_release);
    holder->controller.ShutdownAndWaitForZeroOperations();
    holder.reset();

    state.p1_done.Wait();
    state.p2_done.Wait();
  }

  state.quit.store(true, std::memory_order_release);
  state.p1_start.Signal();
  state.p2_start.Signal();
  p1.Join();
  p2.Join();
}

TEST(OperationsControllerTest, ScopedOperationsControllerIsMoveConstructible) {
  OperationsController controller;
  ScopedShutdown cleanup(&controller);

  controller.StartAcceptingOperations();
  auto operation_token_1 = controller.TryBeginOperation();
  auto operation_token_2 = std::move(operation_token_1);

  EXPECT_FALSE(operation_token_1);  // NOLINT(bugprone-use-after-move)
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
    thread_counter_->fetch_add(1, std::memory_order_relaxed);
    while (true) {
      PlatformThread::YieldCurrentThread();
      bool was_started = started_->load(std::memory_order_relaxed);
      std::array<std::optional<OperationsController::OperationToken>, 100>
          tokens;
      for (auto& token : tokens) {
        token.emplace(controller_->TryBeginOperation());
      }
      if (!was_started) {
        continue;
      }
      if (std::ranges::any_of(tokens,
                              [](const auto& token) { return !*token; })) {
        break;
      }
    }
  }

 private:
  const raw_ref<OperationsController> controller_;
  const raw_ref<std::atomic<bool>> started_;
  const raw_ref<std::atomic<int32_t>> thread_counter_;
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
    constexpr TimeDelta kRaceInducingTimeout = Microseconds(50);
    PlatformThread::Sleep(kRaceInducingTimeout);

    ref_controller.StartAcceptingOperations();
    // Signal threads to terminate on TryBeginOperation() failures
    started.store(true, std::memory_order_relaxed);

    // Let the test run for a while before shuting down.
    PlatformThread::Sleep(Milliseconds(5));
    ref_controller.ShutdownAndWaitForZeroOperations();
    for (const auto& t : threads) {
      t->Join();
    }
  }
}

}  // namespace
}  // namespace base::internal
