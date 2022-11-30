// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_bound.h"

#include <functional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class EventLogger {
 public:
  EventLogger() = default;

  void AddEvent(StringPiece event) {
    AutoLock guard(lock_);
    events_.push_back(std::string(event));
  }
  std::vector<std::string> TakeEvents() {
    AutoLock guard(lock_);
    return std::exchange(events_, {});
  }

 private:
  Lock lock_;
  std::vector<std::string> events_ GUARDED_BY(lock_);
};

class SequenceBoundTest : public ::testing::Test {
 public:
  void TearDown() override {
    // Make sure that any objects owned by `SequenceBound` have been destroyed
    // to avoid tripping leak detection.
    task_environment_.RunUntilIdle();
  }

  // Helper for tests that want to synchronize on a `SequenceBound` which has
  // already been `Reset()`: a null `SequenceBound` has no `SequencedTaskRunner`
  // associated with it, so the usual `FlushPostedTasksForTesting()` helper does
  // not work.
  void FlushPostedTasks() {
    RunLoop run_loop;
    background_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  test::TaskEnvironment task_environment_;

  // Otherwise, default to using `background_task_runner_` for new tests.
  scoped_refptr<SequencedTaskRunner> background_task_runner_ =
      ThreadPool::CreateSequencedTaskRunner({});

  // Defined as part of the test fixture so that tests using `EventLogger` do
  // not need to explicitly synchronize on `Reset() to avoid use-after-frees;
  // instead, tests should rely on `TearDown()` to drain and run any
  // already-posted cleanup tasks.
  EventLogger logger_;
};

class Base {
 public:
  explicit Base(EventLogger& logger) : logger_(logger) {
    logger_->AddEvent("constructed Base");
  }
  virtual ~Base() { logger_->AddEvent("destroyed Base"); }

 protected:
  EventLogger& GetLogger() { return *logger_; }

 private:
  const raw_ref<EventLogger> logger_;
};

class Derived : public Base {
 public:
  explicit Derived(EventLogger& logger) : Base(logger) {
    GetLogger().AddEvent("constructed Derived");
  }

  ~Derived() override { GetLogger().AddEvent("destroyed Derived"); }

  void SetValue(int value) {
    GetLogger().AddEvent(StringPrintf("set Derived to %d", value));
  }
};

class Leftmost {
 public:
  explicit Leftmost(EventLogger& logger) : logger_(logger) {
    logger_->AddEvent("constructed Leftmost");
  }
  virtual ~Leftmost() { logger_->AddEvent("destroyed Leftmost"); }

  void SetValue(int value) {
    logger_->AddEvent(StringPrintf("set Leftmost to %d", value));
  }

 private:
  const raw_ref<EventLogger> logger_;
};

class Rightmost : public Base {
 public:
  explicit Rightmost(EventLogger& logger) : Base(logger) {
    GetLogger().AddEvent("constructed Rightmost");
  }

  ~Rightmost() override { GetLogger().AddEvent("destroyed Rightmost"); }

  void SetValue(int value) {
    GetLogger().AddEvent(StringPrintf("set Rightmost to %d", value));
  }
};

class MultiplyDerived : public Leftmost, public Rightmost {
 public:
  explicit MultiplyDerived(EventLogger& logger)
      : Leftmost(logger), Rightmost(logger) {
    GetLogger().AddEvent("constructed MultiplyDerived");
  }

  ~MultiplyDerived() override {
    GetLogger().AddEvent("destroyed MultiplyDerived");
  }
};

class BoxedValue {
 public:
  explicit BoxedValue(int initial_value, EventLogger* logger = nullptr)
      : logger_(logger), value_(initial_value) {
    AddEventIfNeeded(StringPrintf("constructed BoxedValue = %d", value_));
  }

  BoxedValue(const BoxedValue&) = delete;
  BoxedValue& operator=(const BoxedValue&) = delete;

