// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_bound.h"

#include <functional>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
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

  void TearDown() override {
    // Make sure that any objects owned by `SequenceBound` have been destroyed
    // to avoid tripping leak detection.
    RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

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

  // TODO(dcheng): This isn't used, but upcasting to a virtual base class is
  // unsafe and is currently unchecked! Add these safety checks back in.
  struct VirtuallyDerived : public virtual Base {};

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunnerHandle::Get();

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
#define MAYBE_ConstructAsyncCallReset FLAKY_ConstructAsyncCallReset
#else
#define MAYBE_ConstructAsyncCallReset ConstructAsyncCallReset
#endif
// https://crbug.com/899779 tracks test flakiness on iOS.
TEST_F(SequenceBoundTest, MAYBE_ConstructAsyncCallReset) {
  auto derived = SequenceBound<Derived>(task_runner_, &value_);
  EXPECT_FALSE(derived.is_null());
  EXPECT_TRUE(derived);

  // Nothing should happen until we run the message loop.
  EXPECT_EQ(value_, kInitialValue);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(value_, kDerivedCtorValue);

  // Post now that the object has been constructed.
  derived.AsyncCall(&Derived::SetValue).WithArgs(kDifferentValue);
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
  derived.AsyncCall(&Derived::SetValue).WithArgs(kDifferentValue);
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
  other.AsyncCall(&Other::SetValue).WithArgs(kDifferentValue);

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
  derived.AsyncCall(&Derived::SetValue).WithArgs(kDifferentValue);

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

TEST_F(SequenceBoundTest, LvalueConstructionParameter) {
  // Note here that |value_ptr| is an lvalue, while |&value| would be an rvalue.
  Value value = kInitialValue;
  Value* value_ptr = &value;
  SequenceBound<Derived> derived(task_runner_, value_ptr);
  {
    derived.AsyncCall(&Derived::SetValue).WithArgs(kDifferentValue);
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

TEST_F(SequenceBoundTest, SynchronouslyResetForTest) {
  base::SequenceBound<BoxedValue> value(task_runner_, 0);

  bool destroyed = false;
  value.AsyncCall(&BoxedValue::set_destruction_callback)
      .WithArgs(base::BindLambdaForTesting([&] { destroyed = true; }));

  value.SynchronouslyResetForTest();
  EXPECT_TRUE(destroyed);
}

TEST_F(SequenceBoundTest, SmallObject) {
  class EmptyClass {};
  SequenceBound<EmptyClass> value(task_runner_);
  // Test passes if SequenceBound constructor does not crash in AlignedAlloc().
}

TEST_F(SequenceBoundTest, SelfMoveAssign) {
  class EmptyClass {};
  SequenceBound<EmptyClass> value(task_runner_);
  EXPECT_FALSE(value.is_null());
  // Clang has a warning for self-move, so be clever.
  auto& actually_the_same_value = value;
  value = std::move(actually_the_same_value);
  // Note: in general, moved-from objects are in a valid but undefined state.
  // This is merely a test that self-move doesn't result in something bad
  // happening; this is not an assertion that self-move will always have this
  // behavior.
  EXPECT_TRUE(value.is_null());
}

namespace {

class NoArgsVoidReturn {
 public:
  void Method() {
    if (loop_)
      loop_->Quit();
  }
  void ConstMethod() const {
    if (loop_)
      loop_->Quit();
  }

  void set_loop(RunLoop* loop) { loop_ = loop; }

 private:
  RunLoop* loop_ = nullptr;
};

class NoArgsIntReturn {
 public:
  int Method() { return 123; }
  int ConstMethod() const { return 456; }
};

class IntArgVoidReturn {
 public:
  IntArgVoidReturn(int* method_called_with, int* const_method_called_with)
      : method_called_with_(method_called_with),
        const_method_called_with_(const_method_called_with) {}

  void Method(int x) {
    *method_called_with_ = x;
    if (loop_)
      loop_->Quit();
  }
  void ConstMethod(int x) const {
    *const_method_called_with_ = x;
    if (loop_)
      loop_->Quit();
  }

  void set_loop(RunLoop* loop) { loop_ = loop; }

 private:
  int* const method_called_with_;
  int* const const_method_called_with_;

  RunLoop* loop_ = nullptr;
};

class IntArgIntReturn {
 public:
  int Method(int x) { return -x; }
  int ConstMethod(int x) const { return -x; }
};

}  // namespace

TEST_F(SequenceBoundTest, AsyncCallNoArgsNoThen) {
  SequenceBound<NoArgsVoidReturn> s(task_runner_);

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::set_loop).WithArgs(&loop);
    s.AsyncCall(&NoArgsVoidReturn::Method);
    loop.Run();
  }

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::set_loop).WithArgs(&loop);
    s.AsyncCall(&NoArgsVoidReturn::ConstMethod);
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallIntArgNoThen) {
  int method_called_with = 0;
  int const_method_called_with = 0;
  SequenceBound<IntArgVoidReturn> s(task_runner_, &method_called_with,
                                    &const_method_called_with);

  {
    RunLoop loop;
    s.AsyncCall(&IntArgVoidReturn::set_loop).WithArgs(&loop);
    s.AsyncCall(&IntArgVoidReturn::Method).WithArgs(123);
    loop.Run();
    EXPECT_EQ(123, method_called_with);
  }

  {
    RunLoop loop;
    s.AsyncCall(&IntArgVoidReturn::set_loop).WithArgs(&loop);
    s.AsyncCall(&IntArgVoidReturn::ConstMethod).WithArgs(456);
    loop.Run();
    EXPECT_EQ(456, const_method_called_with);
  }
}

TEST_F(SequenceBoundTest, AsyncCallNoArgsVoidThen) {
  SequenceBound<NoArgsVoidReturn> s(task_runner_);

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::Method).Then(BindLambdaForTesting([&]() {
      loop.Quit();
    }));
    loop.Run();
  }

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::ConstMethod)
        .Then(BindLambdaForTesting([&]() { loop.Quit(); }));
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallNoArgsIntThen) {
  SequenceBound<NoArgsIntReturn> s(task_runner_);

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsIntReturn::Method)
        .Then(BindLambdaForTesting([&](int result) {
          EXPECT_EQ(123, result);
          loop.Quit();
        }));
    loop.Run();
  }

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsIntReturn::ConstMethod)
        .Then(BindLambdaForTesting([&](int result) {
          EXPECT_EQ(456, result);
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallWithArgsVoidThen) {
  int method_called_with = 0;
  int const_method_called_with = 0;
  SequenceBound<IntArgVoidReturn> s(task_runner_, &method_called_with,
                                    &const_method_called_with);

  {
    RunLoop loop;
    s.AsyncCall(&IntArgVoidReturn::Method)
        .WithArgs(123)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(123, method_called_with);
  }

  {
    RunLoop loop;
    s.AsyncCall(&IntArgVoidReturn::ConstMethod)
        .WithArgs(456)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(456, const_method_called_with);
  }
}

TEST_F(SequenceBoundTest, AsyncCallWithArgsIntThen) {
  SequenceBound<IntArgIntReturn> s(task_runner_);

  {
    RunLoop loop;
    s.AsyncCall(&IntArgIntReturn::Method)
        .WithArgs(123)
        .Then(BindLambdaForTesting([&](int result) {
          EXPECT_EQ(-123, result);
          loop.Quit();
        }));
    loop.Run();
  }

  {
    RunLoop loop;
    s.AsyncCall(&IntArgIntReturn::ConstMethod)
        .WithArgs(456)
        .Then(BindLambdaForTesting([&](int result) {
          EXPECT_EQ(-456, result);
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallIsConstQualified) {
  // Tests that both const and non-const methods may be called through a
  // const-qualified SequenceBound.
  const SequenceBound<NoArgsVoidReturn> s(task_runner_);
  s.AsyncCall(&NoArgsVoidReturn::ConstMethod);
  s.AsyncCall(&NoArgsVoidReturn::Method);
}

class IgnoreResultTestHelperWithNoArgs {
 public:
  explicit IgnoreResultTestHelperWithNoArgs(RunLoop* loop, bool* called)
      : loop_(loop), called_(called) {}

  int ConstMethod() const {
    if (loop_) {
      loop_->Quit();
    }
    if (called_) {
      *called_ = true;
    }
    return 0;
  }

  int Method() {
    if (loop_) {
      loop_->Quit();
    }
    if (called_) {
      *called_ = true;
    }
    return 0;
  }

 private:
  RunLoop* const loop_ = nullptr;
  bool* const called_ = nullptr;
};

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultNoArgs) {
  {
    RunLoop loop;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(task_runner_, &loop,
                                                      nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod));
    loop.Run();
  }

  {
    RunLoop loop;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(task_runner_, &loop,
                                                      nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::Method));
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultThen) {
  {
    RunLoop loop;
    bool called = false;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(task_runner_, nullptr,
                                                      &called);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod))
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_TRUE(called);
  }

  {
    RunLoop loop;
    bool called = false;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(task_runner_, nullptr,
                                                      &called);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::Method))
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_TRUE(called);
  }
}

