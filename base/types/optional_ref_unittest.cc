// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/optional_ref.h"

#include <compare>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/test/gtest_util.h"
#include "base/test/memory/dangling_ptr_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Construction from `std::nullptr_t` is disallowed; `std::nullopt` must be
// used to construct an empty `optional_ref`.
static_assert(!std::is_constructible_v<optional_ref<int>, std::nullptr_t>);

// No-compile asserts for various const -> mutable conversions.
static_assert(
    !std::is_constructible_v<optional_ref<int>, const std::optional<int>&>);
static_assert(!std::is_constructible_v<optional_ref<int>, const int*>);
static_assert(!std::is_constructible_v<optional_ref<int>, const int&>);
static_assert(!std::is_constructible_v<optional_ref<int>, int&&>);
static_assert(!std::is_constructible_v<optional_ref<int>, const int>);
static_assert(
    !std::is_constructible_v<optional_ref<int>, optional_ref<const int>>);

// No-compile asserts for implicit conversions.
static_assert(!std::is_constructible_v<optional_ref<bool>, int>);
static_assert(!std::is_constructible_v<optional_ref<bool>, const int&>);
static_assert(!std::is_constructible_v<optional_ref<bool>, int&>);
static_assert(!std::is_constructible_v<optional_ref<bool>, const int*>);
static_assert(!std::is_constructible_v<optional_ref<bool>, int*>);

class ImplicitInt {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  ImplicitInt(int) {}
};

static_assert(!std::is_constructible_v<optional_ref<ImplicitInt>, int>);
static_assert(!std::is_constructible_v<optional_ref<ImplicitInt>, const int&>);
static_assert(!std::is_constructible_v<optional_ref<ImplicitInt>, int&>);
static_assert(!std::is_constructible_v<optional_ref<ImplicitInt>, const int*>);
static_assert(!std::is_constructible_v<optional_ref<ImplicitInt>, int*>);

class TestClass {
 public:
  void ConstMethod() const {}
  void MutableMethod() {}
};

TEST(OptionalRefTest, FromNullopt) {
  [](optional_ref<const int> r) { EXPECT_FALSE(r.has_value()); }(std::nullopt);

  [](optional_ref<int> r) { EXPECT_FALSE(r.has_value()); }(std::nullopt);
}

TEST(OptionalRefTest, FromConstEmptyOptional) {
  const std::optional<int> optional_int;

  [](optional_ref<const int> r) { EXPECT_FALSE(r.has_value()); }(optional_int);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromMutableEmptyOptional) {
  std::optional<int> optional_int;

  [](optional_ref<const int> r) { EXPECT_FALSE(r.has_value()); }(optional_int);

  [](optional_ref<int> r) { EXPECT_FALSE(r.has_value()); }(optional_int);
}

TEST(OptionalRefTest, FromConstOptional) {
  const std::optional<int> optional_int(6);

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(optional_int);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromMutableOptional) {
  std::optional<int> optional_int(6);

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(optional_int);

  [](optional_ref<int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(optional_int);
}

// The From*NullPointer tests intentionally avoid passing nullptr directly,
// which is explicitly disallowed to reduce ambiguity when constructing an empty
// `optional_ref`.
TEST(OptionalRefTest, FromConstNullPointer) {
  const int* ptr = nullptr;

  [](optional_ref<const int> r) { EXPECT_FALSE(r.has_value()); }(ptr);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromMutableNullPointer) {
  int* ptr = nullptr;

  [](optional_ref<const int> r) { EXPECT_FALSE(r.has_value()); }(ptr);

  [](optional_ref<int> r) { EXPECT_FALSE(r.has_value()); }(ptr);
}

TEST(OptionalRefTest, FromConstPointer) {
  int value = 6;
  const int* ptr = &value;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ptr);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromPointer) {
  int value = 6;
  int* ptr = &value;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ptr);

  [](optional_ref<int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ptr);
}

TEST(OptionalRefTest, FromConstRef) {
  int value = 6;
  const int& ref = value;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ref);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromMutableRef) {
  int value = 6;
  int& ref = value;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ref);

  [](optional_ref<int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(ref);
}

