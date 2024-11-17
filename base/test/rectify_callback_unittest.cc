// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/rectify_callback.h"

#include <sstream>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Test rectifying functions with no return value.

#define CALLBACK_TESTS_VOID(TestNameSuffix, Signature, LambdaToRectify, ...)  \
  TEST(RectifyCallbackTest, RepeatingCallbackSignatureVoid##TestNameSuffix) { \
    auto cb = RectifyCallback<Signature>(BindRepeating(LambdaToRectify));     \
    cb.Run(__VA_ARGS__);                                                      \
  }                                                                           \
  TEST(RectifyCallbackTest, RepeatingCallbackTypeVoid##TestNameSuffix) {      \
    auto cb = RectifyCallback<RepeatingCallback<Signature>>(                  \
        BindRepeating(LambdaToRectify));                                      \
    cb.Run(__VA_ARGS__);                                                      \
  }                                                                           \
  TEST(RectifyCallbackTest, RepeatingToOnceVoid##TestNameSuffix) {            \
    auto cb = RectifyCallback<OnceCallback<Signature>>(                       \
        BindRepeating(LambdaToRectify));                                      \
    std::move(cb).Run(__VA_ARGS__);                                           \
  }                                                                           \
  TEST(RectifyCallbackTest, OnceCallbackSignatureVoid##TestNameSuffix) {      \
    auto cb = RectifyCallback<Signature>(BindOnce(LambdaToRectify));          \
    std::move(cb).Run(__VA_ARGS__);                                           \
  }                                                                           \
  TEST(RectifyCallbackTest, OnceCallbackTypeVoid##TestNameSuffix) {           \
    auto cb =                                                                 \
        RectifyCallback<OnceCallback<Signature>>(BindOnce(LambdaToRectify));  \
    std::move(cb).Run(__VA_ARGS__);                                           \
  }

CALLBACK_TESTS_VOID(NoArgsSameSignature, void(), [] {})

CALLBACK_TESTS_VOID(OneArgRemoveOneArg, void(int), [] {}, 1)

CALLBACK_TESTS_VOID(TwoArgsRemoveTwoArgs, void(float, int), [] {}, 0.25f, 1)

CALLBACK_TESTS_VOID(
    OneArgSameSignature,
    void(int),
    [](int x) { EXPECT_EQ(1, x); },
    1)

CALLBACK_TESTS_VOID(
    TwoArgsRemoveOneArg,
    void(float, int),
    [](int x) { EXPECT_EQ(1, x); },
    0.25f,
    1)

CALLBACK_TESTS_VOID(
    ThreeArgsRemoveTwoArgs,
    void(bool, float, int),
    [](int x) { EXPECT_EQ(1, x); },
    true,
    0.25f,
    1)

// Test rectifying functions that do return a value.

#define CALLBACK_TESTS_RETURN(TestNameSuffix, Signature, LambdaToRectify,    \
                              ExpectedReturn, ...)                           \
  TEST(RectifyCallbackTest,                                                  \
       RepeatingCallbackSignatureReturn##TestNameSuffix) {                   \
    auto cb = RectifyCallback<Signature>(BindRepeating(LambdaToRectify));    \
    EXPECT_EQ(ExpectedReturn, cb.Run(__VA_ARGS__));                          \
  }                                                                          \
  TEST(RectifyCallbackTest, RepeatingCallbackTypeReturn##TestNameSuffix) {   \
    auto cb = RectifyCallback<RepeatingCallback<Signature>>(                 \
        BindRepeating(LambdaToRectify));                                     \
    EXPECT_EQ(ExpectedReturn, cb.Run(__VA_ARGS__));                          \
  }                                                                          \
  TEST(RectifyCallbackTest, RepeatingToOnceReturn##TestNameSuffix) {         \
    auto cb = RectifyCallback<OnceCallback<Signature>>(                      \
        BindRepeating(LambdaToRectify));                                     \
    EXPECT_EQ(ExpectedReturn, std::move(cb).Run(__VA_ARGS__));               \
  }                                                                          \
  TEST(RectifyCallbackTest, OnceCallbackSignatureReturn##TestNameSuffix) {   \
    auto cb = RectifyCallback<Signature>(BindOnce(LambdaToRectify));         \
    EXPECT_EQ(ExpectedReturn, std::move(cb).Run(__VA_ARGS__));               \
  }                                                                          \
  TEST(RectifyCallbackTest, OnceCallbackTypeReturn##TestNameSuffix) {        \
    auto cb =                                                                \
        RectifyCallback<OnceCallback<Signature>>(BindOnce(LambdaToRectify)); \
    EXPECT_EQ(ExpectedReturn, std::move(cb).Run(__VA_ARGS__));               \
  }

