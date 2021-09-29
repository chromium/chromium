// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/atomic_sequence_num.h"
#include "base/atomicops.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/aligned_memory.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::AtomicSequenceNumber constructed_seq_;
base::AtomicSequenceNumber destructed_seq_;

class ConstructAndDestructLogger {
 public:
  ConstructAndDestructLogger() {
    constructed_seq_.GetNext();
  }
  ConstructAndDestructLogger(const ConstructAndDestructLogger&) = delete;
  ConstructAndDestructLogger& operator=(const ConstructAndDestructLogger&) =
      delete;
  ~ConstructAndDestructLogger() {
    destructed_seq_.GetNext();
  }
};

class SlowConstructor {
 public:
  SlowConstructor() : some_int_(0) {
    // Sleep for 1 second to try to cause a race.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    ++constructed;
    some_int_ = 12;
  }
  SlowConstructor(const SlowConstructor&) = delete;
  SlowConstructor& operator=(const SlowConstructor&) = delete;
  int some_int() const { return some_int_; }

  static int constructed;
 private:
  int some_int_;
};

// static
int SlowConstructor::constructed = 0;

class SlowDelegate : public base::DelegateSimpleThread::Delegate {
 public:
  explicit SlowDelegate(
      base::LazyInstance<SlowConstructor>::DestructorAtExit* lazy)
      : lazy_(lazy) {}
  SlowDelegate(const SlowDelegate&) = delete;
  SlowDelegate& operator=(const SlowDelegate&) = delete;

  void Run() override {
    EXPECT_EQ(12, lazy_->Get().some_int());
    EXPECT_EQ(12, lazy_->Pointer()->some_int());
  }

 private:
  base::LazyInstance<SlowConstructor>::DestructorAtExit* lazy_;
};

}  // namespace

base::LazyInstance<ConstructAndDestructLogger>::DestructorAtExit lazy_logger =
    LAZY_INSTANCE_INITIALIZER;

TEST(LazyInstanceTest, Basic) {
  {
    base::ShadowingAtExitManager shadow;

    EXPECT_FALSE(lazy_logger.IsCreated());
    EXPECT_EQ(0, constructed_seq_.GetNext());
    EXPECT_EQ(0, destructed_seq_.GetNext());

    lazy_logger.Get();
    EXPECT_TRUE(lazy_logger.IsCreated());
    EXPECT_EQ(2, constructed_seq_.GetNext());
    EXPECT_EQ(1, destructed_seq_.GetNext());

    lazy_logger.Pointer();
    EXPECT_TRUE(lazy_logger.IsCreated());
    EXPECT_EQ(3, constructed_seq_.GetNext());
    EXPECT_EQ(2, destructed_seq_.GetNext());
  }
  EXPECT_FALSE(lazy_logger.IsCreated());
  EXPECT_EQ(4, constructed_seq_.GetNext());
  EXPECT_EQ(4, destructed_seq_.GetNext());
}

base::LazyInstance<SlowConstructor>::DestructorAtExit lazy_slow =
    LAZY_INSTANCE_INITIALIZER;

TEST(LazyInstanceTest, ConstructorThreadSafety) {
  {
    base::ShadowingAtExitManager shadow;

    SlowDelegate delegate(&lazy_slow);
    EXPECT_EQ(0, SlowConstructor::constructed);

    base::DelegateSimpleThreadPool pool("lazy_instance_cons", 5);
    pool.AddWork(&delegate, 20);
    EXPECT_EQ(0, SlowConstructor::constructed);

    pool.Start();
    pool.JoinAll();
    EXPECT_EQ(1, SlowConstructor::constructed);
  }
}

namespace {

// DeleteLogger is an object which sets a flag when it's destroyed.
// It accepts a bool* and sets the bool to true when the dtor runs.
class DeleteLogger {
 public:
  DeleteLogger() : deleted_(nullptr) {}
  ~DeleteLogger() { *deleted_ = true; }

  void SetDeletedPtr(bool* deleted) {
    deleted_ = deleted;
  }

 private:
  bool* deleted_;
};

}  // anonymous namespace

TEST(LazyInstanceTest, LeakyLazyInstance) {
  // Check that using a plain LazyInstance causes the dtor to run
  // when the AtExitManager finishes.
  bool deleted1 = false;
  {
    base::ShadowingAtExitManager shadow;
    static base::LazyInstance<DeleteLogger>::DestructorAtExit test =
        LAZY_INSTANCE_INITIALIZER;
    test.Get().SetDeletedPtr(&deleted1);
  }
  EXPECT_TRUE(deleted1);

  // Check that using a *leaky* LazyInstance makes the dtor not run
  // when the AtExitManager finishes.
  bool deleted2 = false;
  {
    base::ShadowingAtExitManager shadow;
    static base::LazyInstance<DeleteLogger>::Leaky
        test = LAZY_INSTANCE_INITIALIZER;
    test.Get().SetDeletedPtr(&deleted2);
  }
  EXPECT_FALSE(deleted2);
}

namespace {

template <size_t alignment>
class AlignedData {
 public:
  AlignedData() = default;
  ~AlignedData() = default;
  alignas(alignment) char data_[alignment];
};

}  // namespace

