// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

WeakPtr<int> PassThru(WeakPtr<int> ptr) {
  return ptr;
}

template <class T>
class OffThreadObjectCreator {
 public:
  static T* NewObject() {
    T* result;
    {
      Thread creator_thread("creator_thread");
      creator_thread.Start();
      creator_thread.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(OffThreadObjectCreator::CreateObject, &result));
    }
    DCHECK(result);  // We synchronized on thread destruction above.
    return result;
  }
 private:
  static void CreateObject(T** result) {
    *result = new T;
  }
};

struct Base {
  std::string member;
};
struct Derived : public Base {};

struct TargetBase {};
struct Target : public TargetBase, public SupportsWeakPtr<Target> {
  virtual ~Target() = default;
};

struct DerivedTarget : public Target {};

// A class inheriting from Target and defining a nested type called 'Base'.
// To guard against strange compilation errors.
struct DerivedTargetWithNestedBase : public Target {
  using Base = void;
};

// A struct with a virtual destructor.
struct VirtualDestructor {
  virtual ~VirtualDestructor() = default;
};

// A class inheriting from Target where Target is not the first base, and where
// the first base has a virtual method table. This creates a structure where the
// Target base is not positioned at the beginning of
// DerivedTargetMultipleInheritance.
struct DerivedTargetMultipleInheritance : public VirtualDestructor,
                                          public Target {};

struct Arrow {
  WeakPtr<Target> target;
};
struct TargetWithFactory : public Target {
  TargetWithFactory() {}
  WeakPtrFactory<Target> factory{this};
};

// Helper class to create and destroy weak pointer copies
// and delete objects on a background thread.
class BackgroundThread : public Thread {
 public:
  BackgroundThread() : Thread("owner_thread") {}

  ~BackgroundThread() override { Stop(); }

  void CreateArrowFromTarget(Arrow** arrow, Target* target) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BackgroundThread::DoCreateArrowFromTarget,
                                  arrow, target, &completion));
    completion.Wait();
  }

  void CreateArrowFromArrow(Arrow** arrow, const Arrow* other) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BackgroundThread::DoCreateArrowFromArrow,
                                  arrow, other, &completion));
    completion.Wait();
  }

  void DeleteTarget(Target* object) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundThread::DoDeleteTarget, object, &completion));
    completion.Wait();
  }

  void CopyAndAssignArrow(Arrow* object) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BackgroundThread::DoCopyAndAssignArrow,
                                  object, &completion));
    completion.Wait();
  }

  void CopyAndAssignArrowBase(Arrow* object) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BackgroundThread::DoCopyAndAssignArrowBase,
                                  object, &completion));
    completion.Wait();
  }

  void DeleteArrow(Arrow* object) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundThread::DoDeleteArrow, object, &completion));
    completion.Wait();
  }

  Target* DeRef(const Arrow* arrow) {
    WaitableEvent completion(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
    Target* result = nullptr;
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BackgroundThread::DoDeRef, arrow, &result,
                                  &completion));
    completion.Wait();
    return result;
  }

 protected:
  static void DoCreateArrowFromArrow(Arrow** arrow,
                                     const Arrow* other,
                                     WaitableEvent* completion) {
    *arrow = new Arrow;
    **arrow = *other;
    completion->Signal();
  }

  static void DoCreateArrowFromTarget(Arrow** arrow,
                                      Target* target,
                                      WaitableEvent* completion) {
    *arrow = new Arrow;
    (*arrow)->target = target->AsWeakPtr();
    completion->Signal();
  }

  static void DoDeRef(const Arrow* arrow,
                      Target** result,
                      WaitableEvent* completion) {
    *result = arrow->target.get();
    completion->Signal();
  }

  static void DoDeleteTarget(Target* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }

  static void DoCopyAndAssignArrow(Arrow* object, WaitableEvent* completion) {
    // Copy constructor.
    Arrow a = *object;
    // Assignment operator.
    *object = a;
    completion->Signal();
  }

  static void DoCopyAndAssignArrowBase(
      Arrow* object,
      WaitableEvent* completion) {
    // Copy constructor.
    WeakPtr<TargetBase> b = object->target;
    // Assignment operator.
    WeakPtr<TargetBase> c;
    c = object->target;
    completion->Signal();
  }

  static void DoDeleteArrow(Arrow* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }
};

}  // namespace

