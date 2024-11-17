// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_internal.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

void NopInvokeFunc() {}

// White-box testpoints to inject into a callback object for checking
// comparators and emptiness APIs. Use a BindState that is specialized based on
// a type we declared in the anonymous namespace above to remove any chance of
// colliding with another instantiation and breaking the one-definition-rule.
struct FakeBindState : internal::BindStateBase {
  FakeBindState() : BindStateBase(&NopInvokeFunc, &Destroy) {}

 private:
  ~FakeBindState() = default;
  static void Destroy(const internal::BindStateBase* self) {
    delete static_cast<const FakeBindState*>(self);
  }
};

namespace {

class CallbackTest : public ::testing::Test {
 public:
  CallbackTest()
      : callback_a_(new FakeBindState()), callback_b_(new FakeBindState()) {}

  ~CallbackTest() override = default;

 protected:
  RepeatingCallback<void()> callback_a_;
  const RepeatingCallback<void()> callback_b_;  // Ensure APIs work with const.
  RepeatingCallback<void()> null_callback_;
};

TEST_F(CallbackTest, Types) {
  static_assert(std::is_same_v<void, OnceClosure::ResultType>, "");
  static_assert(std::is_same_v<void(), OnceClosure::RunType>, "");

  using OnceCallbackT = OnceCallback<double(int, char)>;
  static_assert(std::is_same_v<double, OnceCallbackT::ResultType>, "");
  static_assert(std::is_same_v<double(int, char), OnceCallbackT::RunType>, "");

  static_assert(std::is_same_v<void, RepeatingClosure::ResultType>, "");
  static_assert(std::is_same_v<void(), RepeatingClosure::RunType>, "");

  using RepeatingCallbackT = RepeatingCallback<bool(float, short)>;
  static_assert(std::is_same_v<bool, RepeatingCallbackT::ResultType>, "");
  static_assert(std::is_same_v<bool(float, short), RepeatingCallbackT::RunType>,
                "");
}

// Ensure we can create unbound callbacks. We need this to be able to store
// them in class members that can be initialized later.
TEST_F(CallbackTest, DefaultConstruction) {
  RepeatingCallback<void()> c0;
  RepeatingCallback<void(int)> c1;
  RepeatingCallback<void(int, int)> c2;
  RepeatingCallback<void(int, int, int)> c3;
  RepeatingCallback<void(int, int, int, int)> c4;
  RepeatingCallback<void(int, int, int, int, int)> c5;
  RepeatingCallback<void(int, int, int, int, int, int)> c6;

  EXPECT_TRUE(c0.is_null());
  EXPECT_TRUE(c1.is_null());
  EXPECT_TRUE(c2.is_null());
  EXPECT_TRUE(c3.is_null());
  EXPECT_TRUE(c4.is_null());
  EXPECT_TRUE(c5.is_null());
  EXPECT_TRUE(c6.is_null());
}

TEST_F(CallbackTest, IsNull) {
  EXPECT_TRUE(null_callback_.is_null());
  EXPECT_FALSE(callback_a_.is_null());
  EXPECT_FALSE(callback_b_.is_null());
}

TEST_F(CallbackTest, Equals) {
  EXPECT_EQ(callback_a_, callback_a_);
  EXPECT_NE(callback_a_, callback_b_);
  EXPECT_NE(callback_b_, callback_a_);

  // We should compare based on instance, not type.
  RepeatingCallback<void()> callback_c(new FakeBindState());
  RepeatingCallback<void()> callback_a2 = callback_a_;
  EXPECT_EQ(callback_a_, callback_a2);
  EXPECT_NE(callback_a_, callback_c);

  // Empty, however, is always equal to empty.
  RepeatingCallback<void()> empty2;
  EXPECT_EQ(null_callback_, empty2);
}

TEST_F(CallbackTest, Reset) {
  // Resetting should bring us back to empty.
  ASSERT_FALSE(callback_a_.is_null());
  EXPECT_NE(callback_a_, null_callback_);

  callback_a_.Reset();

  EXPECT_TRUE(callback_a_.is_null());
  EXPECT_EQ(callback_a_, null_callback_);
}

TEST_F(CallbackTest, Move) {
  // Moving should reset the callback.
  ASSERT_FALSE(callback_a_.is_null());
  EXPECT_NE(callback_a_, null_callback_);

  auto tmp = std::move(callback_a_);

  EXPECT_TRUE(callback_a_.is_null());
  EXPECT_EQ(callback_a_, null_callback_);
}

TEST_F(CallbackTest, NullAfterMoveRun) {
  RepeatingCallback<void(void*)> cb = BindRepeating([](void* param) {
    EXPECT_TRUE(static_cast<RepeatingCallback<void(void*)>*>(param)->is_null());
  });
  ASSERT_TRUE(cb);
  std::move(cb).Run(&cb);
  EXPECT_FALSE(cb);

  const RepeatingClosure cb2 = BindRepeating([] {});
  ASSERT_TRUE(cb2);
  std::move(cb2).Run();
  EXPECT_TRUE(cb2);

  OnceCallback<void(void*)> cb3 = BindOnce([](void* param) {
    EXPECT_TRUE(static_cast<OnceCallback<void(void*)>*>(param)->is_null());
  });
  ASSERT_TRUE(cb3);
  std::move(cb3).Run(&cb3);
  EXPECT_FALSE(cb3);
}

TEST_F(CallbackTest, MaybeValidReturnsTrue) {
  RepeatingCallback<void()> cb = BindRepeating([] {});
  // By default, MaybeValid() just returns true all the time.
  EXPECT_TRUE(cb.MaybeValid());
  cb.Run();
  EXPECT_TRUE(cb.MaybeValid());
}

TEST_F(CallbackTest, ThenResetsOriginalCallback) {
  {
    // OnceCallback::Then() always destroys the original callback.
    OnceClosure orig = base::BindOnce([] {});
    EXPECT_TRUE(!!orig);
    OnceClosure joined = std::move(orig).Then(base::BindOnce([] {}));
    EXPECT_TRUE(!!joined);
    EXPECT_FALSE(!!orig);
  }
  {
    // RepeatingCallback::Then() destroys the original callback if it's an
    // rvalue.
    RepeatingClosure orig = base::BindRepeating([] {});
    EXPECT_TRUE(!!orig);
    RepeatingClosure joined = std::move(orig).Then(base::BindRepeating([] {}));
    EXPECT_TRUE(!!joined);
    EXPECT_FALSE(!!orig);
  }
  {
    // RepeatingCallback::Then() doesn't destroy the original callback if it's
    // not an rvalue.
    RepeatingClosure orig = base::BindRepeating([] {});
    RepeatingClosure copy = orig;
    EXPECT_TRUE(!!orig);
    RepeatingClosure joined = orig.Then(base::BindRepeating([] {}));
    EXPECT_TRUE(!!joined);
    EXPECT_TRUE(!!orig);
    // The original callback is not changed.
    EXPECT_EQ(orig, copy);
    EXPECT_NE(joined, copy);
  }
}

// A RepeatingCallback will implicitly convert to a OnceCallback, so a
// once_callback.Then(repeating_callback) should turn into a OnceCallback
// that holds 2 OnceCallbacks which it will run.
TEST_F(CallbackTest, ThenCanConvertRepeatingToOnce) {
  {
    RepeatingClosure repeating_closure = base::BindRepeating([] {});
    OnceClosure once_closure = base::BindOnce([] {});
    std::move(once_closure).Then(repeating_closure).Run();

    RepeatingCallback<int(int)> repeating_callback =
        base::BindRepeating([](int i) { return i + 1; });
    OnceCallback<int(int)> once_callback =
        base::BindOnce([](int i) { return i * 2; });
    EXPECT_EQ(3, std::move(once_callback).Then(repeating_callback).Run(1));
  }
  {
    RepeatingClosure repeating_closure = base::BindRepeating([] {});
    OnceClosure once_closure = base::BindOnce([] {});
    std::move(once_closure).Then(std::move(repeating_closure)).Run();

    RepeatingCallback<int(int)> repeating_callback =
        base::BindRepeating([](int i) { return i + 1; });
    OnceCallback<int(int)> once_callback =
        base::BindOnce([](int i) { return i * 2; });
    EXPECT_EQ(
        3, std::move(once_callback).Then(std::move(repeating_callback)).Run(1));
  }
}

// `Then()` should should allow a return value of type `R` to be passed to a
// callback with one parameter of type `const R&` or type `R&&`.
TEST_F(CallbackTest, ThenWithCompatibleButNotSameType) {
  {
    OnceCallback<std::string()> once_callback =
        BindOnce([] { return std::string("hello"); });
    EXPECT_EQ("hello",
              std::move(once_callback)
                  .Then(BindOnce([](const std::string& s) { return s; }))
                  .Run());
  }

  class NotCopied {
   public:
    NotCopied() = default;
    NotCopied(NotCopied&&) = default;
    NotCopied& operator=(NotCopied&&) = default;

    NotCopied(const NotCopied&) {
      ADD_FAILURE() << "should not have been copied";
    }

    NotCopied& operator=(const NotCopied&) {
      ADD_FAILURE() << "should not have been copied";
      return *this;
    }
  };

  {
    OnceCallback<NotCopied()> once_callback =
        BindOnce([] { return NotCopied(); });
    std::move(once_callback).Then(BindOnce([](const NotCopied&) {})).Run();
  }

  {
    OnceCallback<NotCopied()> once_callback =
        BindOnce([] { return NotCopied(); });
    std::move(once_callback).Then(BindOnce([](NotCopied&&) {})).Run();
  }
}

// A factory class for building an outer and inner callback for calling
// Then() on either a OnceCallback or RepeatingCallback with combinations of
// void return types, non-void, and move-only return types.
template <bool use_once, typename R, typename ThenR, typename... Args>
class CallbackThenTest;
template <bool use_once, typename R, typename ThenR, typename... Args>
class CallbackThenTest<use_once, R(Args...), ThenR> {
 public:
  using CallbackType =
      typename std::conditional<use_once,
                                OnceCallback<R(Args...)>,
                                RepeatingCallback<R(Args...)>>::type;
  using ThenType =
      typename std::conditional<use_once, OnceClosure, RepeatingClosure>::type;