  ~BoxedValue() {
    EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    AddEventIfNeeded(StringPrintf("destroyed BoxedValue = %d", value_));
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  void set_destruction_callback(OnceClosure callback) {
    EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    destruction_callback_ = std::move(callback);
  }

  int value() const {
    EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    AddEventIfNeeded(StringPrintf("accessed BoxedValue = %d", value_));
    return value_;
  }
  void set_value(int value) {
    EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    AddEventIfNeeded(
        StringPrintf("updated BoxedValue from %d to %d", value_, value));
    value_ = value;
  }

 private:
  void AddEventIfNeeded(StringPiece event) const {
    if (logger_) {
      logger_->AddEvent(event);
    }
  }

  SequenceChecker sequence_checker;

  mutable raw_ptr<EventLogger> logger_ = nullptr;

  int value_ = 0;
  OnceClosure destruction_callback_;
};

// Smoke test that all interactions with the wrapped object are posted to the
// correct task runner.
TEST_F(SequenceBoundTest, SequenceValidation) {
  class Validator {
   public:
    explicit Validator(scoped_refptr<SequencedTaskRunner> task_runner)
        : task_runner_(std::move(task_runner)) {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    }

    ~Validator() { EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence()); }

    void ReturnsVoid() const {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    }

    void ReturnsVoidMutable() {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    }

    int ReturnsInt() const {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
      return 0;
    }

    int ReturnsIntMutable() {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
      return 0;
    }

   private:
    scoped_refptr<SequencedTaskRunner> task_runner_;
  };

  SequenceBound<Validator> validator(background_task_runner_,
                                     background_task_runner_);
  validator.AsyncCall(&Validator::ReturnsVoid);
  validator.AsyncCall(&Validator::ReturnsVoidMutable);
  validator.AsyncCall(&Validator::ReturnsInt).Then(BindOnce([](int) {}));
  validator.AsyncCall(&Validator::ReturnsIntMutable).Then(BindOnce([](int) {}));
  validator.AsyncCall(IgnoreResult(&Validator::ReturnsInt));
  validator.AsyncCall(IgnoreResult(&Validator::ReturnsIntMutable));
  validator.emplace(background_task_runner_, background_task_runner_);
  validator.PostTaskWithThisObject(
      BindLambdaForTesting([](const Validator& v) { v.ReturnsVoid(); }));
  validator.PostTaskWithThisObject(
      BindLambdaForTesting([](Validator* v) { v->ReturnsVoidMutable(); }));
  validator.Reset();
  FlushPostedTasks();
}

TEST_F(SequenceBoundTest, Basic) {
  SequenceBound<BoxedValue> value(background_task_runner_, 0, &logger_);
  // Construction of `BoxedValue` is posted to `background_task_runner_`, but
  // the `SequenceBound` itself should immediately be treated as valid /
  // non-null.
  EXPECT_FALSE(value.is_null());
  EXPECT_TRUE(value);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0"));

  value.AsyncCall(&BoxedValue::set_value).WithArgs(66);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("updated BoxedValue from 0 to 66"));

  // Destruction of `BoxedValue` is posted to `background_task_runner_`, but the
  // `SequenceBound` itself should immediately be treated as valid / non-null.
  value.Reset();
  EXPECT_TRUE(value.is_null());
  EXPECT_FALSE(value);
  FlushPostedTasks();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("destroyed BoxedValue = 66"));
}

TEST_F(SequenceBoundTest, ConstructAndImmediateAsyncCall) {
  // Calling `AsyncCall` immediately after construction should always work.
  SequenceBound<BoxedValue> value(background_task_runner_, 0, &logger_);
  value.AsyncCall(&BoxedValue::set_value).WithArgs(8);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0",
                                     "updated BoxedValue from 0 to 8"));
}

