// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/function_ref.h"

#include <stdint.h>

#include <concepts>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

namespace base {

namespace {

char Func(float) {
  return 'a';
}

}  // namespace

TEST(FunctionRef, Lambda) {
  auto add = [](int a, int b) { return a + b; };

  {
    const FunctionRef<int(int, int)> ref = add;
    EXPECT_EQ(19, ref(17, 2));
  }

  {
    const auto add_const = add;
    const FunctionRef<int(int, int)> ref = add_const;
    EXPECT_EQ(19, ref(17, 2));
  }
}

TEST(FunctionRef, CapturingLambda) {
  int x = 3;
  const auto lambda = [&x] { return x; };
  FunctionRef<int()> ref = lambda;
  EXPECT_EQ(3, ref());
}

TEST(FunctionRef, FunctionPtr) {
  [](FunctionRef<char(float)> ref) { EXPECT_EQ('a', ref(1.0)); }(&Func);
}

TEST(FunctionRef, Functor) {
  struct S {
    int operator()(int x) const { return x; }
  };
  const S s;
  const FunctionRef<int(int)> ref = s;
  EXPECT_EQ(17, ref(17));
}

TEST(FunctionRef, Method) {
  struct S {
    int Method() const { return value; }

    const int value;
  };
  const S s(25);
  [&s](FunctionRef<int(const S*)> ref) { EXPECT_EQ(25, ref(&s)); }(&S::Method);
}

// Tests for passing a `base::FunctionRef` as an `absl::FunctionRef`.
TEST(FunctionRef, AbslConversion) {
  // Matching signatures should work.
  {
    bool called = false;
    auto lambda = [&called](float) {
      called = true;
      return 'a';
    };
    FunctionRef<char(float)> ref(lambda);
    [](absl::FunctionRef<char(float)> absl_ref) {
      absl_ref(1.0);
    }(ref.ToAbsl());
    EXPECT_TRUE(called);
  }

  // `absl::FunctionRef` should be able to adapt "similar enough" signatures.
  {
    bool called = false;
    auto lambda = [&called](float) {
      called = true;
      return 'a';
    };
    FunctionRef<char(float)> ref(lambda);
    [](absl::FunctionRef<void(float)> absl_ref) {
      absl_ref(1.0);
    }(ref.ToAbsl());
    EXPECT_TRUE(called);
  }
}

// `FunctionRef` allows functors with convertible return types to be adapted.
TEST(FunctionRef, ConvertibleReturnTypes) {
  {
    const auto lambda = [] { return true; };
    const FunctionRef<int()> ref = lambda;
    EXPECT_EQ(1, ref());
  }

  {
    class Base {};
    class Derived : public Base {};

    const auto lambda = []() -> Derived* { return nullptr; };
    const FunctionRef<const Base*()> ref = lambda;
    EXPECT_EQ(nullptr, ref());
  }
}

TEST(FunctionRef, ConstructionFromInexactMatches) {
  // Lambda.
  const auto lambda = [](int32_t x) { return x; };

  // Capturing lambda.
  const auto capturing_lambda = [&](int32_t x) { return lambda(x); };

  // Function pointer.
  int32_t (*const function_ptr)(int32_t) = +lambda;

  // Functor.
  struct Functor {
    int32_t operator()(int32_t x) const { return x; }
  };
  const Functor functor;

  // Method.
  struct Obj {
    int32_t Method(int32_t x) const { return x; }
  };
  int32_t (Obj::*const method)(int32_t) const = &Obj::Method;

  // Each of the objects above should work for a `FunctionRef` with a
  // convertible return type. In this case, they all return `int32_t`, which
  // should be seamlessly convertible to `int64_t` below.
  static_assert(
      std::constructible_from<FunctionRef<int64_t(int32_t)>, decltype(lambda)>);
  static_assert(std::constructible_from<FunctionRef<int64_t(int32_t)>,
                                        decltype(capturing_lambda)>);
  static_assert(std::constructible_from<FunctionRef<int64_t(int32_t)>,
                                        decltype(function_ptr)>);
  static_assert(std::constructible_from<FunctionRef<int64_t(int32_t)>,
                                        decltype(functor)>);
  static_assert(
      std::constructible_from<FunctionRef<int64_t(const Obj*, int32_t)>,
                              decltype(method)>);

  // It shouldn't be possible to construct a `FunctionRef` from any of the
  // objects above if we discard the return value.
  static_assert(
      !std::constructible_from<FunctionRef<void(int32_t)>, decltype(lambda)>);
  static_assert(!std::constructible_from<FunctionRef<void(int32_t)>,
                                         decltype(capturing_lambda)>);
  static_assert(!std::constructible_from<FunctionRef<void(int32_t)>,
                                         decltype(function_ptr)>);
  static_assert(
      !std::constructible_from<FunctionRef<void(int32_t)>, decltype(functor)>);
  static_assert(!std::constructible_from<FunctionRef<void(const Obj*, int32_t)>,
                                         decltype(method)>);

  // It shouldn't be possible to construct a `FunctionRef` from a pointer to a
  // functor, even with a compatible signature.
  static_assert(!std::constructible_from<FunctionRef<int32_t(int32_t)>,
                                         decltype(&functor)>);
}

}  // namespace base
