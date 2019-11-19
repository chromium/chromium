// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_bound.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SequenceBoundTest : public ::testing::Test {
 public:
  // Helpful values that our test classes use.
  enum Value {
    kInitialValue = 0,
    kDifferentValue = 1,

    // Values used by the Derived class.
    kDerivedCtorValue = 111,
    kDerivedDtorValue = 222,

    // Values used by the Other class.
    kOtherCtorValue = 333,
    kOtherDtorValue = 444,
  };

  void SetUp() override { task_runner_ = base::ThreadTaskRunnerHandle::Get(); }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Do-nothing base class, just so we can test assignment of derived classes.
  // It introduces a virtual destructor, so that casting derived classes to
  // Base should still use the appropriate (virtual) destructor.
  class Base {
   public:
    virtual ~Base() {}
  };

  // Handy class to set an int ptr to different values, to verify that things
  // are being run properly.
  class Derived : public Base {
   public:
    Derived(Value* ptr) : ptr_(ptr) { *ptr_ = kDerivedCtorValue; }
    ~Derived() override { *ptr_ = kDerivedDtorValue; }
    void SetValue(Value value) { *ptr_ = value; }
    Value* ptr_;
  };

  // Another base class, which sets ints to different values.
  class Other {
   public:
    Other(Value* ptr) : ptr_(ptr) { *ptr = kOtherCtorValue; }
    virtual ~Other() { *ptr_ = kOtherDtorValue; }
    void SetValue(Value value) { *ptr_ = value; }
    Value* ptr_;
  };

  class MultiplyDerived : public Other, public Derived {
   public:
    MultiplyDerived(Value* ptr1, Value* ptr2) : Other(ptr1), Derived(ptr2) {}
  };

  struct VirtuallyDerived : public virtual Base {};

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Value value_ = kInitialValue;
};

class BoxedValue {
 public:
  explicit BoxedValue(int initial_value) : value_(initial_value) {}