TEST_F(SequenceBoundTest, MoveConstruction) {
  // std::ref() is required here: internally, the async work is bound into the
  // standard base callback infrastructure, which requires the explicit use of
  // `std::cref()` and `std::ref()` when passing by reference.
  SequenceBound<Derived> derived_old(background_task_runner_,
                                     std::ref(logger_));
  SequenceBound<Derived> derived_new = std::move(derived_old);
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());
  derived_new.Reset();
  FlushPostedTasks();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TEST_F(SequenceBoundTest, MoveConstructionUpcastsToBase) {
  SequenceBound<Derived> derived(background_task_runner_, std::ref(logger_));
  SequenceBound<Base> base = std::move(derived);
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());

  // The original `Derived` object is now owned by `SequencedBound<Base>`; make
  // sure `~Derived()` still runs when it is reset.
  base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

// Classes with multiple-derived bases may need pointer adjustments when
// upcasting. These tests rely on sanitizers to catch potential mistakes.
TEST_F(SequenceBoundTest, MoveConstructionUpcastsToLeftmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  SequenceBound<Leftmost> leftmost_base = std::move(multiply_derived);
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(leftmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Leftmost>`; make sure all the expected destructors
  // still run when it is reset.
  leftmost_base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(
      logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TEST_F(SequenceBoundTest, MoveConstructionUpcastsToRightmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  SequenceBound<Rightmost> rightmost_base = std::move(multiply_derived);
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(rightmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Rightmost>`; make sure all the expected destructors
  // still run when it is reset.
  rightmost_base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(
      logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TEST_F(SequenceBoundTest, MoveAssignment) {
  SequenceBound<Derived> derived_old(background_task_runner_,
                                     std::ref(logger_));
  SequenceBound<Derived> derived_new;

  derived_new = std::move(derived_old);
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());

  // Note that this explicitly avoids using `Reset()` as a basic test that
  // assignment resets any previously-owned object.
  derived_new = SequenceBound<Derived>();
  FlushPostedTasks();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TEST_F(SequenceBoundTest, MoveAssignmentUpcastsToBase) {
  SequenceBound<Derived> derived(background_task_runner_, std::ref(logger_));
  SequenceBound<Base> base;

  base = std::move(derived);
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());

  // The original `Derived` object is now owned by `SequencedBound<Base>`; make
  // sure `~Derived()` still runs when it is reset.
  base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TEST_F(SequenceBoundTest, MoveAssignmentUpcastsToLeftmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  SequenceBound<Leftmost> leftmost_base;

  leftmost_base = std::move(multiply_derived);
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(leftmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Leftmost>`; make sure all the expected destructors
  // still run when it is reset.
  leftmost_base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(
      logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TEST_F(SequenceBoundTest, MoveAssignmentUpcastsToRightmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  SequenceBound<Rightmost> rightmost_base;

  rightmost_base = std::move(multiply_derived);
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(rightmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Rightmost>`; make sure all the expected destructors
  // still run when it is reset.
  rightmost_base.Reset();
  FlushPostedTasks();
  EXPECT_THAT(
      logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TEST_F(SequenceBoundTest, AsyncCallLeftmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  multiply_derived.AsyncCall(&Leftmost::SetValue).WithArgs(3);
  multiply_derived.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Leftmost", "constructed Base",
                                     "constructed Rightmost",
                                     "constructed MultiplyDerived",
                                     "set Leftmost to 3"));
}

TEST_F(SequenceBoundTest, AsyncCallRightmost) {
  SequenceBound<MultiplyDerived> multiply_derived(background_task_runner_,
                                                  std::ref(logger_));
  multiply_derived.AsyncCall(&Rightmost::SetValue).WithArgs(3);
  multiply_derived.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Leftmost", "constructed Base",
                                     "constructed Rightmost",
                                     "constructed MultiplyDerived",
                                     "set Rightmost to 3"));
}

TEST_F(SequenceBoundTest, MoveConstructionFromNull) {
  SequenceBound<BoxedValue> value1;
  // Should not crash.
  SequenceBound<BoxedValue> value2(std::move(value1));
}

TEST_F(SequenceBoundTest, MoveAssignmentFromNull) {
  SequenceBound<BoxedValue> value1;
  SequenceBound<BoxedValue> value2;
  // Should not crash.
  value2 = std::move(value1);
}

TEST_F(SequenceBoundTest, MoveAssignmentFromSelf) {
  SequenceBound<BoxedValue> value;
  // Cheat to avoid clang self-move warning.
  auto& value2 = value;
  // Should not crash.
  value2 = std::move(value);
}

TEST_F(SequenceBoundTest, ResetNullSequenceBound) {
  SequenceBound<BoxedValue> value;
  // Should not crash.
  value.Reset();
}

TEST_F(SequenceBoundTest, ConstructWithLvalue) {
  int lvalue = 99;
  SequenceBound<BoxedValue> value(background_task_runner_, lvalue, &logger_);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 99"));
}

TEST_F(SequenceBoundTest, PostTaskWithThisObject) {
  constexpr int kTestValue1 = 42;
  constexpr int kTestValue2 = 42;
  SequenceBound<BoxedValue> value(background_task_runner_, kTestValue1);
  value.PostTaskWithThisObject(BindLambdaForTesting(
      [&](const BoxedValue& v) { EXPECT_EQ(kTestValue1, v.value()); }));
  value.PostTaskWithThisObject(
      BindLambdaForTesting([&](BoxedValue* v) { v->set_value(kTestValue2); }));
  value.PostTaskWithThisObject(BindLambdaForTesting(
      [&](const BoxedValue& v) { EXPECT_EQ(kTestValue2, v.value()); }));
  value.FlushPostedTasksForTesting();
}

TEST_F(SequenceBoundTest, SynchronouslyResetForTest) {
  SequenceBound<BoxedValue> value(background_task_runner_, 0);

  bool destroyed = false;
  value.AsyncCall(&BoxedValue::set_destruction_callback)
      .WithArgs(BindLambdaForTesting([&] { destroyed = true; }));

  value.SynchronouslyResetForTest();
  EXPECT_TRUE(destroyed);
}

TEST_F(SequenceBoundTest, FlushPostedTasksForTesting) {
  SequenceBound<BoxedValue> value(background_task_runner_, 0, &logger_);

  value.AsyncCall(&BoxedValue::set_value).WithArgs(42);
  value.FlushPostedTasksForTesting();

  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0",
                                     "updated BoxedValue from 0 to 42"));
}

