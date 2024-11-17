// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local.h"

#include <optional>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// A simple helper which sets the given boolean to true on destruction.
class SetTrueOnDestruction {
 public:
  explicit SetTrueOnDestruction(bool* was_destroyed)
      : was_destroyed_(was_destroyed) {
    CHECK_NE(was_destroyed, nullptr);
  }

  SetTrueOnDestruction(const SetTrueOnDestruction&) = delete;
  SetTrueOnDestruction& operator=(const SetTrueOnDestruction&) = delete;

  ~SetTrueOnDestruction() {
    EXPECT_FALSE(*was_destroyed_);
    *was_destroyed_ = true;
  }

 private:
  const raw_ptr<bool> was_destroyed_;
};

}  // namespace

TEST(ThreadLocalTest, ThreadLocalOwnedPointerBasic) {
  ThreadLocalOwnedPointer<SetTrueOnDestruction> tls_owned_pointer;
  EXPECT_FALSE(tls_owned_pointer.Get());

  bool was_destroyed1 = false;
  tls_owned_pointer.Set(
      std::make_unique<SetTrueOnDestruction>(&was_destroyed1));
  EXPECT_FALSE(was_destroyed1);
  EXPECT_TRUE(tls_owned_pointer.Get());

  bool was_destroyed2 = false;
  tls_owned_pointer.Set(
      std::make_unique<SetTrueOnDestruction>(&was_destroyed2));
  EXPECT_TRUE(was_destroyed1);
  EXPECT_FALSE(was_destroyed2);
  EXPECT_TRUE(tls_owned_pointer.Get());

  tls_owned_pointer.Set(nullptr);
  EXPECT_TRUE(was_destroyed1);
  EXPECT_TRUE(was_destroyed2);
  EXPECT_FALSE(tls_owned_pointer.Get());
}

TEST(ThreadLocalTest, ThreadLocalOwnedPointerFreedOnThreadExit) {
  bool tls_was_destroyed = false;
  ThreadLocalOwnedPointer<SetTrueOnDestruction> tls_owned_pointer;

  Thread thread("TestThread");
  thread.Start();

  WaitableEvent tls_set;

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        tls_owned_pointer.Set(
            std::make_unique<SetTrueOnDestruction>(&tls_was_destroyed));
        tls_set.Signal();
      }));

  tls_set.Wait();
  EXPECT_FALSE(tls_was_destroyed);

  thread.Stop();
  EXPECT_TRUE(tls_was_destroyed);
}

TEST(ThreadLocalTest, ThreadLocalOwnedPointerCleansUpMainThreadOnDestruction) {
  std::optional<ThreadLocalOwnedPointer<SetTrueOnDestruction>>
      tls_owned_pointer(std::in_place);
  bool tls_was_destroyed_other = false;

  Thread thread("TestThread");
  thread.Start();

  WaitableEvent tls_set;

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        tls_owned_pointer->Set(
            std::make_unique<SetTrueOnDestruction>(&tls_was_destroyed_other));
        tls_set.Signal();
      }));

  tls_set.Wait();

  bool tls_was_destroyed_main = false;
  tls_owned_pointer->Set(
      std::make_unique<SetTrueOnDestruction>(&tls_was_destroyed_main));
  EXPECT_FALSE(tls_was_destroyed_other);
  EXPECT_FALSE(tls_was_destroyed_main);

  // Stopping the thread relinquishes its TLS (as in
  // ThreadLocalOwnedPointerFreedOnThreadExit).
  thread.Stop();
  EXPECT_TRUE(tls_was_destroyed_other);
  EXPECT_FALSE(tls_was_destroyed_main);

  // Deleting the ThreadLocalOwnedPointer instance on the main thread is allowed
  // iff that's the only thread with remaining storage (ref. disallowed use case
  // in ThreadLocalOwnedPointerDeathIfDestroyedWithActiveThread below). In that
  // case, the storage on the main thread is freed before releasing the TLS
  // slot.
  tls_owned_pointer.reset();
  EXPECT_TRUE(tls_was_destroyed_main);
}

TEST(ThreadLocalTest, ThreadLocalOwnedPointerDeathIfDestroyedWithActiveThread) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  std::optional<ThreadLocalOwnedPointer<int>> tls_owned_pointer(std::in_place);

  Thread thread("TestThread");
  thread.Start();

  WaitableEvent tls_set;

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        tls_owned_pointer->Set(std::make_unique<int>(1));
        tls_set.Signal();
      }));

  tls_set.Wait();

  EXPECT_DCHECK_DEATH({ tls_owned_pointer.reset(); });
}

TEST(ThreadLocalTest, ThreadLocalOwnedPointerMultiThreadedAndStaticStorage) {
  constexpr int kNumThreads = 16;

  static ThreadLocalOwnedPointer<SetTrueOnDestruction> tls_owned_pointer;

  std::array<bool, kNumThreads> were_destroyed{};

  std::array<std::unique_ptr<Thread>, kNumThreads> threads;

  for (auto& thread : threads) {
    thread = std::make_unique<Thread>("TestThread");
    thread->Start();
  }

  for (const auto& thread : threads) {
    // Waiting is unnecessary but enhances the likelihood of data races in the
    // next steps.
    thread->WaitUntilThreadStarted();
  }

  for (const bool was_destroyed : were_destroyed) {
    EXPECT_FALSE(was_destroyed);
  }

  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->task_runner()->PostTask(
        FROM_HERE,
        BindOnce(
            [](bool* was_destroyed) {
              tls_owned_pointer.Set(
                  std::make_unique<SetTrueOnDestruction>(was_destroyed));
            },
            &were_destroyed[i]));
  }

  static bool main_thread_was_destroyed = false;
  // Even when the test is run multiple times in the same process: TLS should
  // never be destroyed until static uninitialization.
  EXPECT_FALSE(main_thread_was_destroyed);

  tls_owned_pointer.Set(
      std::make_unique<SetTrueOnDestruction>(&main_thread_was_destroyed));

  for (const auto& thread : threads) {
    thread->Stop();
  }

  for (const bool was_destroyed : were_destroyed) {
    EXPECT_TRUE(was_destroyed);
  }

  // The main thread's TLS still wasn't destroyed (let the test unfold naturally
  // through static uninitialization).
  EXPECT_FALSE(main_thread_was_destroyed);
}

}  // namespace base