  // Gets the Callback that will have Then() called on it. Has a return type
  // of `R`, which would chain to the inner callback for Then(). Has inputs of
  // type `Args...`.
  static auto GetOuter(std::string& s) {
    s = "";
    return Bind(
        [](std::string* s, Args... args) {
          return Outer(s, std::forward<Args>(args)...);
        },
        &s);
  }
  // Gets the Callback that will be passed to Then(). Has a return type of
  // `ThenR`, specified for the class instance. Receives as input the return
  // type `R` from the function bound and returned in GetOuter().
  static auto GetInner(std::string& s) { return Bind(&Inner<R, ThenR>, &s); }

 private:
  template <bool bind_once = use_once,
            typename F,
            typename... FArgs,
            std::enable_if_t<bind_once, int> = 0>
  static auto Bind(F function, FArgs... args) {
    return BindOnce(function, std::forward<FArgs>(args)...);
  }
  template <bool bind_once = use_once,
            typename F,
            typename... FArgs,
            std::enable_if_t<!bind_once, int> = 0>
  static auto Bind(F function, FArgs... args) {
    return BindRepeating(function, std::forward<FArgs>(args)...);
  }

  template <typename R2 = R, std::enable_if_t<!std::is_void_v<R2>, int> = 0>
  static int Outer(std::string* s,
                   std::unique_ptr<int> a,
                   std::unique_ptr<int> b) {
    *s += "Outer";
    *s += base::NumberToString(*a) + base::NumberToString(*b);
    return *a + *b;
  }
  template <typename R2 = R, std::enable_if_t<!std::is_void_v<R2>, int> = 0>
  static int Outer(std::string* s, int a, int b) {
    *s += "Outer";
    *s += base::NumberToString(a) + base::NumberToString(b);
    return a + b;
  }
  template <typename R2 = R, std::enable_if_t<!std::is_void_v<R2>, int> = 0>
  static int Outer(std::string* s) {
    *s += "Outer";
    *s += "None";
    return 99;
  }