TEST_F(SequenceBoundTest, SmallObject) {
  class EmptyClass {};
  SequenceBound<EmptyClass> value(background_task_runner_);
  // Test passes if SequenceBound constructor does not crash in AlignedAlloc().
}

TEST_F(SequenceBoundTest, SelfMoveAssign) {
  class EmptyClass {};
  SequenceBound<EmptyClass> value(background_task_runner_);
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

TEST_F(SequenceBoundTest, Emplace) {
  SequenceBound<BoxedValue> value;
  EXPECT_TRUE(value.is_null());
  value.emplace(background_task_runner_, 8);
  value.AsyncCall(&BoxedValue::value)
      .Then(BindLambdaForTesting(
          [&](int actual_value) { EXPECT_EQ(8, actual_value); }));
  value.FlushPostedTasksForTesting();
}

TEST_F(SequenceBoundTest, EmplaceOverExisting) {
  SequenceBound<BoxedValue> value(background_task_runner_, 8, &logger_);
  EXPECT_FALSE(value.is_null());
  value.emplace(background_task_runner_, 9, &logger_);
  value.AsyncCall(&BoxedValue::value)
      .Then(BindLambdaForTesting(
          [&](int actual_value) { EXPECT_EQ(9, actual_value); }));
  value.FlushPostedTasksForTesting();
  // Both the replaced `BoxedValue` and the current `BoxedValue` should
  // live on the same sequence: make sure the replaced `BoxedValue` was
  // destroyed before the current `BoxedValue` was constructed.
  EXPECT_THAT(logger_.TakeEvents(),
              ::testing::ElementsAre(
                  "constructed BoxedValue = 8", "destroyed BoxedValue = 8",
                  "constructed BoxedValue = 9", "accessed BoxedValue = 9"));
}

TEST_F(SequenceBoundTest, EmplaceOverExistingWithTaskRunnerSwap) {
  scoped_refptr<SequencedTaskRunner> another_task_runner =
      ThreadPool::CreateSequencedTaskRunner({});
  // No `EventLogger` here since destruction of the old `BoxedValue` and
  // construction of the new `BoxedValue` take place on different sequences and
  // can arbitrarily race.
  SequenceBound<BoxedValue> value(another_task_runner, 8);
  EXPECT_FALSE(value.is_null());
  value.emplace(background_task_runner_, 9);
  {
    value.PostTaskWithThisObject(BindLambdaForTesting(
        [another_task_runner, background_task_runner = background_task_runner_](
            const BoxedValue& boxed_value) {
          EXPECT_FALSE(another_task_runner->RunsTasksInCurrentSequence());
          EXPECT_TRUE(background_task_runner->RunsTasksInCurrentSequence());
          EXPECT_EQ(9, boxed_value.value());
        }));
    value.FlushPostedTasksForTesting();
  }
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
  const raw_ptr<int> method_called_with_;
  const raw_ptr<int> const_method_called_with_;

  raw_ptr<RunLoop> loop_ = nullptr;
};

class IntArgIntReturn {
 public:
  int Method(int x) { return -x; }
  int ConstMethod(int x) const { return -x; }
};

}  // namespace

