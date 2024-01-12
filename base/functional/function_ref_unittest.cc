// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/function_ref.h"

#include <stdint.h>

#include <concepts>
#include <optional>

#include "base/compiler_specific.h"
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
  const FunctionRef<char(float)> ref = +[](float) { return 'a'; };
  EXPECT_EQ('a', ref(1.0f));
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

// If we construct from another `FunctionRef`, that should work fine, even if
// the input is destroyed before we call the output. In other words, we should
// reference the underlying callable, not the `FunctionRef`.
//
// We construct in a `noinline` function to maximize the chance that ASAN
// notices the use-after-free if we get this wrong.
NOINLINE void ConstructFromLValue(std::optional<FunctionRef<int()>>& ref) {
  const auto return_17 = [] { return 17; };
  FunctionRef<int()> other = return_17;
  ref.emplace(other);
}
NOINLINE void ConstructFromConstLValue(std::optional<FunctionRef<int()>>& ref) {
  const auto return_17 = [] { return 17; };
  const FunctionRef<int()> other = return_17;
  ref.emplace(other);
}
NOINLINE void ConstructFromRValue(std::optional<FunctionRef<int()>>& ref) {
  const auto return_17 = [] { return 17; };
  using Ref = FunctionRef<int()>;
  ref.emplace(Ref(return_17));
}
NOINLINE void ConstructFromConstRValue(std::optional<FunctionRef<int()>>& ref) {
  const auto return_17 = [] { return 17; };
  using Ref = const FunctionRef<int()>;
  ref.emplace(Ref(return_17));
}
TEST(FunctionRef, ConstructionFromOtherFunctionRefObjects) {
  using Ref = FunctionRef<int()>;
  std::optional<Ref> ref;

  ConstructFromLValue(ref);
  EXPECT_EQ(17, (*ref)());

  ConstructFromConstLValue(ref);
  EXPECT_EQ(17, (*ref)());

  ConstructFromRValue(ref);
  EXPECT_EQ(17, (*ref)());

  ConstructFromConstRValue(ref);
  EXPECT_EQ(17, (*ref)());

  // It shouldn't be possible to construct from `FunctionRef` objects with
  // differing signatures, even if they are compatible with `int()`.
  static_assert(!std::constructible_from<Ref, FunctionRef<void()>>);
  static_assert(!std::constructible_from<Ref, FunctionRef<int(int)>>);
  static_assert(!std::constructible_from<Ref, FunctionRef<int64_t()>>);

  // Check again with various qualifiers.
  static_assert(!std::constructible_from<Ref, const FunctionRef<void()>>);
  static_assert(!std::constructible_from<Ref, FunctionRef<void()>&>);
  static_assert(!std::constructible_from<Ref, FunctionRef<void()>&&>);
  static_assert(!std::constructible_from<Ref, const FunctionRef<void()>&>);
  static_assert(!std::constructible_from<Ref, const FunctionRef<void()>&&>);
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

TEST(FunctionRef, ConstructionFromAbslFunctionRef) {
  // It shouldn't be possible to construct a `FunctionRef` from an
  // `absl::FunctionRef`, whether the signatures are compatible or not.
  using Ref = FunctionRef<int(int)>;
  static_assert(!std::is_constructible_v<Ref, absl::FunctionRef<void()>>);
  static_assert(!std::is_constructible_v<Ref, absl::FunctionRef<void(int)>>);
  static_assert(!std::is_constructible_v<Ref, absl::FunctionRef<int(int)>>);

  // Check again with various qualifiers.
  using AbslRef = absl::FunctionRef<int(int)>;
  static_assert(!std::is_constructible_v<Ref, const AbslRef>);
  static_assert(!std::is_constructible_v<Ref, AbslRef&>);
  static_assert(!std::is_constructible_v<Ref, AbslRef&&>);
  static_assert(!std::is_constructible_v<Ref, const AbslRef&>);
  static_assert(!std::is_constructible_v<Ref, const AbslRef&&>);
}

}  // namespace base