  template <typename R2 = R, std::enable_if_t<std::is_void_v<R2>, int> = 0>
  static void Outer(std::string* s,
                    std::unique_ptr<int> a,
                    std::unique_ptr<int> b) {
    *s += "Outer";
    *s += base::NumberToString(*a) + base::NumberToString(*b);
  }
  template <typename R2 = R, std::enable_if_t<std::is_void_v<R2>, int> = 0>
  static void Outer(std::string* s, int a, int b) {
    *s += "Outer";
    *s += base::NumberToString(a) + base::NumberToString(b);
  }
  template <typename R2 = R, std::enable_if_t<std::is_void_v<R2>, int> = 0>
  static void Outer(std::string* s) {
    *s += "Outer";
    *s += "None";
  }

  template <typename OuterR,
            typename InnerR,
            std::enable_if_t<!std::is_void_v<OuterR>, int> = 0,
            std::enable_if_t<!std::is_void_v<InnerR>, int> = 0>
  static int Inner(std::string* s, OuterR a) {
    static_assert(std::is_same_v<InnerR, int>, "Use int return type");
    *s += "Inner";
    *s += base::NumberToString(a);
    return a;
  }
  template <typename OuterR,
            typename InnerR,
            std::enable_if_t<std::is_void_v<OuterR>, int> = 0,
            std::enable_if_t<!std::is_void_v<InnerR>, int> = 0>
  static int Inner(std::string* s) {
    static_assert(std::is_same_v<InnerR, int>, "Use int return type");
    *s += "Inner";
    *s += "None";
    return 99;
  }

