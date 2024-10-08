// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_bound.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class EventLogger {
 public:
  EventLogger() = default;

  void AddEvent(std::string_view event) {
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

// Helpers for writing type tests against both `SequenceBound<T>` and
// `SequenceBound<std::unique_ptr<T>`. The tricky part here is that the
// constructor and emplace both need to accept variadic args; however,
// construction of the actual `T` depends on the storage strategy.  The
// `Wrapper` template provides this layer of indirection to construct the
// managed `T` while still passing through all the other remaining
// `SequenceBound` APIs.
struct DirectVariation {
  static constexpr bool kManagingTaskRunnerConstructsT = true;

  template <typename T>
  class Wrapper : public SequenceBound<T> {
   public:
    template <typename... Args>
    explicit Wrapper(scoped_refptr<SequencedTaskRunner> task_runner,
                     Args&&... args)
        : SequenceBound<T>(std::move(task_runner),
                           std::forward<Args>(args)...) {}

    template <typename... Args>
    void WrappedEmplace(scoped_refptr<SequencedTaskRunner> task_runner,
                        Args&&... args) {
      this->emplace(std::move(task_runner), std::forward<Args>(args)...);
    }

    using SequenceBound<T>::SequenceBound;
    using SequenceBound<T>::operator=;

   private:
    using SequenceBound<T>::emplace;
  };
};

struct UniquePtrVariation {
  static constexpr bool kManagingTaskRunnerConstructsT = false;

  template <typename T>
  struct Wrapper : public SequenceBound<std::unique_ptr<T>> {
   public:
    template <typename... Args>
    explicit Wrapper(scoped_refptr<SequencedTaskRunner> task_runner,
                     Args&&... args)
        : SequenceBound<std::unique_ptr<T>>(
              std::move(task_runner),
              std::make_unique<T>(std::forward<Args>(args)...)) {}

    template <typename... Args>
    void WrappedEmplace(scoped_refptr<SequencedTaskRunner> task_runner,
                        Args&&... args) {
      this->emplace(std::move(task_runner),
                    std::make_unique<T>(std::forward<Args>(args)...));
    }

    using SequenceBound<std::unique_ptr<T>>::SequenceBound;
    using SequenceBound<std::unique_ptr<T>>::operator=;

   private:
    using SequenceBound<std::unique_ptr<T>>::emplace;
  };
};

// Helper macros since using the name directly is otherwise quite unwieldy.
#define SEQUENCE_BOUND_T typename TypeParam::template Wrapper
// Try to catch tests that inadvertently use SequenceBound<T> directly instead
// of SEQUENCE_BOUND_T, as that bypasses the point of having a typed test.
#define SequenceBound PleaseUseSequenceBoundT

template <typename Variation>
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

  // Task runner to use for SequenceBound's managed `T`.
  scoped_refptr<SequencedTaskRunner> background_task_runner_ =
      ThreadPool::CreateSequencedTaskRunner({});

  // Defined as part of the test fixture so that tests using `EventLogger` do
  // not need to explicitly synchronize on `Reset() to avoid use-after-frees;
  // instead, tests should rely on `TearDown()` to drain and run any
  // already-posted cleanup tasks.
  EventLogger logger_;
};

using Variations = ::testing::Types<DirectVariation, UniquePtrVariation>;
TYPED_TEST_SUITE(SequenceBoundTest, Variations);

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
    sequence_checker_.DetachFromSequence();
    AddEventIfNeeded(StringPrintf("constructed BoxedValue = %d", value_));
  }

  BoxedValue(const BoxedValue&) = delete;
  BoxedValue& operator=(const BoxedValue&) = delete;

  ~BoxedValue() {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    AddEventIfNeeded(StringPrintf("destroyed BoxedValue = %d", value_));
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  void set_destruction_callback(OnceClosure callback) {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    destruction_callback_ = std::move(callback);
  }

  int value() const {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    AddEventIfNeeded(StringPrintf("accessed BoxedValue = %d", value_));
    return value_;
  }
  void set_value(int value) {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    AddEventIfNeeded(
        StringPrintf("updated BoxedValue from %d to %d", value_, value));
    value_ = value;
  }

 private:
  void AddEventIfNeeded(std::string_view event) const {
    if (logger_) {
      logger_->AddEvent(event);
    }
  }

  SequenceChecker sequence_checker_;

  mutable raw_ptr<EventLogger> logger_ = nullptr;

  int value_ = 0;
  OnceClosure destruction_callback_;
};