TEST(OptionalRefTest, FromConstValue) {
  const int value = 6;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(value);

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(6);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromMutableValue) {
  int value = 6;

  [](optional_ref<const int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(value);

  [](optional_ref<int> r) {
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(6, r.value());
  }(value);
}

TEST(OptionalRefTest, FromMutableEmptyOptionalRefTest) {
  {
    optional_ref<int> r1;
    [](optional_ref<const int> r2) { EXPECT_FALSE(r2.has_value()); }(r1);
  }

  {
    optional_ref<int> r1(std::nullopt);
    [](optional_ref<int> r2) { EXPECT_FALSE(r2.has_value()); }(r1);
  }
}

TEST(OptionalRefTest, FromMutableOptionalRefTest) {
  int value = 6;
  optional_ref<int> r1(value);

  [](optional_ref<const int> r2) {
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(6, r2.value());
  }(r1);

  [](optional_ref<int> r2) {
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(6, r2.value());
  }(r1);
}

TEST(OptionalRefTest, FromCopyConstructorConst) {
  [](optional_ref<const int> r) {
    EXPECT_FALSE(r.has_value());
  }(optional_ref<const int>());

  int value = 6;
  optional_ref<const int> r1(value);
  [](optional_ref<const int> r2) {
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(6, r2.value());
  }(r1);

  // Mutable case covered by static_assert test above.
}

TEST(OptionalRefTest, FromCopyConstructorMutable) {
  [](optional_ref<int> r) { EXPECT_FALSE(r.has_value()); }(optional_ref<int>());

  int value = 6;
  optional_ref<int> r1(value);
  [](optional_ref<int> r2) {
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(6, r2.value());
  }(r1);
}

TEST(OptionalRefTest, Arrow) {
  int uninitialized_value;

  {
    const optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator->()), const int*>);
    EXPECT_EQ(&uninitialized_value, r.operator->());
  }

  {
    optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator->()), const int*>);
    EXPECT_EQ(&uninitialized_value, r.operator->());
  }

  {
    const optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator->()), int*>);
    EXPECT_EQ(&uninitialized_value, r.operator->());
  }

  {
    optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator->()), int*>);
    EXPECT_EQ(&uninitialized_value, r.operator->());
  }
}

TEST(OptionalRefTest, Star) {
  int uninitialized_value;

  {
    const optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator*()), const int&>);
    EXPECT_EQ(&uninitialized_value, &r.operator*());
  }

  {
    optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator*()), const int&>);
    EXPECT_EQ(&uninitialized_value, &r.operator*());
  }

  {
    const optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator*()), int&>);
    EXPECT_EQ(&uninitialized_value, &r.operator*());
  }

  {
    optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.operator*()), int&>);
    EXPECT_EQ(&uninitialized_value, &r.operator*());
  }
}

TEST(OptionalRefTest, BoolConversion) {
  {
    optional_ref<int> r;
    EXPECT_FALSE(r);
  }
  {
    int i;
    base::optional_ref<int> r = i;
    EXPECT_TRUE(r);
  }
}

TEST(OptionalRefTest, Value) {
  // has_value() and value() are generally covered by the construction tests.
  // Make sure value() doesn't somehow break const-ness here.
  {
    const optional_ref<const int> r;
    static_assert(std::is_same_v<decltype(r.value()), const int&>);
  }

  {
    optional_ref<const int> r;
    static_assert(std::is_same_v<decltype(r.value()), const int&>);
  }

  {
    const optional_ref<int> r;
    static_assert(std::is_same_v<decltype(r.value()), int&>);
  }

  {
    optional_ref<int> r;
    static_assert(std::is_same_v<decltype(r.value()), int&>);
  }
}

TEST(OptionalRefTest, AsPtr) {
  optional_ref<int> r1;
  EXPECT_EQ(nullptr, r1.as_ptr());

  int uninitialized_value;
  {
    const optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.as_ptr()), const int*>);
    EXPECT_EQ(&uninitialized_value, r.as_ptr());
  }

  {
    optional_ref<const int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.as_ptr()), const int*>);
    EXPECT_EQ(&uninitialized_value, r.as_ptr());
  }

  {
    const optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.as_ptr()), int*>);
    EXPECT_EQ(&uninitialized_value, r.as_ptr());
  }

  {
    optional_ref<int> r(uninitialized_value);
    static_assert(std::is_same_v<decltype(r.as_ptr()), int*>);
    EXPECT_EQ(&uninitialized_value, r.as_ptr());
  }
}

