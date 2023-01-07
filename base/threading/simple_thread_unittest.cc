// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class SetIntRunner : public DelegateSimpleThread::Delegate {
 public:
  SetIntRunner(int* ptr, int val) : ptr_(ptr), val_(val) { }

  SetIntRunner(const SetIntRunner&) = delete;
  SetIntRunner& operator=(const SetIntRunner&) = delete;

  ~SetIntRunner() override = default;

 private:
  void Run() override { *ptr_ = val_; }

  raw_ptr<int> ptr_;
  int val_;
};

// Signals |started_| when Run() is invoked and waits until |released_| is
// signaled to return, signaling |done_| before doing so. Useful for tests that
// care to control Run()'s flow.
class ControlledRunner : public DelegateSimpleThread::Delegate {
 public:
  ControlledRunner()
      : started_(WaitableEvent::ResetPolicy::MANUAL,
                 WaitableEvent::InitialState::NOT_SIGNALED),
        released_(WaitableEvent::ResetPolicy::MANUAL,
                  WaitableEvent::InitialState::NOT_SIGNALED),
        done_(WaitableEvent::ResetPolicy::MANUAL,
              WaitableEvent::InitialState::NOT_SIGNALED) {}

  ControlledRunner(const ControlledRunner&) = delete;
  ControlledRunner& operator=(const ControlledRunner&) = delete;

  ~ControlledRunner() override { ReleaseAndWaitUntilDone(); }

  void WaitUntilStarted() { started_.Wait(); }

  void ReleaseAndWaitUntilDone() {
    released_.Signal();
    done_.Wait();
  }

 private:
  void Run() override {
    started_.Signal();
    released_.Wait();
    done_.Signal();
  }

  WaitableEvent started_;
  WaitableEvent released_;
  WaitableEvent done_;
};

class WaitEventRunner : public DelegateSimpleThread::Delegate {
 public:
  explicit WaitEventRunner(WaitableEvent* event) : event_(event) { }

  WaitEventRunner(const WaitEventRunner&) = delete;
  WaitEventRunner& operator=(const WaitEventRunner&) = delete;

  ~WaitEventRunner() override = default;

 private:
  void Run() override {
    EXPECT_FALSE(event_->IsSignaled());
    event_->Signal();
    EXPECT_TRUE(event_->IsSignaled());
  }

  raw_ptr<WaitableEvent> event_;
};

class SeqRunner : public DelegateSimpleThread::Delegate {
 public:
  explicit SeqRunner(AtomicSequenceNumber* seq) : seq_(seq) { }

  SeqRunner(const SeqRunner&) = delete;
  SeqRunner& operator=(const SeqRunner&) = delete;

 private:
  void Run() override { seq_->GetNext(); }

  raw_ptr<AtomicSequenceNumber> seq_;
};

// We count up on a sequence number, firing on the event when we've hit our
// expected amount, otherwise we wait on the event.  This will ensure that we
// have all threads outstanding until we hit our expected thread pool size.
class VerifyPoolRunner : public DelegateSimpleThread::Delegate {
 public:
  VerifyPoolRunner(AtomicSequenceNumber* seq,
                   int total, WaitableEvent* event)
      : seq_(seq), total_(total), event_(event) { }

  VerifyPoolRunner(const VerifyPoolRunner&) = delete;
  VerifyPoolRunner& operator=(const VerifyPoolRunner&) = delete;

 private:
  void Run() override {
    if (seq_->GetNext() == total_) {
      event_->Signal();
    } else {
      event_->Wait();
    }
  }

  raw_ptr<AtomicSequenceNumber> seq_;
  int total_;
  raw_ptr<WaitableEvent> event_;
};

}  // namespace

TEST(SimpleThreadTest, CreateAndJoin) {
  int stack_int = 0;

  SetIntRunner runner(&stack_int, 7);
  EXPECT_EQ(0, stack_int);

  DelegateSimpleThread thread(&runner, "int_setter");
  EXPECT_FALSE(thread.HasBeenStarted());
  EXPECT_FALSE(thread.HasBeenJoined());
  EXPECT_EQ(0, stack_int);

  thread.Start();
  EXPECT_TRUE(thread.HasBeenStarted());
  EXPECT_FALSE(thread.HasBeenJoined());

  thread.Join();
  EXPECT_TRUE(thread.HasBeenStarted());
  EXPECT_TRUE(thread.HasBeenJoined());
  EXPECT_EQ(7, stack_int);
}

TEST(SimpleThreadTest, WaitForEvent) {
  // Create a thread, and wait for it to signal us.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitEventRunner runner(&event);
  DelegateSimpleThread thread(&runner, "event_waiter");

  EXPECT_FALSE(event.IsSignaled());
  thread.Start();
  event.Wait();
  EXPECT_TRUE(event.IsSignaled());
  thread.Join();
}

TEST(SimpleThreadTest, NonJoinableStartAndDieOnJoin) {
  ControlledRunner runner;

  SimpleThread::Options options;
  options.joinable = false;
  DelegateSimpleThread thread(&runner, "non_joinable", options);

  EXPECT_FALSE(thread.HasBeenStarted());
  thread.Start();
  EXPECT_TRUE(thread.HasBeenStarted());

  // Note: this is not quite the same as |thread.HasBeenStarted()| which
  // represents ThreadMain() getting ready to invoke Run() whereas
  // |runner.WaitUntilStarted()| ensures Run() was actually invoked.
  runner.WaitUntilStarted();

  EXPECT_FALSE(thread.HasBeenJoined());
  EXPECT_DCHECK_DEATH({ thread.Join(); });
}

TEST(SimpleThreadTest, NonJoinableInactiveDelegateDestructionIsOkay) {
  std::unique_ptr<ControlledRunner> runner(new ControlledRunner);

  SimpleThread::Options options;
  options.joinable = false;
  std::unique_ptr<DelegateSimpleThread> thread(
      new DelegateSimpleThread(runner.get(), "non_joinable", options));

  thread->Start();
  runner->WaitUntilStarted();

  // Deleting a non-joinable SimpleThread after Run() was invoked is okay.
  thread.reset();

  runner->WaitUntilStarted();
  runner->ReleaseAndWaitUntilDone();
  // It should be safe to destroy a Delegate after its Run() method completed.
  runner.reset();
}

TEST(SimpleThreadTest, ThreadPool) {
  AtomicSequenceNumber seq;
  SeqRunner runner(&seq);
  DelegateSimpleThreadPool pool("seq_runner", 10);

  // Add work before we're running.
  pool.AddWork(&runner, 300);

  EXPECT_EQ(seq.GetNext(), 0);
  pool.Start();

  // Add work while we're running.
  pool.AddWork(&runner, 300);

  pool.JoinAll();

  EXPECT_EQ(seq.GetNext(), 601);

  // We can reuse our pool.  Verify that all 10 threads can actually run in
  // parallel, so this test will only pass if there are actually 10 threads.
  AtomicSequenceNumber seq2;
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  // Changing 9 to 10, for example, would cause us JoinAll() to never return.
  VerifyPoolRunner verifier(&seq2, 9, &event);
  pool.Start();

  pool.AddWork(&verifier, 10);

  pool.JoinAll();
  EXPECT_EQ(seq2.GetNext(), 10);
}

}  // namespace base