CALLBACK_TESTS_RETURN(NoArgsSameSignature, int(), [] { return 2; }, 2)

CALLBACK_TESTS_RETURN(OneArgRemoveOneArg, int(int), [] { return 2; }, 2, 1)

CALLBACK_TESTS_RETURN(
    TwoArgsRemoveTwoArgs,
    int(float, int),
    [] { return 2; },
    2,
    0.25f,
    1)

CALLBACK_TESTS_RETURN(
    OneArgSameSignature,
    int(int),
    [](int x) { return x; },
    2,
    2)

CALLBACK_TESTS_RETURN(
    TwoArgsRemoveOneArg,
    int(float, int),
    [](int x) { return x; },
    2,
    0.25f,
    2)

CALLBACK_TESTS_RETURN(
    ThreeArgsRemoveTwoArgs,
    int(bool, float, int),
    [](int x) { return x; },
    2,
    true,
    0.25f,
    2)

// Test proper forwarding of move-only arguments.

CALLBACK_TESTS_RETURN(
    DiscardsMoveOnlyArgs,
    bool(int, std::unique_ptr<int>),
    [] { return true; },
    true,
    2,
    std::make_unique<int>(3))

CALLBACK_TESTS_RETURN(
    UsesMoveOnlyArg,
    int(float, int, std::unique_ptr<int>),
    [](std::unique_ptr<int> p) { return *p; },
    3,
    0.25f,
    2,
    std::make_unique<int>(3))

// Test rectifying DoNothing().

#define CALLBACK_TESTS_DO_NOTHING(TestNameSuffix, Signature, ...)         \
  TEST(RectifyCallbackTest, SignatureDoNothing##TestNameSuffix) {         \
    auto cb = RectifyCallback<Signature>(DoNothing());                    \
    cb.Run(__VA_ARGS__);                                                  \
  }                                                                       \
  TEST(RectifyCallbackTest, RepeatingCallbackDoNothing##TestNameSuffix) { \
    auto cb = RectifyCallback<RepeatingCallback<Signature>>(DoNothing()); \
    cb.Run(__VA_ARGS__);                                                  \
  }                                                                       \
  TEST(RectifyCallbackTest, OnceCallbackDoNothing##TestNameSuffix) {      \
    auto cb = RectifyCallback<OnceCallback<Signature>>(DoNothing());      \
    std::move(cb).Run(__VA_ARGS__);                                       \
  }

CALLBACK_TESTS_DO_NOTHING(NoArgs, void())
CALLBACK_TESTS_DO_NOTHING(OneArg, void(int), 2)
CALLBACK_TESTS_DO_NOTHING(TwoArgs, void(float, int), 0.25, 2)
CALLBACK_TESTS_DO_NOTHING(ThreeArgs, void(bool, float, int), false, 0.25, 2)

// Test passing callbacks to RectifyCallback() in different ways.

TEST(RectifyCallbackTest, RepeatingMove) {
  auto cb = BindRepeating([](int x) { return x != 0; });
  auto cb2 = RectifyCallback<bool(float, int)>(std::move(cb));
  EXPECT_EQ(true, cb2.Run(1.0, 1));
  EXPECT_EQ(false, cb2.Run(1.0, 0));
}

TEST(RectifyCallbackTest, RepeatingCopy) {
  const auto cb = BindRepeating([](int x) { return x != 0; });
  auto cb2 = RectifyCallback<bool(float, int)>(cb);
  EXPECT_EQ(true, cb2.Run(1.0, 1));
  EXPECT_EQ(false, cb2.Run(1.0, 0));
}

TEST(RectifyCallbackTest, RepeatingConstReference) {
  const auto cb = BindRepeating([](int x) { return x != 0; });
  const auto& cb_ref = cb;
  auto cb2 = RectifyCallback<bool(float, int)>(cb_ref);
  EXPECT_EQ(true, cb2.Run(1.0, 1));
  EXPECT_EQ(false, cb2.Run(1.0, 0));
}

TEST(RectifyCallbackTest, OnceMove) {
  auto cb = BindOnce([](int x) { return x != 0; });
  auto cb2 = RectifyCallback<bool(float, int)>(std::move(cb));
  EXPECT_EQ(true, std::move(cb2).Run(1.0, 1));
}