// Smoke test that all interactions with the wrapped object are posted to the
// correct task runner.
class SequenceValidator {
 public:
  explicit SequenceValidator(scoped_refptr<SequencedTaskRunner> task_runner,
                             bool constructs_on_managing_task_runner)
      : task_runner_(std::move(task_runner)) {
    if (constructs_on_managing_task_runner) {
      EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    }
  }

  ~SequenceValidator() {
    EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
  }

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

TYPED_TEST(SequenceBoundTest, SequenceValidation) {
  SEQUENCE_BOUND_T<SequenceValidator> validator(
      this->background_task_runner_, this->background_task_runner_,
      TypeParam::kManagingTaskRunnerConstructsT);
  validator.AsyncCall(&SequenceValidator::ReturnsVoid);
  validator.AsyncCall(&SequenceValidator::ReturnsVoidMutable);
  validator.AsyncCall(&SequenceValidator::ReturnsInt).Then(BindOnce([](int) {
  }));
  validator.AsyncCall(&SequenceValidator::ReturnsIntMutable)
      .Then(BindOnce([](int) {}));
  validator.AsyncCall(IgnoreResult(&SequenceValidator::ReturnsInt));
  validator.AsyncCall(IgnoreResult(&SequenceValidator::ReturnsIntMutable));
  validator.WrappedEmplace(this->background_task_runner_,
                           this->background_task_runner_,
                           TypeParam::kManagingTaskRunnerConstructsT);
  validator.PostTaskWithThisObject(BindLambdaForTesting(
      [](const SequenceValidator& v) { v.ReturnsVoid(); }));
  validator.PostTaskWithThisObject(BindLambdaForTesting(
      [](SequenceValidator* v) { v->ReturnsVoidMutable(); }));
  validator.Reset();
  this->FlushPostedTasks();
}

TYPED_TEST(SequenceBoundTest, Basic) {
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, 0,
                                     &this->logger_);
  // Construction of `BoxedValue` may be posted to `background_task_runner_`,
  // but the `SequenceBound` itself should immediately be treated as valid /
  // non-null.
  EXPECT_FALSE(value.is_null());
  EXPECT_TRUE(value);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0"));

  value.AsyncCall(&BoxedValue::set_value).WithArgs(66);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("updated BoxedValue from 0 to 66"));

  // Destruction of `BoxedValue` may be posted to `background_task_runner_`, but
  // the `SequenceBound` itself should immediately be treated as valid /
  // non-null.
  value.Reset();
  EXPECT_TRUE(value.is_null());
  EXPECT_FALSE(value);
  this->FlushPostedTasks();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("destroyed BoxedValue = 66"));
}

TYPED_TEST(SequenceBoundTest, ConstructAndImmediateAsyncCall) {
  // Calling `AsyncCall` immediately after construction should always work.
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, 0,
                                     &this->logger_);
  value.AsyncCall(&BoxedValue::set_value).WithArgs(8);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0",
                                     "updated BoxedValue from 0 to 8"));
}