  template <typename OuterR,
            typename InnerR,
            std::enable_if_t<!std::is_void_v<OuterR>, int> = 0,
            std::enable_if_t<std::is_void_v<InnerR>, int> = 0>
  static void Inner(std::string* s, OuterR a) {
    *s += "Inner";
    *s += base::NumberToString(a);
  }
  template <typename OuterR,
            typename InnerR,
            std::enable_if_t<std::is_void_v<OuterR>, int> = 0,
            std::enable_if_t<std::is_void_v<InnerR>, int> = 0>
  static void Inner(std::string* s) {
    *s += "Inner";
    *s += "None";
  }
};

template <typename R, typename ThenR = void, typename... Args>
using CallbackThenOnceTest = CallbackThenTest<true, R, ThenR, Args...>;
template <typename R, typename ThenR = void, typename... Args>
using CallbackThenRepeatingTest = CallbackThenTest<false, R, ThenR, Args...>;

TEST_F(CallbackTest, ThenOnce) {
  std::string s;

  // Void return from outer + void return from Then().
  {
    using VoidReturnWithoutArgs = void();
    using ThenReturn = void;
    using Test = CallbackThenOnceTest<VoidReturnWithoutArgs, ThenReturn>;
    Test::GetOuter(s).Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInnerNone");
  }
  {
    using VoidReturnWithArgs = void(int, int);
    using ThenReturn = void;
    using Test = CallbackThenOnceTest<VoidReturnWithArgs, ThenReturn>;
    Test::GetOuter(s).Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12InnerNone");
  }
  {
    using VoidReturnWithMoveOnlyArgs =
        void(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = void;
    using Test = CallbackThenOnceTest<VoidReturnWithMoveOnlyArgs, ThenReturn>;
    Test::GetOuter(s)
        .Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12InnerNone");
  }

  // Void return from outer + non-void return from Then().
  {
    using VoidReturnWithoutArgs = void();
    using ThenReturn = int;
    using Test = CallbackThenOnceTest<VoidReturnWithoutArgs, ThenReturn>;
    EXPECT_EQ(99, Test::GetOuter(s).Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInnerNone");
  }
  {
    using VoidReturnWithArgs = void(int, int);
    using ThenReturn = int;
    using Test = CallbackThenOnceTest<VoidReturnWithArgs, ThenReturn>;
    EXPECT_EQ(99, Test::GetOuter(s).Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12InnerNone");
  }
  {
    using VoidReturnWithMoveOnlyArgs =
        void(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = int;
    using Test = CallbackThenOnceTest<VoidReturnWithMoveOnlyArgs, ThenReturn>;
    EXPECT_EQ(99, Test::GetOuter(s)
                      .Then(Test::GetInner(s))
                      .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12InnerNone");
  }

  // Non-void return from outer + void return from Then().
  {
    using NonVoidReturnWithoutArgs = int();
    using ThenReturn = void;
    using Test = CallbackThenOnceTest<NonVoidReturnWithoutArgs, ThenReturn>;
    Test::GetOuter(s).Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInner99");
  }
  {
    using NonVoidReturnWithArgs = int(int, int);
    using ThenReturn = void;
    using Test = CallbackThenOnceTest<NonVoidReturnWithArgs, ThenReturn>;
    Test::GetOuter(s).Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12Inner3");
  }
  {
    using NonVoidReturnWithMoveOnlyArgs =
        int(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = void;
    using Test =
        CallbackThenOnceTest<NonVoidReturnWithMoveOnlyArgs, ThenReturn>;
    Test::GetOuter(s)
        .Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12Inner3");
  }

  // Non-void return from outer + non-void return from Then().
  {
    using NonVoidReturnWithoutArgs = int();
    using ThenReturn = int;
    using Test = CallbackThenOnceTest<NonVoidReturnWithoutArgs, ThenReturn>;
    EXPECT_EQ(99, Test::GetOuter(s).Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInner99");
  }
  {
    using NonVoidReturnWithArgs = int(int, int);
    using ThenReturn = int;
    using Test = CallbackThenOnceTest<NonVoidReturnWithArgs, ThenReturn>;
    EXPECT_EQ(3, Test::GetOuter(s).Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12Inner3");
  }
  {
    using NonVoidReturnWithMoveOnlyArgs =
        int(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = int;
    using Test =
        CallbackThenOnceTest<NonVoidReturnWithMoveOnlyArgs, ThenReturn>;
    EXPECT_EQ(3, Test::GetOuter(s)
                     .Then(Test::GetInner(s))
                     .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12Inner3");
  }
}

TEST_F(CallbackTest, ThenRepeating) {
  std::string s;

  // Void return from outer + void return from Then().
  {
    using VoidReturnWithoutArgs = void();
    using ThenReturn = void;
    using Test = CallbackThenRepeatingTest<VoidReturnWithoutArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInnerNone");
    std::move(outer).Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInnerNoneOuterNoneInnerNone");
  }
  {
    using VoidReturnWithArgs = void(int, int);
    using ThenReturn = void;
    using Test = CallbackThenRepeatingTest<VoidReturnWithArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12InnerNone");
    std::move(outer).Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12InnerNoneOuter12InnerNone");
  }
  {
    using VoidReturnWithMoveOnlyArgs =
        void(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = void;
    using Test =
        CallbackThenRepeatingTest<VoidReturnWithMoveOnlyArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12InnerNone");
    std::move(outer)
        .Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12InnerNoneOuter12InnerNone");
  }

  // Void return from outer + non-void return from Then().
  {
    using VoidReturnWithoutArgs = void();
    using ThenReturn = int;
    using Test = CallbackThenRepeatingTest<VoidReturnWithoutArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(99, outer.Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInnerNone");
    EXPECT_EQ(99, std::move(outer).Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInnerNoneOuterNoneInnerNone");
  }
  {
    using VoidReturnWithArgs = void(int, int);
    using ThenReturn = int;
    using Test = CallbackThenRepeatingTest<VoidReturnWithArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(99, outer.Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12InnerNone");
    EXPECT_EQ(99, std::move(outer).Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12InnerNoneOuter12InnerNone");
  }
  {
    using VoidReturnWithMoveOnlyArgs =
        void(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = int;
    using Test =
        CallbackThenRepeatingTest<VoidReturnWithMoveOnlyArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(99, outer.Then(Test::GetInner(s))
                      .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12InnerNone");
    EXPECT_EQ(99, std::move(outer)
                      .Then(Test::GetInner(s))
                      .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12InnerNoneOuter12InnerNone");
  }

  // Non-void return from outer + void return from Then().
  {
    using NonVoidReturnWithoutArgs = int();
    using ThenReturn = void;
    using Test =
        CallbackThenRepeatingTest<NonVoidReturnWithoutArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInner99");
    std::move(outer).Then(Test::GetInner(s)).Run();
    EXPECT_EQ(s, "OuterNoneInner99OuterNoneInner99");
  }
  {
    using NonVoidReturnWithArgs = int(int, int);
    using ThenReturn = void;
    using Test = CallbackThenRepeatingTest<NonVoidReturnWithArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12Inner3");
    std::move(outer).Then(Test::GetInner(s)).Run(1, 2);
    EXPECT_EQ(s, "Outer12Inner3Outer12Inner3");
  }
  {
    using NonVoidReturnWithMoveOnlyArgs =
        int(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = void;
    using Test =
        CallbackThenRepeatingTest<NonVoidReturnWithMoveOnlyArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    outer.Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12Inner3");
    std::move(outer)
        .Then(Test::GetInner(s))
        .Run(std::make_unique<int>(1), std::make_unique<int>(2));
    EXPECT_EQ(s, "Outer12Inner3Outer12Inner3");
  }

  // Non-void return from outer + non-void return from Then().
  {
    using NonVoidReturnWithoutArgs = int();
    using ThenReturn = int;
    using Test =
        CallbackThenRepeatingTest<NonVoidReturnWithoutArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(99, outer.Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInner99");
    EXPECT_EQ(99, std::move(outer).Then(Test::GetInner(s)).Run());
    EXPECT_EQ(s, "OuterNoneInner99OuterNoneInner99");
  }
  {
    using NonVoidReturnWithArgs = int(int, int);
    using ThenReturn = int;
    using Test = CallbackThenRepeatingTest<NonVoidReturnWithArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(3, outer.Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12Inner3");
    EXPECT_EQ(3, std::move(outer).Then(Test::GetInner(s)).Run(1, 2));
    EXPECT_EQ(s, "Outer12Inner3Outer12Inner3");
  }
  {
    using NonVoidReturnWithMoveOnlyArgs =
        int(std::unique_ptr<int>, std::unique_ptr<int>);
    using ThenReturn = int;
    using Test =
        CallbackThenRepeatingTest<NonVoidReturnWithMoveOnlyArgs, ThenReturn>;
    auto outer = Test::GetOuter(s);
    EXPECT_EQ(3, outer.Then(Test::GetInner(s))
                     .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12Inner3");
    EXPECT_EQ(3, std::move(outer)
                     .Then(Test::GetInner(s))
                     .Run(std::make_unique<int>(1), std::make_unique<int>(2)));
    EXPECT_EQ(s, "Outer12Inner3Outer12Inner3");
  }
}

// WeakPtr detection in BindRepeating() requires a method, not just any
// function.
class ClassWithAMethod {
 public:
  void TheMethod() { method_called = true; }

  bool method_called = false;
};

TEST_F(CallbackTest, MaybeValidInvalidateWeakPtrsOnSameSequence) {
  ClassWithAMethod obj;
  WeakPtrFactory<ClassWithAMethod> factory(&obj);
  WeakPtr<ClassWithAMethod> ptr = factory.GetWeakPtr();

  RepeatingCallback<void()> cb =
      BindRepeating(&ClassWithAMethod::TheMethod, ptr);
  EXPECT_TRUE(cb.MaybeValid());
  EXPECT_FALSE(cb.IsCancelled());

  factory.InvalidateWeakPtrs();
  // MaybeValid() should be false and IsCancelled() should become true because
  // InvalidateWeakPtrs() was called on the same thread.
  EXPECT_FALSE(cb.MaybeValid());
  EXPECT_TRUE(cb.IsCancelled());
  // is_null() is not affected by the invalidated WeakPtr.
  EXPECT_FALSE(cb.is_null());
}

TEST_F(CallbackTest, MaybeValidInvalidateWeakPtrsOnOtherSequence) {
  ClassWithAMethod obj;
  WeakPtrFactory<ClassWithAMethod> factory(&obj);
  WeakPtr<ClassWithAMethod> ptr = factory.GetWeakPtr();

  RepeatingCallback<void()> cb =
      BindRepeating(&ClassWithAMethod::TheMethod, ptr);
  EXPECT_TRUE(cb.MaybeValid());

  Thread other_thread("other_thread");
  other_thread.StartAndWaitForTesting();
  other_thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RepeatingCallback<void()> cb) {
            // Check that MaybeValid() _eventually_ returns false.
            const TimeDelta timeout = TestTimeouts::tiny_timeout();
            const TimeTicks begin = TimeTicks::Now();
            while (cb.MaybeValid() && (TimeTicks::Now() - begin) < timeout)
              PlatformThread::YieldCurrentThread();
            EXPECT_FALSE(cb.MaybeValid());
          },
          cb));
  factory.InvalidateWeakPtrs();
  // |other_thread|'s destructor will join, ensuring we wait for the task to be
  // run.
}

TEST_F(CallbackTest, ThenAfterWeakPtr) {
  ClassWithAMethod obj;
  WeakPtrFactory<ClassWithAMethod> factory(&obj);
  WeakPtr<ClassWithAMethod> ptr = factory.GetWeakPtr();

  // If the first callback of a chain is skipped due to InvalidateWeakPtrs(),
  // the remaining callbacks should still run.
  bool chained_closure_called = false;
  OnceClosure closure =
      BindOnce(&ClassWithAMethod::TheMethod, ptr)
          .Then(BindLambdaForTesting(
              [&chained_closure_called] { chained_closure_called = true; }));
  factory.InvalidateWeakPtrs();
  std::move(closure).Run();
  EXPECT_FALSE(obj.method_called);
  EXPECT_TRUE(chained_closure_called);
}

class CallbackOwner : public base::RefCounted<CallbackOwner> {
 public:
  explicit CallbackOwner(bool* deleted) {
    // WrapRefCounted() here is needed to avoid the check failure in the
    // BindRepeating implementation, that refuses to create the first reference
    // to ref-counted objects.
    callback_ = BindRepeating(&CallbackOwner::Unused, WrapRefCounted(this));
    deleted_ = deleted;
  }
  void Reset() {
    callback_.Reset();
    // We are deleted here if no-one else had a ref to us.
  }

 private:
  friend class base::RefCounted<CallbackOwner>;
  virtual ~CallbackOwner() { *deleted_ = true; }
  void Unused() { FAIL() << "Should never be called"; }

  RepeatingClosure callback_;
  raw_ptr<bool> deleted_;
};

TEST_F(CallbackTest, CallbackHasLastRefOnContainingObject) {
  bool deleted = false;
  CallbackOwner* owner = new CallbackOwner(&deleted);
  owner->Reset();
  ASSERT_TRUE(deleted);
}

// According to legends, it is good practice to put death tests into their own
// test suite, so they are grouped separately from regular tests, since death
// tests are somewhat slow and have quirks that can slow down test running if
// intermixed.
TEST(CallbackDeathTest, RunNullCallbackChecks) {
  {
    base::OnceClosure closure;
    EXPECT_CHECK_DEATH(std::move(closure).Run());
  }

  {
    base::RepeatingClosure closure;
    EXPECT_CHECK_DEATH(std::move(closure).Run());
  }

  {
    base::RepeatingClosure closure;
    EXPECT_CHECK_DEATH(closure.Run());
  }
}

}  // namespace
}  // namespace base
