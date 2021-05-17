// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

class ThreadLocalTesterBase : public DelegateSimpleThreadPool::Delegate {
 public:
  typedef ThreadLocalPointer<char> TLPType;

  ThreadLocalTesterBase(TLPType* tlp, WaitableEvent* done)
      : tlp_(tlp), done_(done) {}
  ~ThreadLocalTesterBase() override = default;

 protected:
  TLPType* tlp_;
  WaitableEvent* done_;
};

class SetThreadLocal : public ThreadLocalTesterBase {
 public:
  SetThreadLocal(TLPType* tlp, WaitableEvent* done)
      : ThreadLocalTesterBase(tlp, done), val_(nullptr) {}
  ~SetThreadLocal() override = default;

  void set_value(char* val) { val_ = val; }

  void Run() override {
    DCHECK(!done_->IsSignaled());
    tlp_->Set(val_);
    done_->Signal();
  }

 private:
  char* val_;
};

class GetThreadLocal : public ThreadLocalTesterBase {
 public:
  GetThreadLocal(TLPType* tlp, WaitableEvent* done)
      : ThreadLocalTesterBase(tlp, done), ptr_(nullptr) {}
  ~GetThreadLocal() override = default;

  void set_ptr(char** ptr) { ptr_ = ptr; }

  void Run() override {
    DCHECK(!done_->IsSignaled());
    *ptr_ = tlp_->Get();
    done_->Signal();
  }

 private:
  char** ptr_;
};

}  // namespace

// In this test, we start 2 threads which will access a ThreadLocalPointer.  We
// make sure the default is NULL, and the pointers are unique to the threads.
TEST(ThreadLocalTest, Pointer) {
  DelegateSimpleThreadPool tp1("ThreadLocalTest tp1", 1);
  DelegateSimpleThreadPool tp2("ThreadLocalTest tp1", 1);
  tp1.Start();
  tp2.Start();

  ThreadLocalPointer<char> tlp;

  static char* const kBogusPointer = reinterpret_cast<char*>(0x1234);

  char* tls_val;
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);

  GetThreadLocal getter(&tlp, &done);
  getter.set_ptr(&tls_val);

  // Check that both threads defaulted to NULL.
  tls_val = kBogusPointer;
  done.Reset();
  tp1.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(static_cast<char*>(nullptr), tls_val);

  tls_val = kBogusPointer;
  done.Reset();
  tp2.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(static_cast<char*>(nullptr), tls_val);

  SetThreadLocal setter(&tlp, &done);
  setter.set_value(kBogusPointer);

  // Have thread 1 set their pointer value to kBogusPointer.
  done.Reset();
  tp1.AddWork(&setter);
  done.Wait();

  tls_val = nullptr;
  done.Reset();
  tp1.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(kBogusPointer, tls_val);

  // Make sure thread 2 is still NULL
  tls_val = kBogusPointer;
  done.Reset();
  tp2.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(static_cast<char*>(nullptr), tls_val);

  // Set thread 2 to kBogusPointer + 1.
  setter.set_value(kBogusPointer + 1);

  done.Reset();
  tp2.AddWork(&setter);
  done.Wait();

  tls_val = nullptr;
  done.Reset();
  tp2.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(kBogusPointer + 1, tls_val);

  // Make sure thread 1 is still kBogusPointer.
  tls_val = nullptr;
  done.Reset();
  tp1.AddWork(&getter);
  done.Wait();
  EXPECT_EQ(kBogusPointer, tls_val);

  tp1.JoinAll();
  tp2.JoinAll();
}

namespace {

// A simple helper which sets the given boolean to true on destruction.
class SetTrueOnDestruction {
 public:
  SetTrueOnDestruction(bool* was_destroyed) : was_destroyed_(was_destroyed) {
    CHECK(was_destroyed != nullptr);
  }
  ~SetTrueOnDestruction() {
    EXPECT_FALSE(*was_destroyed_);
    *was_destroyed_ = true;
  }

 private:
  bool* const was_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(SetTrueOnDestruction);
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
      FROM_HERE, BindLambdaForTesting([&]() {
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
  absl::optional<ThreadLocalOwnedPointer<SetTrueOnDestruction>>
      tls_owned_pointer(absl::in_place);
  bool tls_was_destroyed_other = false;

  Thread thread("TestThread");
  thread.Start();

  WaitableEvent tls_set;

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
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
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  absl::optional<ThreadLocalOwnedPointer<int>> tls_owned_pointer(
      absl::in_place);

  Thread thread("TestThread");
  thread.Start();

  WaitableEvent tls_set;

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
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

TEST(ThreadLocalTest, Boolean) {
  {
    ThreadLocalBoolean tlb;
    EXPECT_FALSE(tlb.Get());

    tlb.Set(false);
    EXPECT_FALSE(tlb.Get());

    tlb.Set(true);
    EXPECT_TRUE(tlb.Get());
  }

  // Our slot should have been freed, we're all reset.
  {
    ThreadLocalBoolean tlb;
    EXPECT_FALSE(tlb.Get());
  }
}

}  // namespace base