TEST(RectifyCallbackTest, OnceFromRepeating) {
  auto cb = BindRepeating([](int x) { return x != 0; });
  auto cb2 = RectifyCallback<OnceCallback<bool(float, int)>>(std::move(cb));
  EXPECT_EQ(true, std::move(cb2).Run(1.0, 1));
}

// Test that we can write a function that can rectify its callback argument into
// the signature it wants.
//
// If this is implemented incorrectly, it can result in strange/bad behavior,
// even if it manages to compile.

namespace {

using ExampleRepeatingTargetType = RepeatingCallback<bool(double, int)>;
using ExampleOnceTargetType = OnceCallback<bool(double, int)>;

// This is the call we'll be rectifying into the expected signature.
bool ExampleTargetFunction(int x) {
  return x != 0;
}

// Base version of the function that takes a repeating callback.
// This is the actual implementation.
void ExampleFunctionRepeatingCallback(ExampleRepeatingTargetType callback) {
  EXPECT_EQ(true, std::move(callback).Run(1.0, 1));
}

// Template version of the function that wants a repeating callback; it will
// rectify its input and call the base version.
//
// If the rectify goes awry (e.g. the wrong kind of callback is generated),
// this can result in an infinite loop.
template <typename T>
void ExampleFunctionRepeatingCallback(T&& callback) {
  ExampleFunctionRepeatingCallback(
      RectifyCallback<ExampleRepeatingTargetType>(std::forward<T>(callback)));
}

// Base version of the function that takes a once callback.
// This is the actual implementation.
void ExampleFunctionOnceCallback(ExampleOnceTargetType callback) {
  EXPECT_EQ(true, std::move(callback).Run(1.0, 1));
}

// Template version of the function that wants a once callback; it will
// rectify its input and call the base version.
//
// If the rectify goes awry (e.g. the wrong kind of callback is generated),
// this can result in an infinite loop.
template <typename T>
void ExampleFunctionOnceCallback(T&& callback) {
  ExampleFunctionOnceCallback(
      RectifyCallback<ExampleOnceTargetType>(std::forward<T>(callback)));
}

}  // namespace

TEST(RectifyCallbackTest, TemplateOverloadRectifiesOnceCallback) {
  ExampleFunctionOnceCallback(BindOnce(&ExampleTargetFunction));
  ExampleFunctionOnceCallback(BindOnce([] { return true; }));
  ExampleFunctionOnceCallback(BindOnce([](double d, int i) { return d && i; }));
  auto cb = BindOnce(&ExampleTargetFunction);
  ExampleFunctionOnceCallback(std::move(cb));
}

TEST(RectifyCallbackTest, TemplateOverloadRectifiesRepeatingCallback) {
  ExampleFunctionOnceCallback(BindRepeating(&ExampleTargetFunction));
  ExampleFunctionOnceCallback(BindRepeating([] { return true; }));
  ExampleFunctionOnceCallback(
      BindRepeating([](double d, int i) { return d && i; }));
  auto cb = BindRepeating(&ExampleTargetFunction);
  ExampleFunctionOnceCallback(cb);
  bool result = true;
  ExampleFunctionOnceCallback(BindLambdaForTesting([&] { return result; }));
}

TEST(RectifyCallbackTest, TemplateOverloadCoerceRepeatingTarget) {
  ExampleFunctionRepeatingCallback(BindRepeating(&ExampleTargetFunction));
  ExampleFunctionRepeatingCallback(BindRepeating([] { return true; }));
  ExampleFunctionRepeatingCallback(
      BindRepeating([](double d, int i) { return d && i; }));
  auto cb = BindRepeating(&ExampleTargetFunction);
  ExampleFunctionRepeatingCallback(cb);
  bool result = true;
  ExampleFunctionRepeatingCallback(
      BindLambdaForTesting([&] { return result; }));
}

TEST(RectifyCallbackTest, NullCallbackPassthrough) {
  {
    OnceCallback<void()> once;
    RepeatingCallback<void()> repeating;
    EXPECT_TRUE(RectifyCallback<void()>(std::move(once)).is_null());
    EXPECT_TRUE(RectifyCallback<void()>(repeating).is_null());
    EXPECT_TRUE(RectifyCallback<void()>(NullCallback()).is_null());
  }

  {
    OnceCallback<void()> once;
    RepeatingCallback<void()> repeating;
    EXPECT_TRUE(RectifyCallback<void(int)>(std::move(once)).is_null());
    EXPECT_TRUE(RectifyCallback<void(int)>(repeating).is_null());
    EXPECT_TRUE(RectifyCallback<void(int)>(NullCallback()).is_null());
  }
}

}  // namespace base