TEST(WeakPtrFactoryTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrFactoryTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_EQ(ptr.get(), ptr2.get());
}

TEST(WeakPtrFactoryTest, Move) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = factory.GetWeakPtr();
  WeakPtr<int> ptr3 = std::move(ptr2);
  EXPECT_NE(ptr.get(), ptr2.get());
  EXPECT_EQ(ptr.get(), ptr3.get());
}

TEST(WeakPtrFactoryTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_EQ(nullptr, ptr.get());
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_EQ(nullptr, ptr.get());
}

TEST(WeakPtrFactoryTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_EQ(nullptr, a.get());
  EXPECT_EQ(nullptr, b.get());
}

TEST(WeakPtrFactoryTest, MultipleStaged) {
  WeakPtr<int> a;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    {
      WeakPtr<int> b = factory.GetWeakPtr();
    }
    EXPECT_NE(nullptr, a.get());
  }
  EXPECT_EQ(nullptr, a.get());
}

TEST(WeakPtrFactoryTest, Dereference) {
  Base data;
  data.member = "123456";
  WeakPtrFactory<Base> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_EQ(data.member, (*ptr).member);
  EXPECT_EQ(data.member, ptr->member);
}

TEST(WeakPtrFactoryTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, ConstructFromNullptr) {
  WeakPtr<int> ptr = PassThru(nullptr);
  EXPECT_EQ(nullptr, ptr.get());
}

TEST(WeakPtrTest, SupportsWeakPtr) {
  Target target;
  WeakPtr<Target> ptr = target.AsWeakPtr();
  EXPECT_EQ(&target, ptr.get());
}

TEST(WeakPtrTest, DerivedTarget) {
  DerivedTarget target;
  WeakPtr<DerivedTarget> ptr = AsWeakPtr(&target);
  EXPECT_EQ(&target, ptr.get());
}

TEST(WeakPtrTest, DerivedTargetWithNestedBase) {
  DerivedTargetWithNestedBase target;
  WeakPtr<DerivedTargetWithNestedBase> ptr = AsWeakPtr(&target);
  EXPECT_EQ(&target, ptr.get());
}

TEST(WeakPtrTest, DerivedTargetMultipleInheritance) {
  DerivedTargetMultipleInheritance d;
  Target& b = d;
  EXPECT_NE(static_cast<void*>(&d), static_cast<void*>(&b));
  const WeakPtr<Target> pb = AsWeakPtr(&b);
  EXPECT_EQ(pb.get(), &b);
  const WeakPtr<DerivedTargetMultipleInheritance> pd = AsWeakPtr(&d);
  EXPECT_EQ(pd.get(), &d);
}

TEST(WeakPtrFactoryTest, BooleanTesting) {
  int data;
  WeakPtrFactory<int> factory(&data);

  WeakPtr<int> ptr_to_an_instance = factory.GetWeakPtr();
  EXPECT_TRUE(ptr_to_an_instance);
  EXPECT_FALSE(!ptr_to_an_instance);

  if (ptr_to_an_instance) {
  } else {
    ADD_FAILURE() << "Pointer to an instance should result in true.";
  }

  if (!ptr_to_an_instance) {  // check for operator!().
    ADD_FAILURE() << "Pointer to an instance should result in !x being false.";
  }

  WeakPtr<int> null_ptr;
  EXPECT_FALSE(null_ptr);
  EXPECT_TRUE(!null_ptr);

  if (null_ptr) {
    ADD_FAILURE() << "Null pointer should result in false.";
  }

  if (!null_ptr) {  // check for operator!().
  } else {
    ADD_FAILURE() << "Null pointer should result in !x being true.";
  }
}

