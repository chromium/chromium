// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/optional_ref.h"

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/test/gtest_util.h"
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

}  // namespace

}  // namespace base