TEST(LazyInstanceTest, Alignment) {
  using base::LazyInstance;

  // Create some static instances with increasing sizes and alignment
  // requirements. By ordering this way, the linker will need to do some work to
  // ensure proper alignment of the static data.
  static LazyInstance<AlignedData<4>>::DestructorAtExit align4 =
      LAZY_INSTANCE_INITIALIZER;
  static LazyInstance<AlignedData<32>>::DestructorAtExit align32 =
      LAZY_INSTANCE_INITIALIZER;
  static LazyInstance<AlignedData<4096>>::DestructorAtExit align4096 =
      LAZY_INSTANCE_INITIALIZER;

  EXPECT_TRUE(base::IsAligned(align4.Pointer(), 4));
  EXPECT_TRUE(base::IsAligned(align32.Pointer(), 32));
  EXPECT_TRUE(base::IsAligned(align4096.Pointer(), 4096));
}

namespace {

// A class whose constructor busy-loops until it is told to complete
// construction.
class BlockingConstructor {
 public:
  BlockingConstructor() {
    EXPECT_FALSE(WasConstructorCalled());
    base::subtle::NoBarrier_Store(&constructor_called_, 1);
    EXPECT_TRUE(WasConstructorCalled());
    while (!base::subtle::NoBarrier_Load(&complete_construction_))
      base::PlatformThread::YieldCurrentThread();
    done_construction_ = true;
  }
  BlockingConstructor(const BlockingConstructor&) = delete;
  BlockingConstructor& operator=(const BlockingConstructor&) = delete;
  ~BlockingConstructor() {
    // Restore static state for the next test.
    base::subtle::NoBarrier_Store(&constructor_called_, 0);
    base::subtle::NoBarrier_Store(&complete_construction_, 0);
  }

  // Returns true if BlockingConstructor() was entered.
  static bool WasConstructorCalled() {
    return base::subtle::NoBarrier_Load(&constructor_called_);
  }

  // Instructs BlockingConstructor() that it may now unblock its construction.
  static void CompleteConstructionNow() {
    base::subtle::NoBarrier_Store(&complete_construction_, 1);
  }

  bool done_construction() const { return done_construction_; }

 private:
  // Use Atomic32 instead of AtomicFlag for them to be trivially initialized.
  static base::subtle::Atomic32 constructor_called_;
  static base::subtle::Atomic32 complete_construction_;

  bool done_construction_ = false;
};

// A SimpleThread running at |thread_priority| which invokes |before_get|
// (optional) and then invokes Get() on the LazyInstance it's assigned.
class BlockingConstructorThread : public base::SimpleThread {
 public:
  BlockingConstructorThread(
      base::ThreadPriority thread_priority,
      base::LazyInstance<BlockingConstructor>::DestructorAtExit* lazy,
      base::OnceClosure before_get)
      : SimpleThread("BlockingConstructorThread", Options(thread_priority)),
        lazy_(lazy),
        before_get_(std::move(before_get)) {}
  BlockingConstructorThread(const BlockingConstructorThread&) = delete;
  BlockingConstructorThread& operator=(const BlockingConstructorThread&) =
      delete;

  void Run() override {
    if (before_get_)
      std::move(before_get_).Run();
    EXPECT_TRUE(lazy_->Get().done_construction());
  }

 private:
  base::LazyInstance<BlockingConstructor>::DestructorAtExit* lazy_;
  base::OnceClosure before_get_;
};

// static
base::subtle::Atomic32 BlockingConstructor::constructor_called_ = 0;
// static
base::subtle::Atomic32 BlockingConstructor::complete_construction_ = 0;

base::LazyInstance<BlockingConstructor>::DestructorAtExit lazy_blocking =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Tests that if the thread assigned to construct the LazyInstance runs at
// background priority : the foreground threads will yield to it enough for it
// to eventually complete construction.
// This is a regression test for https://crbug.com/797129.
TEST(LazyInstanceTest, PriorityInversionAtInitializationResolves) {
  base::TimeTicks test_begin = base::TimeTicks::Now();

  // Construct BlockingConstructor from a background thread.
  BlockingConstructorThread background_getter(
      base::ThreadPriority::BACKGROUND, &lazy_blocking, base::OnceClosure());
  background_getter.Start();

  while (!BlockingConstructor::WasConstructorCalled())
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));

  // Spin 4 foreground thread per core contending to get the already under
  // construction LazyInstance. When they are all running and poking at it :
  // allow the background thread to complete its work.
  const int kNumForegroundThreads = 4 * base::SysInfo::NumberOfProcessors();
  std::vector<std::unique_ptr<base::SimpleThread>> foreground_threads;
  base::RepeatingClosure foreground_thread_ready_callback =
      base::BarrierClosure(
          kNumForegroundThreads,
          base::BindOnce(&BlockingConstructor::CompleteConstructionNow));
  for (int i = 0; i < kNumForegroundThreads; ++i) {
    foreground_threads.push_back(std::make_unique<BlockingConstructorThread>(
        base::ThreadPriority::NORMAL, &lazy_blocking,
        foreground_thread_ready_callback));
    foreground_threads.back()->Start();
  }

  // This test will hang if the foreground threads become stuck in
  // LazyInstance::Get() per the background thread never being scheduled to
  // complete construction.
  for (auto& foreground_thread : foreground_threads)
    foreground_thread->Join();
  background_getter.Join();

  // Fail if this test takes more than 5 seconds (it takes 5-10 seconds on a
  // Z840 without r527445 but is expected to be fast (~30ms) with the fix).
  EXPECT_LT(base::TimeTicks::Now() - test_begin,
            base::TimeDelta::FromSeconds(5));
}