  ~BoxedValue() {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  void set_destruction_callback(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

 private:
  int value_ = 0;
  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(BoxedValue);
};

#if defined(OS_IOS) && !TARGET_OS_SIMULATOR
#define MAYBE_ConstructThenPostThenReset FLAKY_ConstructThenPostThenReset
#else
#define MAYBE_ConstructThenPostThenReset ConstructThenPostThenReset
#endif
// https://crbug.com/899779 tracks test flakiness on iOS.
TEST_F(SequenceBoundTest, MAYBE_ConstructThenPostThenReset) {
  auto derived = SequenceBound<Derived>(task_runner_, &value_);
  EXPECT_FALSE(derived.is_null());
  EXPECT_TRUE(derived);

  // Nothing should happen until we run the message loop.
  EXPECT_EQ(value_, kInitialValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Post now that the object has been constructed.
  derived.Post(FROM_HERE, &Derived::SetValue, kDifferentValue);
  EXPECT_EQ(value_, kDerivedCtorValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDifferentValue);

  // Reset it, and make sure that destruction is posted.  The owner should
  // report that it is null immediately.
  derived.Reset();
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(derived);
  EXPECT_EQ(value_, kDifferentValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, PostBeforeConstruction) {
  // Construct an object and post a message to it, before construction has been
  // run on |task_runner_|.
  auto derived = SequenceBound<Derived>(task_runner_, &value_);
  derived.Post(FROM_HERE, &Derived::SetValue, kDifferentValue);
  EXPECT_EQ(value_, kInitialValue);
  // Both construction and SetValue should run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDifferentValue);
}

TEST_F(SequenceBoundTest, MoveConstructionFromSameClass) {
  // Verify that we can move-construct with the same class.
  auto derived_old = SequenceBound<Derived>(task_runner_, &value_);
  auto derived_new = std::move(derived_old);
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Verify that |derived_new| owns the object now, and that the virtual
  // destructor is called.
  derived_new.Reset();
  EXPECT_EQ(value_, kDerivedCtorValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MoveConstructionFromDerivedClass) {
  // Verify that we can move-construct to a base class from a derived class.
  auto derived = SequenceBound<Derived>(task_runner_, &value_);
  SequenceBound<Base> base(std::move(derived));
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Verify that |base| owns the object now, and that destruction still destroys
  // Derived properly.
  base.Reset();
  EXPECT_EQ(value_, kDerivedCtorValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MultiplyDerivedDestructionWorksLeftSuper) {
  // Verify that everything works when we're casting around in ways that might
  // change the address.  We cast to the left side of MultiplyDerived and then
  // reset the owner.  ASAN will catch free() errors.
  Value value2 = kInitialValue;
  auto mderived =
      SequenceBound<MultiplyDerived>(task_runner_, &value_, &value2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kOtherCtorValue);
  EXPECT_EQ(value2, kDerivedCtorValue);
  SequenceBound<Other> other = std::move(mderived);

  other.Reset();
  base::RunLoop().RunUntilIdle();

  // Both destructors should have run.
  EXPECT_EQ(value_, kOtherDtorValue);
  EXPECT_EQ(value2, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MultiplyDerivedDestructionWorksRightSuper) {
  // Verify that everything works when we're casting around in ways that might
  // change the address.  We cast to the right side of MultiplyDerived and then
  // reset the owner.  ASAN will catch free() errors.
  Value value2 = kInitialValue;
  auto mderived =
      SequenceBound<MultiplyDerived>(task_runner_, &value_, &value2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kOtherCtorValue);
  EXPECT_EQ(value2, kDerivedCtorValue);
  SequenceBound<Base> base = std::move(mderived);

  base.Reset();
  base::RunLoop().RunUntilIdle();

  // Both destructors should have run.
  EXPECT_EQ(value_, kOtherDtorValue);
  EXPECT_EQ(value2, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MoveAssignmentFromSameClass) {
  // Test move-assignment using the same classes.
  auto derived_old = SequenceBound<Derived>(task_runner_, &value_);
  SequenceBound<Derived> derived_new;

  derived_new = std::move(derived_old);
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Verify that |derived_new| owns the object now.  Also verifies that move
  // assignment from the same class deletes the outgoing object.
  derived_new = SequenceBound<Derived>();
  EXPECT_EQ(value_, kDerivedCtorValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MoveAssignmentFromDerivedClass) {
  // Move-assignment from a derived class to a base class.
  auto derived = SequenceBound<Derived>(task_runner_, &value_);
  SequenceBound<Base> base;

  base = std::move(derived);
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Verify that |base| owns the object now, and that destruction still destroys
  // Derived properly.
  base.Reset();
  EXPECT_EQ(value_, kDerivedCtorValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);
}

TEST_F(SequenceBoundTest, MoveAssignmentFromDerivedClassDestroysOldObject) {
  // Verify that move-assignment from a derived class runs the dtor of the
  // outgoing object.
  auto derived = SequenceBound<Derived>(task_runner_, &value_);

  Value value1 = kInitialValue;
  Value value2 = kInitialValue;
  auto mderived =
      SequenceBound<MultiplyDerived>(task_runner_, &value1, &value2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(value_, kDerivedCtorValue);

  // Assign |mderived|, and verify that the original object in |derived| is
  // destroyed properly.
  derived = std::move(mderived);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedDtorValue);

  // Delete |derived|, since it has pointers to local vars.
  derived.Reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SequenceBoundTest, MultiplyDerivedPostToLeftBaseClass) {
  // Cast and call methods on the left base class.
  Value value1 = kInitialValue;
  Value value2 = kInitialValue;
  auto mderived =
      SequenceBound<MultiplyDerived>(task_runner_, &value1, &value2);

  // Cast to Other, the left base.
  SequenceBound<Other> other(std::move(mderived));
  other.Post(FROM_HERE, &Other::SetValue, kDifferentValue);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(value1, kDifferentValue);
  EXPECT_EQ(value2, kDerivedCtorValue);

  other.Reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SequenceBoundTest, MultiplyDerivedPostToRightBaseClass) {
  // Cast and call methods on the right base class.
  Value value1 = kInitialValue;
  Value value2 = kInitialValue;
  auto mderived =
      SequenceBound<MultiplyDerived>(task_runner_, &value1, &value2);

  SequenceBound<Derived> derived(std::move(mderived));
  derived.Post(FROM_HERE, &Derived::SetValue, kDifferentValue);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(value1, kOtherCtorValue);
  EXPECT_EQ(value2, kDifferentValue);

  derived.Reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SequenceBoundTest, MoveConstructionFromNullWorks) {
  // Verify that this doesn't crash.
  SequenceBound<Derived> derived1;
  SequenceBound<Derived> derived2(std::move(derived1));
}

TEST_F(SequenceBoundTest, MoveAssignmentFromNullWorks) {
  // Verify that this doesn't crash.
  SequenceBound<Derived> derived1;
  SequenceBound<Derived> derived2;
  derived2 = std::move(derived1);
}

TEST_F(SequenceBoundTest, ResetOnNullObjectWorks) {
  // Verify that this doesn't crash.
  SequenceBound<Derived> derived;
  derived.Reset();
}

TEST_F(SequenceBoundTest, IsVirtualBaseClassOf) {
  // Check that is_virtual_base_of<> works properly.

  // Neither |Base| nor |Derived| is a virtual base of the other.
  static_assert(!internal::is_virtual_base_of<Base, Derived>::value,
                "|Base| shouldn't be a virtual base of |Derived|");
  static_assert(!internal::is_virtual_base_of<Derived, Base>::value,
                "|Derived| shouldn't be a virtual base of |Base|");

  // |Base| should be a virtual base class of |VirtuallyDerived|, but not the
  // other way.
  static_assert(internal::is_virtual_base_of<Base, VirtuallyDerived>::value,
                "|Base| should be a virtual base of |VirtuallyDerived|");
  static_assert(!internal::is_virtual_base_of<VirtuallyDerived, Base>::value,
                "|VirtuallyDerived shouldn't be a virtual base of |Base|");
}

TEST_F(SequenceBoundTest, LvalueConstructionParameter) {
  // Note here that |value_ptr| is an lvalue, while |&value| would be an rvalue.
  Value value = kInitialValue;
  Value* value_ptr = &value;
  SequenceBound<Derived> derived(task_runner_, value_ptr);
  {
    derived.Post(FROM_HERE, &Derived::SetValue, kDifferentValue);
    base::RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(value, kDifferentValue);
  }
  {
    derived.Reset();
    base::RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(value, kDerivedDtorValue);
  }
}

TEST_F(SequenceBoundTest, PostTaskWithThisObject) {
  constexpr int kTestValue1 = 42;
  constexpr int kTestValue2 = 42;
  base::SequenceBound<BoxedValue> value(task_runner_, kTestValue1);
  base::RunLoop loop;
  value.PostTaskWithThisObject(
      FROM_HERE, base::BindLambdaForTesting([&](const BoxedValue& v) {
        EXPECT_EQ(kTestValue1, v.value());
      }));
  value.PostTaskWithThisObject(
      FROM_HERE, base::BindLambdaForTesting(
                     [&](BoxedValue* v) { v->set_value(kTestValue2); }));
  value.PostTaskWithThisObject(
      FROM_HERE, base::BindLambdaForTesting([&](const BoxedValue& v) {
        EXPECT_EQ(kTestValue2, v.value());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SequenceBoundTest, ResetWithCallbackAfterDestruction) {
  base::SequenceBound<BoxedValue> value(task_runner_, 0);

  // Verify that the callback passed to ResetWithCallbackAfterDestruction always
  // does happen *after* destruction.
  bool destroyed = false;
  value.Post(FROM_HERE, &BoxedValue::set_destruction_callback,
             base::BindLambdaForTesting([&] { destroyed = true; }));

  base::RunLoop loop;
  value.ResetWithCallbackAfterDestruction(base::BindLambdaForTesting([&] {
    EXPECT_TRUE(destroyed);
    loop.Quit();
  }));
  loop.Run();
}

}  // namespace base