TEST(OptionalRefTest, CopyAsOptional) {
  optional_ref<int> r1;
  std::optional<int> o1 = r1.CopyAsOptional();
  EXPECT_EQ(std::nullopt, o1);

  int value = 6;
  optional_ref<int> r2(value);
  std::optional<int> o2 = r2.CopyAsOptional();
  EXPECT_EQ(6, o2);
}

TEST(OptionalRefTest, EqualityComparisonWithNullOpt) {
  {
    optional_ref<int> r;
    EXPECT_EQ(r, std::nullopt);
    EXPECT_EQ(std::nullopt, r);
  }

  {
    int value = 5;
    optional_ref<int> r(value);
    EXPECT_NE(r, std::nullopt);
    EXPECT_NE(std::nullopt, r);
  }
}

class Comparable {
 public:
  explicit Comparable(bool b, int i) : b_(b), i_(i) {}

  Comparable(const Comparable&) = delete;
  Comparable(Comparable&&) = delete;
  Comparable& operator=(const Comparable&) = delete;
  Comparable& operator=(Comparable&&) = delete;

  friend auto operator<=>(const Comparable&, const Comparable&) = default;

 private:
  bool b_;
  int i_;
};

static_assert(!std::is_copy_assignable<Comparable>());
static_assert(!std::is_copy_constructible<Comparable>());
static_assert(std::equality_comparable<optional_ref<Comparable>>);
static_assert(std::equality_comparable<optional_ref<const Comparable>>);
static_assert(std::three_way_comparable<optional_ref<Comparable>>);
static_assert(std::three_way_comparable<optional_ref<const Comparable>>);

TEST(OptionalRefTest, EqualityComparison) {
  int value = 5;

  {
    // Nulls.
    optional_ref<int> r;
    optional_ref<int> s;
    EXPECT_EQ(r, s);
    EXPECT_EQ(s, r);
    EXPECT_EQ(r, r);
    EXPECT_EQ(s, s);

    std::optional<int> opt = 5;

    EXPECT_NE(r, value);
    EXPECT_NE(r, opt);
    EXPECT_NE(value, r);
    EXPECT_NE(opt, r);
  }

  {
    // Populated values.
    int other_value = 5;
    std::optional<int> opt = 7;

    optional_ref<int> r(value);
    optional_ref<int> s(other_value);
    EXPECT_EQ(r, s);
    EXPECT_EQ(s, r);
    EXPECT_EQ(r, r);
    EXPECT_EQ(s, s);

    EXPECT_EQ(s, 5);
    EXPECT_EQ(s, value);

    EXPECT_NE(s, 6);
    EXPECT_NE(s, opt);
  }

  {
    // Mismatched const-qualification.
    optional_ref<int> r(value);
    optional_ref<const int> s(value);
    EXPECT_EQ(r, s);
    EXPECT_EQ(s, r);
    EXPECT_EQ(r, r);
    EXPECT_EQ(s, s);
  }

  {
    // Use with references.
    Comparable comp(true, 5);
    optional_ref<Comparable> r(comp);
    optional_ref<const Comparable> s(comp);

    EXPECT_EQ(comp, r);
    EXPECT_EQ(r, comp);
    EXPECT_EQ(comp, s);
    EXPECT_EQ(s, comp);

    EXPECT_EQ(r, s);
    EXPECT_EQ(s, r);
    EXPECT_EQ(r, r);
    EXPECT_EQ(s, s);

    EXPECT_NE(Comparable(false, 5), r);
    EXPECT_NE(r, Comparable(false, 5));
    EXPECT_NE(Comparable(false, 5), s);
    EXPECT_NE(s, Comparable(false, 5));
  }
}

TEST(OptionalRefTest, ThreeWayComparison) {
  int value = 5;
  int other_value = 7;

  {
    // Nulls.
    optional_ref<int> r;
    optional_ref<int> s;
    EXPECT_EQ(r <=> s, std::strong_ordering::equal);

    optional_ref<int> t(value);
    EXPECT_LT(r, t);
    EXPECT_GT(t, r);
  }

  {
    // Populated values.
    optional_ref<int> r(value);
    optional_ref<int> s(other_value);
    EXPECT_LT(r, s);
    EXPECT_GT(s, r);
  }

  {
    // Mismatched const-qualification.
    optional_ref<int> r(value);
    optional_ref<const int> s(other_value);
    EXPECT_LT(r, s);
  }
}

class Noncomparable {};

