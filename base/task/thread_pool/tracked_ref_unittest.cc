// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/tracked_ref.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/atomic_flag.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class ObjectWithTrackedRefs {
 public:
  ObjectWithTrackedRefs() : tracked_ref_factory_(this) {}
  ~ObjectWithTrackedRefs() { under_destruction_.Set(); }

  TrackedRef<ObjectWithTrackedRefs> GetTrackedRef() {
    return tracked_ref_factory_.GetTrackedRef();
  }

  bool under_destruction() const { return under_destruction_.IsSet(); }

 private:
  // True once ~ObjectWithTrackedRefs() has been initiated.
  AtomicFlag under_destruction_;

  TrackedRefFactory<ObjectWithTrackedRefs> tracked_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(ObjectWithTrackedRefs);
};

}  // namespace

// Test that an object with a TrackedRefFactory can be destroyed by a single
// owner but that its destruction will be blocked on the TrackedRefs being
// released.
TEST(TrackedRefTest, TrackedRefObjectDeletion) {
  Thread thread("TrackedRefTestThread");
  thread.Start();

  std::unique_ptr<ObjectWithTrackedRefs> obj =
      std::make_unique<ObjectWithTrackedRefs>();

  TimeTicks begin = TimeTicks::Now();

  thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](TrackedRef<ObjectWithTrackedRefs> obj) {
            // By the time this kicks in, the object should already be under
            // destruction, but blocked on this TrackedRef being released. This
            // is technically racy (main thread has to run |obj.reset()| and
            // this thread has to observe the side-effects before this delayed
            // task fires). If this ever flakes this expectation could be turned
            // into a while(!obj->under_destruction()); but until that's proven
            // flaky in practice, this expectation is more readable and
            // diagnosable then a hang.
            EXPECT_TRUE(obj->under_destruction());
          },
          obj->GetTrackedRef()),
      TestTimeouts::tiny_timeout());

  // This should kick off destruction but block until the above task resolves
  // and releases the TrackedRef.
  obj.reset();
  EXPECT_GE(TimeTicks::Now() - begin, TestTimeouts::tiny_timeout());
}

TEST(TrackedRefTest, ManyThreadsRacing) {
  constexpr int kNumThreads = 16;
  std::vector<std::unique_ptr<Thread>> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.push_back(std::make_unique<Thread>("TrackedRefTestThread"));
    threads.back()->StartAndWaitForTesting();
  }

  std::unique_ptr<ObjectWithTrackedRefs> obj =
      std::make_unique<ObjectWithTrackedRefs>();

  // Send a TrackedRef to each thread.
  for (auto& thread : threads) {
    thread->task_runner()->PostTask(
        FROM_HERE, BindOnce(
                       [](TrackedRef<ObjectWithTrackedRefs> obj) {
                         // Confirm it's still safe to
                         // dereference |obj| (and, bonus, that
                         // playing with TrackedRefs some more
                         // isn't problematic).
                         EXPECT_TRUE(obj->GetTrackedRef());
                       },
                       obj->GetTrackedRef()));
  }

  // Initiate destruction racily with the above tasks' execution (they will
  // crash if TrackedRefs aren't WAI).
  obj.reset();
}

// Test that instantiating and deleting a TrackedRefFactory without ever taking
// a TrackedRef on it is fine.
TEST(TrackedRefTest, NoTrackedRefs) {
  ObjectWithTrackedRefs obj;
}

namespace {
void ConsumesTrackedRef(TrackedRef<ObjectWithTrackedRefs> obj) {}
}  // namespace

// Test that destroying a TrackedRefFactory which had TrackedRefs in the past
// that are already gone is WAI.
TEST(TrackedRefTest, NoPendingTrackedRefs) {
  ObjectWithTrackedRefs obj;
  ConsumesTrackedRef(obj.GetTrackedRef());
}

TEST(TrackedRefTest, CopyAndMoveSemantics) {
  struct Foo {
    Foo() : factory(this) {}
    TrackedRefFactory<Foo> factory;
  };
  Foo foo;

  EXPECT_EQ(1, foo.factory.live_tracked_refs_.SubtleRefCountForDebug());

  {
    TrackedRef<Foo> plain = foo.factory.GetTrackedRef();
    EXPECT_EQ(2, foo.factory.live_tracked_refs_.SubtleRefCountForDebug());

    TrackedRef<Foo> copy_constructed(plain);
    EXPECT_EQ(3, foo.factory.live_tracked_refs_.SubtleRefCountForDebug());

    TrackedRef<Foo> moved_constructed(std::move(copy_constructed));
    EXPECT_EQ(3, foo.factory.live_tracked_refs_.SubtleRefCountForDebug());
  }

  EXPECT_EQ(1, foo.factory.live_tracked_refs_.SubtleRefCountForDebug());
}

}  // namespace internal
}  // namespace base