TEST_F(SequenceBoundTest, AsyncCallNoArgsNoThen) {
  SequenceBound<NoArgsVoidReturn> s(background_task_runner_);

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
  SequenceBound<IntArgVoidReturn> s(
      background_task_runner_, &method_called_with, &const_method_called_with);

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
  SequenceBound<NoArgsVoidReturn> s(background_task_runner_);

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
  SequenceBound<NoArgsIntReturn> s(background_task_runner_);

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
  SequenceBound<IntArgVoidReturn> s(
      background_task_runner_, &method_called_with, &const_method_called_with);

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
  SequenceBound<IntArgIntReturn> s(background_task_runner_);

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
  const SequenceBound<NoArgsVoidReturn> s(background_task_runner_);
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
  const raw_ptr<RunLoop> loop_ = nullptr;
  const raw_ptr<bool> called_ = nullptr;
};

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultNoArgs) {
  {
    RunLoop loop;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(background_task_runner_,
                                                      &loop, nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod));
    loop.Run();
  }

  {
    RunLoop loop;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(background_task_runner_,
                                                      &loop, nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::Method));
    loop.Run();
  }
}

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultThen) {
  {
    RunLoop loop;
    bool called = false;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(background_task_runner_,
                                                      nullptr, &called);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod))
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_TRUE(called);
  }

  {
    RunLoop loop;
    bool called = false;
    SequenceBound<IgnoreResultTestHelperWithNoArgs> s(background_task_runner_,
                                                      nullptr, &called);
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
  const raw_ptr<RunLoop> loop_ = nullptr;
  int& value_;
};

TEST_F(SequenceBoundTest, AsyncCallIgnoreResultWithArgs) {
  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(background_task_runner_,
                                                    &loop, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60);
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(background_task_runner_,
                                                    &loop, std::ref(result));
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
    SequenceBound<IgnoreResultTestHelperWithArgs> s(background_task_runner_,
                                                    nullptr, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SequenceBound<IgnoreResultTestHelperWithArgs> s(background_task_runner_,
                                                    nullptr, std::ref(result));
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
      SequencedTaskRunnerHandle::Get();
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

}  // namespace

}  // namespace base