TEST(WeakPtrFactoryTest, ComparisonToNull) {
  int data;
  WeakPtrFactory<int> factory(&data);

  WeakPtr<int> ptr_to_an_instance = factory.GetWeakPtr();
  EXPECT_NE(nullptr, ptr_to_an_instance);
  EXPECT_NE(ptr_to_an_instance, nullptr);

  WeakPtr<int> null_ptr;
  EXPECT_EQ(null_ptr, nullptr);
  EXPECT_EQ(nullptr, null_ptr);
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, ptr.get());
  EXPECT_FALSE(factory.HasWeakPtrs());

  // Test that the factory can create new weak pointers after a
  // InvalidateWeakPtrs call, and they remain valid until the next
  // InvalidateWeakPtrs call.
  WeakPtr<int> ptr2 = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr2.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, ptr2.get());
  EXPECT_FALSE(factory.HasWeakPtrs());
}

// Tests that WasInvalidated() is true only for invalidated WeakPtrs (not
// nullptr) and doesn't DCHECK (e.g. because of a dereference attempt).
TEST(WeakPtrTest, WasInvalidatedByFactoryDestruction) {
  WeakPtr<int> ptr;
  EXPECT_FALSE(ptr.WasInvalidated());

  // Test |data| destroyed. That is, the typical pattern when |data| (and its
  // associated factory) go out of scope.
  {
    int data = 0;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();

    // Verify that a live WeakPtr is not reported as Invalidated.
    EXPECT_FALSE(ptr.WasInvalidated());
  }

  // Checking validity shouldn't read beyond the stack frame.
  EXPECT_TRUE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
}

// As above, but testing InvalidateWeakPtrs().
TEST(WeakPtrTest, WasInvalidatedByInvalidateWeakPtrs) {
  int data = 0;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_FALSE(ptr.WasInvalidated());
  factory.InvalidateWeakPtrs();
  EXPECT_TRUE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
}

// A WeakPtr should not be reported as 'invalidated' if nullptr was assigned to
// it.
TEST(WeakPtrTest, WasInvalidatedWhilstNull) {
  int data = 0;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_FALSE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
  factory.InvalidateWeakPtrs();
  EXPECT_FALSE(ptr.WasInvalidated());
}

TEST(WeakPtrTest, MaybeValidOnSameSequence) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_TRUE(ptr.MaybeValid());
  factory.InvalidateWeakPtrs();
  // Since InvalidateWeakPtrs() ran on this sequence, MaybeValid() should be
  // false.
  EXPECT_FALSE(ptr.MaybeValid());
}

TEST(WeakPtrTest, MaybeValidOnOtherSequence) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_TRUE(ptr.MaybeValid());

  base::Thread other_thread("other_thread");
  other_thread.StartAndWaitForTesting();
  other_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WeakPtr<int> ptr) {
            // Check that MaybeValid() _eventually_ returns false.
            const TimeDelta timeout = TestTimeouts::tiny_timeout();
            const TimeTicks begin = TimeTicks::Now();
            while (ptr.MaybeValid() && (TimeTicks::Now() - begin) < timeout)
              PlatformThread::YieldCurrentThread();
            EXPECT_FALSE(ptr.MaybeValid());
          },
          ptr));
  factory.InvalidateWeakPtrs();
  // |other_thread|'s destructor will join, ensuring we wait for the task to be
  // run.
}

TEST(WeakPtrTest, HasWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  {
    WeakPtr<int> ptr = factory.GetWeakPtr();
    EXPECT_TRUE(factory.HasWeakPtrs());
  }
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, ObjectAndWeakPtrOnDifferentThreads) {
  // Test that it is OK to create an object that supports WeakPtr on one thread,
  // but use it on another.  This tests that we do not trip runtime checks that
  // ensure that a WeakPtr is not used by multiple threads.
  std::unique_ptr<Target> target(OffThreadObjectCreator<Target>::NewObject());
  WeakPtr<Target> weak_ptr = target->AsWeakPtr();
  EXPECT_EQ(target.get(), weak_ptr.get());
}

TEST(WeakPtrTest, WeakPtrInitiateAndUseOnDifferentThreads) {
  // Test that it is OK to create an object that has a WeakPtr member on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a WeakPtr is not used by multiple threads.
  std::unique_ptr<Arrow> arrow(OffThreadObjectCreator<Arrow>::NewObject());
  Target target;
  arrow->target = target.AsWeakPtr();
  EXPECT_EQ(&target, arrow->target.get());
}