TYPED_TEST(SequenceBoundTest, MoveConstruction) {
  // std::ref() is required here: internally, the async work is bound into the
  // standard base callback infrastructure, which requires the explicit use of
  // `std::cref()` and `std::ref()` when passing by reference.
  SEQUENCE_BOUND_T<Derived> derived_old(this->background_task_runner_,
                                        std::ref(this->logger_));
  SEQUENCE_BOUND_T<Derived> derived_new = std::move(derived_old);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());
  derived_new.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TYPED_TEST(SequenceBoundTest, MoveConstructionUpcastsToBase) {
  SEQUENCE_BOUND_T<Derived> derived(this->background_task_runner_,
                                    std::ref(this->logger_));
  SEQUENCE_BOUND_T<Base> base = std::move(derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());

  // The original `Derived` object is now owned by `SequencedBound<Base>`; make
  // sure `~Derived()` still runs when it is reset.
  base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

// Classes with multiple-derived bases may need pointer adjustments when
// upcasting. These tests rely on sanitizers to catch potential mistakes.
TYPED_TEST(SequenceBoundTest, MoveConstructionUpcastsToLeftmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  SEQUENCE_BOUND_T<Leftmost> leftmost_base = std::move(multiply_derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(leftmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Leftmost>`; make sure all the expected destructors
  // still run when it is reset.
  leftmost_base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(
      this->logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TYPED_TEST(SequenceBoundTest, MoveConstructionUpcastsToRightmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  SEQUENCE_BOUND_T<Rightmost> rightmost_base = std::move(multiply_derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(rightmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Rightmost>`; make sure all the expected destructors
  // still run when it is reset.
  rightmost_base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(
      this->logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TYPED_TEST(SequenceBoundTest, MoveAssignment) {
  SEQUENCE_BOUND_T<Derived> derived_old(this->background_task_runner_,
                                        std::ref(this->logger_));
  SEQUENCE_BOUND_T<Derived> derived_new;

  derived_new = std::move(derived_old);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(derived_old.is_null());
  EXPECT_FALSE(derived_new.is_null());

  // Note that this explicitly avoids using `Reset()` as a basic test that
  // assignment resets any previously-owned object.
  derived_new = SEQUENCE_BOUND_T<Derived>();
  this->FlushPostedTasks();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TYPED_TEST(SequenceBoundTest, MoveAssignmentUpcastsToBase) {
  SEQUENCE_BOUND_T<Derived> derived(this->background_task_runner_,
                                    std::ref(this->logger_));
  SEQUENCE_BOUND_T<Base> base;

  base = std::move(derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(derived.is_null());
  EXPECT_FALSE(base.is_null());

  // The original `Derived` object is now owned by `SequencedBound<Base>`; make
  // sure `~Derived()` still runs when it is reset.
  base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Base", "constructed Derived",
                                     "destroyed Derived", "destroyed Base"));
}

TYPED_TEST(SequenceBoundTest, MoveAssignmentUpcastsToLeftmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  SEQUENCE_BOUND_T<Leftmost> leftmost_base;

  leftmost_base = std::move(multiply_derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(leftmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Leftmost>`; make sure all the expected destructors
  // still run when it is reset.
  leftmost_base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(
      this->logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TYPED_TEST(SequenceBoundTest, MoveAssignmentUpcastsToRightmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  SEQUENCE_BOUND_T<Rightmost> rightmost_base;

  rightmost_base = std::move(multiply_derived);
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(multiply_derived.is_null());
  EXPECT_FALSE(rightmost_base.is_null());

  // The original `MultiplyDerived` object is now owned by
  // `SequencedBound<Rightmost>`; make sure all the expected destructors
  // still run when it is reset.
  rightmost_base.Reset();
  this->FlushPostedTasks();
  EXPECT_THAT(
      this->logger_.TakeEvents(),
      ::testing::ElementsAre(
          "constructed Leftmost", "constructed Base", "constructed Rightmost",
          "constructed MultiplyDerived", "destroyed MultiplyDerived",
          "destroyed Rightmost", "destroyed Base", "destroyed Leftmost"));
}

TYPED_TEST(SequenceBoundTest, AsyncCallLeftmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  multiply_derived.AsyncCall(&Leftmost::SetValue).WithArgs(3);
  multiply_derived.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Leftmost", "constructed Base",
                                     "constructed Rightmost",
                                     "constructed MultiplyDerived",
                                     "set Leftmost to 3"));
}

TYPED_TEST(SequenceBoundTest, AsyncCallRightmost) {
  SEQUENCE_BOUND_T<MultiplyDerived> multiply_derived(
      this->background_task_runner_, std::ref(this->logger_));
  multiply_derived.AsyncCall(&Rightmost::SetValue).WithArgs(3);
  multiply_derived.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed Leftmost", "constructed Base",
                                     "constructed Rightmost",
                                     "constructed MultiplyDerived",
                                     "set Rightmost to 3"));
}

TYPED_TEST(SequenceBoundTest, MoveConstructionFromNull) {
  SEQUENCE_BOUND_T<BoxedValue> value1;
  // Should not crash.
  SEQUENCE_BOUND_T<BoxedValue> value2(std::move(value1));
}

TYPED_TEST(SequenceBoundTest, MoveAssignmentFromNull) {
  SEQUENCE_BOUND_T<BoxedValue> value1;
  SEQUENCE_BOUND_T<BoxedValue> value2;
  // Should not crash.
  value2 = std::move(value1);
}

TYPED_TEST(SequenceBoundTest, MoveAssignmentFromSelf) {
  SEQUENCE_BOUND_T<BoxedValue> value;
  // Cheat to avoid clang self-move warning.
  auto& value2 = value;
  // Should not crash.
  value2 = std::move(value);
}

TYPED_TEST(SequenceBoundTest, ResetNullSequenceBound) {
  SEQUENCE_BOUND_T<BoxedValue> value;
  // Should not crash.
  value.Reset();
}

TYPED_TEST(SequenceBoundTest, ConstructWithLvalue) {
  int lvalue = 99;
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, lvalue,
                                     &this->logger_);
  value.FlushPostedTasksForTesting();
  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 99"));
}

TYPED_TEST(SequenceBoundTest, PostTaskWithThisObject) {
  constexpr int kTestValue1 = 42;
  constexpr int kTestValue2 = 42;
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_,
                                     kTestValue1);
  value.PostTaskWithThisObject(BindLambdaForTesting(
      [&](const BoxedValue& v) { EXPECT_EQ(kTestValue1, v.value()); }));
  value.PostTaskWithThisObject(
      BindLambdaForTesting([&](BoxedValue* v) { v->set_value(kTestValue2); }));
  value.PostTaskWithThisObject(BindLambdaForTesting(
      [&](const BoxedValue& v) { EXPECT_EQ(kTestValue2, v.value()); }));
  value.FlushPostedTasksForTesting();
}

TYPED_TEST(SequenceBoundTest, SynchronouslyResetForTest) {
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, 0);

  bool destroyed = false;
  value.AsyncCall(&BoxedValue::set_destruction_callback)
      .WithArgs(BindLambdaForTesting([&] { destroyed = true; }));

  value.SynchronouslyResetForTest();
  EXPECT_TRUE(destroyed);
}

TYPED_TEST(SequenceBoundTest, FlushPostedTasksForTesting) {
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, 0,
                                     &this->logger_);

  value.AsyncCall(&BoxedValue::set_value).WithArgs(42);
  value.FlushPostedTasksForTesting();

  EXPECT_THAT(this->logger_.TakeEvents(),
              ::testing::ElementsAre("constructed BoxedValue = 0",
                                     "updated BoxedValue from 0 to 42"));
}

TYPED_TEST(SequenceBoundTest, SmallObject) {
  class EmptyClass {};
  SEQUENCE_BOUND_T<EmptyClass> value(this->background_task_runner_);
  // Test passes if SequenceBound constructor does not crash in AlignedAlloc().
}

TYPED_TEST(SequenceBoundTest, SelfMoveAssign) {
  class EmptyClass {};
  SEQUENCE_BOUND_T<EmptyClass> value(this->background_task_runner_);
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

TYPED_TEST(SequenceBoundTest, Emplace) {
  SEQUENCE_BOUND_T<BoxedValue> value;
  EXPECT_TRUE(value.is_null());
  value.WrappedEmplace(this->background_task_runner_, 8);
  value.AsyncCall(&BoxedValue::value)
      .Then(BindLambdaForTesting(
          [&](int actual_value) { EXPECT_EQ(8, actual_value); }));
  value.FlushPostedTasksForTesting();
}

TYPED_TEST(SequenceBoundTest, EmplaceOverExisting) {
  SEQUENCE_BOUND_T<BoxedValue> value(this->background_task_runner_, 8,
                                     &this->logger_);
  EXPECT_FALSE(value.is_null());
  value.WrappedEmplace(this->background_task_runner_, 9, &this->logger_);
  value.AsyncCall(&BoxedValue::value)
      .Then(BindLambdaForTesting(
          [&](int actual_value) { EXPECT_EQ(9, actual_value); }));
  value.FlushPostedTasksForTesting();

  if constexpr (TypeParam::kManagingTaskRunnerConstructsT) {
    // Both the replaced `BoxedValue` and the current `BoxedValue` should
    // live on the same sequence: make sure the replaced `BoxedValue` was
    // destroyed before the current `BoxedValue` was constructed.
    EXPECT_THAT(this->logger_.TakeEvents(),
                ::testing::ElementsAre(
                    "constructed BoxedValue = 8", "destroyed BoxedValue = 8",
                    "constructed BoxedValue = 9", "accessed BoxedValue = 9"));
  } else {
    // When `SequenceBound` manages a `std::unique_ptr<T>`, `T` is constructed
    // on the current sequence so construction of the new managed instance will
    // happen before the previously-managed instance is destroyed on the
    // managing task runner.
    EXPECT_THAT(this->logger_.TakeEvents(),
                ::testing::ElementsAre(
                    "constructed BoxedValue = 8", "constructed BoxedValue = 9",
                    "destroyed BoxedValue = 8", "accessed BoxedValue = 9"));
  }
}

TYPED_TEST(SequenceBoundTest, EmplaceOverExistingWithTaskRunnerSwap) {
  scoped_refptr<SequencedTaskRunner> another_task_runner =
      ThreadPool::CreateSequencedTaskRunner({});
  // No `EventLogger` here since destruction of the old `BoxedValue` and
  // construction of the new `BoxedValue` take place on different sequences and
  // can arbitrarily race.
  SEQUENCE_BOUND_T<BoxedValue> value(another_task_runner, 8);
  EXPECT_FALSE(value.is_null());
  value.WrappedEmplace(this->background_task_runner_, 9);
  {
    value.PostTaskWithThisObject(BindLambdaForTesting(
        [another_task_runner,
         background_task_runner =
             this->background_task_runner_](const BoxedValue& boxed_value) {
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
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
  }
  void ConstMethod() const {
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
  }

  void set_loop(RunLoop* loop) { loop_ = loop; }

 private:
  mutable raw_ptr<RunLoop> loop_ = nullptr;
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
    method_called_with_ = nullptr;
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
  }
  void ConstMethod(int x) const {
    *const_method_called_with_ = x;
    const_method_called_with_ = nullptr;
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
  }

  void set_loop(RunLoop* loop) { loop_ = loop; }

 private:
  raw_ptr<int> method_called_with_;
  mutable raw_ptr<int> const_method_called_with_;
  mutable raw_ptr<RunLoop> loop_ = nullptr;
};

class IntArgIntReturn {
 public:
  int Method(int x) { return -x; }
  int ConstMethod(int x) const { return -x; }
};

}  // namespace

TYPED_TEST(SequenceBoundTest, AsyncCallNoArgsNoThen) {
  SEQUENCE_BOUND_T<NoArgsVoidReturn> s(this->background_task_runner_);

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

TYPED_TEST(SequenceBoundTest, AsyncCallIntArgNoThen) {
  int method_called_with = 0;
  int const_method_called_with = 0;
  SEQUENCE_BOUND_T<IntArgVoidReturn> s(this->background_task_runner_,
                                       &method_called_with,
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

TYPED_TEST(SequenceBoundTest, AsyncCallNoArgsVoidThen) {
  SEQUENCE_BOUND_T<NoArgsVoidReturn> s(this->background_task_runner_);

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::Method).Then(BindLambdaForTesting([&] {
      loop.Quit();
    }));
    loop.Run();
  }

  {
    RunLoop loop;
    s.AsyncCall(&NoArgsVoidReturn::ConstMethod).Then(BindLambdaForTesting([&] {
      loop.Quit();
    }));
    loop.Run();
  }
}

TYPED_TEST(SequenceBoundTest, AsyncCallNoArgsIntThen) {
  SEQUENCE_BOUND_T<NoArgsIntReturn> s(this->background_task_runner_);

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

TYPED_TEST(SequenceBoundTest, AsyncCallWithArgsVoidThen) {
  int method_called_with = 0;
  int const_method_called_with = 0;
  SEQUENCE_BOUND_T<IntArgVoidReturn> s(this->background_task_runner_,
                                       &method_called_with,
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

TYPED_TEST(SequenceBoundTest, AsyncCallWithArgsIntThen) {
  SEQUENCE_BOUND_T<IntArgIntReturn> s(this->background_task_runner_);

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

TYPED_TEST(SequenceBoundTest, AsyncCallIsConstQualified) {
  // Tests that both const and non-const methods may be called through a
  // const-qualified SequenceBound.
  const SEQUENCE_BOUND_T<NoArgsVoidReturn> s(this->background_task_runner_);
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
      loop_ = nullptr;
    }
    if (called_) {
      *called_ = true;
      called_ = nullptr;
    }
    return 0;
  }

  int Method() {
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
    if (called_) {
      *called_ = true;
      called_ = nullptr;
    }
    return 0;
  }

 private:
  mutable raw_ptr<RunLoop> loop_ = nullptr;
  mutable raw_ptr<bool> called_ = nullptr;
};

TYPED_TEST(SequenceBoundTest, AsyncCallIgnoreResultNoArgs) {
  {
    RunLoop loop;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithNoArgs> s(
        this->background_task_runner_, &loop, nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod));
    loop.Run();
  }

  {
    RunLoop loop;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithNoArgs> s(
        this->background_task_runner_, &loop, nullptr);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::Method));
    loop.Run();
  }
}

TYPED_TEST(SequenceBoundTest, AsyncCallIgnoreResultThen) {
  {
    RunLoop loop;
    bool called = false;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithNoArgs> s(
        this->background_task_runner_, nullptr, &called);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::ConstMethod))
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_TRUE(called);
  }

  {
    RunLoop loop;
    bool called = false;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithNoArgs> s(
        this->background_task_runner_, nullptr, &called);
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithNoArgs::Method))
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_TRUE(called);
  }
}

class IgnoreResultTestHelperWithArgs {
 public:
  IgnoreResultTestHelperWithArgs(RunLoop* loop, int& value)
      : loop_(loop), value_(&value) {}

  int ConstMethod(int arg) const {
    if (value_) {
      *value_ = arg;
      value_ = nullptr;
    }
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
    return arg;
  }

  int Method(int arg) {
    if (value_) {
      *value_ = arg;
      value_ = nullptr;
    }
    if (loop_) {
      loop_->Quit();
      loop_ = nullptr;
    }
    return arg;
  }

 private:
  mutable raw_ptr<RunLoop> loop_ = nullptr;
  mutable raw_ptr<int> value_;
};

TYPED_TEST(SequenceBoundTest, AsyncCallIgnoreResultWithArgs) {
  {
    RunLoop loop;
    int result = 0;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithArgs> s(
        this->background_task_runner_, &loop, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60);
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithArgs> s(
        this->background_task_runner_, &loop, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::Method))
        .WithArgs(06);
    loop.Run();
    EXPECT_EQ(06, result);
  }
}

TYPED_TEST(SequenceBoundTest, AsyncCallIgnoreResultWithArgsThen) {
  {
    RunLoop loop;
    int result = 0;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithArgs> s(
        this->background_task_runner_, nullptr, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::ConstMethod))
        .WithArgs(60)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(60, result);
  }

  {
    RunLoop loop;
    int result = 0;
    SEQUENCE_BOUND_T<IgnoreResultTestHelperWithArgs> s(
        this->background_task_runner_, nullptr, std::ref(result));
    s.AsyncCall(IgnoreResult(&IgnoreResultTestHelperWithArgs::Method))
        .WithArgs(06)
        .Then(BindLambdaForTesting([&] { loop.Quit(); }));
    loop.Run();
    EXPECT_EQ(06, result);
  }
}

// TODO(crbug.com/40245687): Maybe use the nocompile harness here instead
// of being "clever"...
TYPED_TEST(SequenceBoundTest, NoCompileTests) {
  // TODO(crbug.com/40245687): Test calling WithArgs() on a method that
  // takes no arguments.
  //
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
  // TODO(crbug.com/40245687): Test calling Then() before calling
  // WithArgs().
  //
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
  // TODO(crbug.com/40245687): Add no-compile tests for converting
  // between SequenceBound<T> and SequenceBound<std::unique_ptr<T>>.
}
#undef SequenceBound

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
      SequencedTaskRunner::GetCurrentDefault();
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