static_assert(!std::equality_comparable<Noncomparable>);
static_assert(!std::equality_comparable<optional_ref<Noncomparable>>);
static_assert(!std::three_way_comparable<Noncomparable>);
static_assert(!std::three_way_comparable<optional_ref<Noncomparable>>);

class PartiallyComparable {
 public:
  friend std::partial_ordering operator<=>(const PartiallyComparable&,
                                           const PartiallyComparable&) =
      default;
};

static_assert(std::three_way_comparable<PartiallyComparable>);
static_assert(std::three_way_comparable<optional_ref<PartiallyComparable>>);

TEST(OptionalRefTest, CompatibilityWithOptionalMatcher) {
  using ::testing::Optional;

  int x = 45;
  optional_ref<int> r(x);
  EXPECT_THAT(r, Optional(x));
  EXPECT_THAT(r, Optional(45));
  EXPECT_THAT(r, ::testing::Not(Optional(46)));
}

TEST(OptionalRefDeathTest, ArrowOnEmpty) {
  [](optional_ref<const TestClass> r) {
    EXPECT_CHECK_DEATH(r->ConstMethod());
  }(std::nullopt);

  [](optional_ref<TestClass> r) {
    EXPECT_CHECK_DEATH(r->ConstMethod());
    EXPECT_CHECK_DEATH(r->MutableMethod());
  }(std::nullopt);
}

TEST(OptionalRefDeathTest, StarOnEmpty) {
  [](optional_ref<const TestClass> r) {
    EXPECT_CHECK_DEATH((*r).ConstMethod());
  }(std::nullopt);

  [](optional_ref<TestClass> r) {
    EXPECT_CHECK_DEATH((*r).ConstMethod());
    EXPECT_CHECK_DEATH((*r).MutableMethod());
  }(std::nullopt);
}

TEST(OptionalRefDeathTest, ValueOnEmpty) {
  [](optional_ref<const TestClass> r) {
    EXPECT_CHECK_DEATH(r.value());
  }(std::nullopt);

  [](optional_ref<TestClass> r) {
    EXPECT_CHECK_DEATH(r.value());
  }(std::nullopt);
}

TEST(OptionalRefTest, ClassTemplateArgumentDeduction) {
  static_assert(
      std::is_same_v<decltype(optional_ref{int()}), optional_ref<const int>>);

  {
    const int i = 0;
    static_assert(
        std::is_same_v<decltype(optional_ref(i)), optional_ref<const int>>);
  }

  {
    int i = 0;
    static_assert(std::is_same_v<decltype(optional_ref(i)), optional_ref<int>>);
  }

  static_assert(std::is_same_v<decltype(optional_ref(std::optional<int>())),
                               optional_ref<const int>>);

  {
    const std::optional<int> o;
    static_assert(
        std::is_same_v<decltype(optional_ref(o)), optional_ref<const int>>);
  }

  {
    std::optional<int> o;
    static_assert(std::is_same_v<decltype(optional_ref(o)), optional_ref<int>>);
  }

  {
    const int* p = nullptr;
    static_assert(
        std::is_same_v<decltype(optional_ref(p)), optional_ref<const int>>);
  }

  {
    int* p = nullptr;
    static_assert(std::is_same_v<decltype(optional_ref(p)), optional_ref<int>>);
  }
}

// TODO(dcheng): It's not yet clear if it's desirable to have optional_ref embed
// a raw_ptr. While it is certainly nice from a certain perspective, there is
// also separate guidance to use native C++ pointers when passing things as
// arguments, and `base::optional_ref` is intended to be an argument type.
TEST(OptionalRefTest, DanglingPointerDetector) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }
  {
    auto owned = std::make_unique<int>();
    optional_ref<int> ref = *owned;
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);

    owned.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 1u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 1u);
}

TEST(OptionalRefTest, DanglingUntriaged) {
  auto instrumentation = test::DanglingPtrInstrumentation::Create();
  if (!instrumentation.has_value()) {
    GTEST_SKIP() << instrumentation.error();
  }
  {
    auto owned = std::make_unique<int>();
    optional_ref<int, DanglingUntriaged> ref = *owned;
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);

    owned.reset();
    EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
    EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
  }
  EXPECT_EQ(instrumentation->dangling_ptr_detected(), 0u);
  EXPECT_EQ(instrumentation->dangling_ptr_released(), 0u);
}

}  // namespace

}  // namespace base