TEST(WeakPtrTest, MoveOwnershipImplicitly) {
  // Move object ownership to another thread by releasing all weak pointers
  // on the original thread first, and then establish WeakPtr on a different
  // thread.
  BackgroundThread background;
  background.Start();

  Target* target = new Target();
  {
    WeakPtr<Target> weak_ptr = target->AsWeakPtr();
    // Main thread deletes the WeakPtr, then the thread ownership of the
    // object can be implicitly moved.
  }
  Arrow* arrow;

  // Background thread creates WeakPtr(and implicitly owns the object).
  background.CreateArrowFromTarget(&arrow, target);
  EXPECT_EQ(background.DeRef(arrow), target);

  {
    // Main thread creates another WeakPtr, but this does not trigger implicitly
    // thread ownership move.
    Arrow scoped_arrow;
    scoped_arrow.target = target->AsWeakPtr();

    // The new WeakPtr is owned by background thread.
    EXPECT_EQ(target, background.DeRef(&scoped_arrow));
  }

  // Target can only be deleted on background thread.
  background.DeleteTarget(target);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, MoveOwnershipOfUnreferencedObject) {
  BackgroundThread background;
  background.Start();

  Arrow* arrow;
  {
    Target target;
    // Background thread creates WeakPtr.
    background.CreateArrowFromTarget(&arrow, &target);

    // Bind to background thread.
    EXPECT_EQ(&target, background.DeRef(arrow));

    // Release the only WeakPtr.
    arrow->target.reset();

    // Now we should be able to create a new reference from this thread.
    arrow->target = target.AsWeakPtr();

    // Re-bind to main thread.
    EXPECT_EQ(&target, arrow->target.get());

    // And the main thread can now delete the target.
  }

  delete arrow;
}

TEST(WeakPtrTest, MoveOwnershipAfterInvalidate) {
  BackgroundThread background;
  background.Start();

  Arrow arrow;
  std::unique_ptr<TargetWithFactory> target(new TargetWithFactory);

  // Bind to main thread.
  arrow.target = target->factory.GetWeakPtr();
  EXPECT_EQ(target.get(), arrow.target.get());

  target->factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, arrow.target.get());

  arrow.target = target->factory.GetWeakPtr();
  // Re-bind to background thread.
  EXPECT_EQ(target.get(), background.DeRef(&arrow));

  // And the background thread can now delete the target.
  background.DeleteTarget(target.release());
}