class IgnoreResultTestHelperWithArgs {
 public:
  IgnoreResultTestHelperWithArgs(RunLoop* loop, int& value)
      : loop_(loop), value_(value) {}

  int ConstMethod(int arg) const {
    value_ = arg;
    if (loop_) {
      loop_->Quit();
    }
    return arg;
  }

  int Method(int arg) {
    value_ = arg;
    if (loop_) {
      loop_->Quit();
    }
    return arg;
  }

 private:
  RunLoop* const loop_ = nullptr;
  int& value_;
};

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultWithArgs) {
  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(task_runner_, &loop,
                                                    std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60);
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(task_runner_, &loop,
                                                    std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::Method))
        .WithArgs(06);
    loop.Run();
    EXPECT_EQ(06, result);
  }
}

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultWithArgsThen) {
  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(task_runner_, nullptr,
                                                    std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(task_runner_, nullptr,
                                                    std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::Method))
        .WithArgs(06)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(06, result);
  }
}

// TODO(dcheng): Maybe use the nocompile harness here instead of being
// "clever"...
TEST_F(SequenceBoundTest, NoCompileTests) {
  // TODO(dcheng): Test calling WithArgs() on a method that takes no arguments.
  // Given:
  //   class C {
  //     void F();
  //   };
  //
  // Then:
  //   SequenceBound<C> s(...);
  //   s.AsyncCall(&C::F).WithArgs(...);
  //
  // should not compile.
  //
  // TODO(dcheng): Test calling Then() before calling WithArgs().
  // Given:
  //   class C {
  //     void F(int);
  //   };
  //
  // Then:
  //   SequenceBound<C> s(...);
  //   s.AsyncCall(&C::F).Then(...).WithArgs(...);
  //
  // should not compile.
  //
}

class SequenceBoundDeathTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Make sure that any objects owned by `SequenceBound` have been destroyed
    // to avoid tripping leak detection.
    RunLoop run_loop;
    task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Death tests use fork(), which can interact (very) poorly with threads.
  test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunnerHandle::Get();
};

TEST_F(SequenceBoundDeathTest, AsyncCallIntArgNoWithArgsShouldCheck) {
  SequenceBound<IntArgIntReturn> s(task_runner_);
  EXPECT_DEATH_IF_SUPPORTED(s.AsyncCall(&IntArgIntReturn::Method), "");
}

TEST_F(SequenceBoundDeathTest, AsyncCallIntReturnNoThenShouldCheck) {
  {
    SequenceBound<NoArgsIntReturn> s(task_runner_);
    EXPECT_DEATH_IF_SUPPORTED(s.AsyncCall(&NoArgsIntReturn::Method), "");
  }

  {
    SequenceBound<IntArgIntReturn> s(task_runner_);
    EXPECT_DEATH_IF_SUPPORTED(s.AsyncCall(&IntArgIntReturn::Method).WithArgs(0),
                              "");
  }
}

}  // namespace base