TEST(WeakPtrTest, MainThreadRefOutlivesBackgroundThreadRef) {
  // Originating thread has a WeakPtr that outlives others.
  // - Main thread creates a WeakPtr
  // - Background thread creates a WeakPtr copy from the one in main thread
  // - Destruct the WeakPtr on background thread
  // - Destruct the WeakPtr on main thread
  BackgroundThread background;
  background.Start();

  Target target;
  Arrow arrow;
  arrow.target = target.AsWeakPtr();

  Arrow* arrow_copy;
  background.CreateArrowFromArrow(&arrow_copy, &arrow);
  EXPECT_EQ(arrow_copy->target.get(), &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, BackgroundThreadRefOutlivesMainThreadRef) {
  // Originating thread drops all references before another thread.
  // - Main thread creates a WeakPtr and passes copy to background thread
  // - Destruct the pointer on main thread
  // - Destruct the pointer on background thread
  BackgroundThread background;
  background.Start();

  Target target;
  Arrow* arrow_copy;
  {
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_EQ(arrow_copy->target.get(), &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, OwnerThreadDeletesObject) {
  // Originating thread invalidates WeakPtrs while its held by other thread.
  // - Main thread creates WeakPtr and passes Copy to background thread
  // - Object gets destroyed on main thread
  //   (invalidates WeakPtr on background thread)
  // - WeakPtr gets destroyed on Thread B
  BackgroundThread background;
  background.Start();
  Arrow* arrow_copy;
  {
    Target target;
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_EQ(nullptr, arrow_copy->target.get());
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, NonOwnerThreadCanCopyAndAssignWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow *arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can copy and assign arrow (as well as the WeakPtr inside).
  BackgroundThread background;
  background.Start();
  background.CopyAndAssignArrow(arrow);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, NonOwnerThreadCanCopyAndAssignWeakPtrBase) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow *arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can copy and assign arrow's WeakPtr to a base class WeakPtr.
  BackgroundThread background;
  background.Start();
  background.CopyAndAssignArrowBase(arrow);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, NonOwnerThreadCanDeleteWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow* arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can delete arrow (as well as the WeakPtr inside).
  BackgroundThread background;
  background.Start();
  background.DeleteArrow(arrow);
}

TEST(WeakPtrDeathTest, WeakPtrCopyDoesNotChangeThreadBinding) {
  // The default style "fast" does not support multi-threaded tests
  // (introduces deadlock on Linux).
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  BackgroundThread background;
  background.Start();

  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow arrow;
  arrow.target = target.AsWeakPtr();

  // Background copies the WeakPtr.
  Arrow* arrow_copy;
  background.CreateArrowFromArrow(&arrow_copy, &arrow);

  // The copy is still bound to main thread so I can deref.
  EXPECT_EQ(arrow.target.get(), arrow_copy->target.get());

  // Although background thread created the copy, it can not deref the copied
  // WeakPtr.
  ASSERT_DCHECK_DEATH(background.DeRef(arrow_copy));

  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrDeathTest, NonOwnerThreadDereferencesWeakPtrAfterReference) {
  // The default style "fast" does not support multi-threaded tests
  // (introduces deadlock on Linux).
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Main thread creates a Target object.
  Target target;

  // Main thread creates an arrow referencing the Target (so target's
  // thread ownership can not be implicitly moved).
  Arrow arrow;
  arrow.target = target.AsWeakPtr();
  arrow.target.get();

  // Background thread tries to deref target, which violates thread ownership.
  BackgroundThread background;
  background.Start();
  ASSERT_DCHECK_DEATH(background.DeRef(&arrow));
}

TEST(WeakPtrDeathTest, NonOwnerThreadDeletesWeakPtrAfterReference) {
  // The default style "fast" does not support multi-threaded tests
  // (introduces deadlock on Linux).
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  std::unique_ptr<Target> target(new Target());

  // Main thread creates an arrow referencing the Target.
  Arrow arrow;
  arrow.target = target->AsWeakPtr();

  // Background thread tries to deref target, binding it to the thread.
  BackgroundThread background;
  background.Start();
  background.DeRef(&arrow);

  // Main thread deletes Target, violating thread binding.
  ASSERT_DCHECK_DEATH(target.reset());

  // |target.reset()| died so |target| still holds the object, so we
  // must pass it to the background thread to teardown.
  background.DeleteTarget(target.release());
}

TEST(WeakPtrDeathTest, NonOwnerThreadDeletesObjectAfterReference) {
  // The default style "fast" does not support multi-threaded tests
  // (introduces deadlock on Linux).
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  std::unique_ptr<Target> target(new Target());

  // Main thread creates an arrow referencing the Target, and references it, so
  // that it becomes bound to the thread.
  Arrow arrow;
  arrow.target = target->AsWeakPtr();
  arrow.target.get();

  // Background thread tries to delete target, volating thread binding.
  BackgroundThread background;
  background.Start();
  ASSERT_DCHECK_DEATH(background.DeleteTarget(target.release()));
}

TEST(WeakPtrDeathTest, NonOwnerThreadReferencesObjectAfterDeletion) {
  // The default style "fast" does not support multi-threaded tests
  // (introduces deadlock on Linux).
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  std::unique_ptr<Target> target(new Target());

  // Main thread creates an arrow referencing the Target.
  Arrow arrow;
  arrow.target = target->AsWeakPtr();

  // Background thread tries to delete target, binding the object to the thread.
  BackgroundThread background;
  background.Start();
  background.DeleteTarget(target.release());

  // Main thread attempts to dereference the target, violating thread binding.
  ASSERT_DCHECK_DEATH(arrow.target.get());
}

}  // namespace base
