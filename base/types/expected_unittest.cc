// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/expected.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/to_string.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/types/strong_alias.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Additional restrictions on implicit conversions. Not present in the C++23
// proposal.
static_assert(!std::is_convertible_v<int, expected<int, int>>);
static_assert(!std::is_convertible_v<long, expected<bool, long>>);

template <typename T>
struct Strong {
  constexpr explicit Strong(T value) : value(std::move(value)) {}
  T value;
};

template <typename T>
struct Weak {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Weak(T value) : value(std::move(value)) {}
  T value;
};

template <typename T>
struct StrongMoveOnly {
  constexpr explicit StrongMoveOnly(T&& value) : value(std::move(value)) {}
  constexpr StrongMoveOnly(StrongMoveOnly&& other)
      : value(std::exchange(other.value, {})) {}

  constexpr StrongMoveOnly& operator=(StrongMoveOnly&& other) {
    value = std::exchange(other.value, {});
    return *this;
  }

  T value;
};

template <typename T>
struct WeakMoveOnly {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr WeakMoveOnly(T&& value) : value(std::move(value)) {}
  constexpr WeakMoveOnly(WeakMoveOnly&& other)
      : value(std::exchange(other.value, {})) {}
  T value;
};

enum class Error {
  kFail,
};

enum class CvRef {
  kNone,
  kRef,
  kConstRef,
  kRRef,
  kConstRRef,
};

struct SaveCvRef {
  constexpr SaveCvRef() = default;
  constexpr SaveCvRef(SaveCvRef&) : cvref(CvRef::kRef) {}
  constexpr SaveCvRef(const SaveCvRef&) : cvref(CvRef::kConstRef) {}
  constexpr SaveCvRef(SaveCvRef&&) : cvref(CvRef::kRRef) {}
  constexpr SaveCvRef(const SaveCvRef&&) : cvref(CvRef::kConstRRef) {}

  constexpr explicit SaveCvRef(CvRef cvref) : cvref(cvref) {}

  CvRef cvref = CvRef::kNone;
};

TEST(Ok, ValueConstructor) {
  constexpr ok<int> o(42);
  static_assert(o.value() == 42);
}

TEST(Ok, DefaultConstructor) {
  constexpr ok<int> o(std::in_place);
  static_assert(o.value() == 0);
}

TEST(Ok, InPlaceConstructor) {
  constexpr ok<std::pair<int, double>> o(std::in_place, 42, 3.14);
  static_assert(o.value() == std::pair(42, 3.14));
}

TEST(Ok, InPlaceListConstructor) {
  ok<std::vector<int>> o(std::in_place, {1, 2, 3});
  EXPECT_EQ(o.value(), std::vector({1, 2, 3}));
}

TEST(Ok, ValueIsQualified) {
  using Ok = ok<int>;
  static_assert(std::is_same_v<decltype(std::declval<Ok&>().value()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ok&>().value()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ok>().value()), int&&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ok>().value()), const int&&>);
}

TEST(Ok, MemberSwap) {
  ok o1(42);
  ok o2(123);
  o1.swap(o2);

  EXPECT_EQ(o1.value(), 123);
  EXPECT_EQ(o2.value(), 42);
}

TEST(Ok, EqualityOperators) {
  static_assert(ok(42) == ok(42.0));
  static_assert(ok(42) != ok(43));
}

TEST(Ok, FreeSwap) {
  ok o1(42);
  ok o2(123);
  swap(o1, o2);

  EXPECT_EQ(o1.value(), 123);
  EXPECT_EQ(o2.value(), 42);
}

TEST(Unexpected, ValueConstructor) {
  constexpr unexpected<int> unex(42);
  static_assert(unex.error() == 42);
}

TEST(Unexpected, DefaultConstructor) {
  constexpr unexpected<int> unex(std::in_place);
  static_assert(unex.error() == 0);
}

TEST(Unexpected, InPlaceConstructor) {
  constexpr unexpected<std::pair<int, double>> unex(std::in_place, 42, 3.14);
  static_assert(unex.error() == std::pair(42, 3.14));
}

TEST(Unexpected, InPlaceListConstructor) {
  unexpected<std::vector<int>> unex(std::in_place, {1, 2, 3});
  EXPECT_EQ(unex.error(), std::vector({1, 2, 3}));
}

TEST(Unexpected, ErrorIsQualified) {
  using Unex = unexpected<int>;
  static_assert(std::is_same_v<decltype(std::declval<Unex&>().error()), int&>);
  static_assert(std::is_same_v<decltype(std::declval<const Unex&>().error()),
                               const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Unex>().error()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Unex>().error()),
                               const int&&>);
}

TEST(Unexpected, MemberSwap) {
  unexpected u1(42);
  unexpected u2(123);
  u1.swap(u2);

  EXPECT_EQ(u1.error(), 123);
  EXPECT_EQ(u2.error(), 42);
}

TEST(Unexpected, EqualityOperators) {
  static_assert(unexpected(42) == unexpected(42.0));
  static_assert(unexpected(42) != unexpected(43));
}

TEST(Unexpected, FreeSwap) {
  unexpected u1(42);
  unexpected u2(123);
  swap(u1, u2);

  EXPECT_EQ(u1.error(), 123);
  EXPECT_EQ(u2.error(), 42);
}

TEST(Expected, Triviality) {
  using TrivialExpected = expected<int, Error>;
  static_assert(std::is_trivially_destructible_v<TrivialExpected>);

  using NonTrivialExpected = expected<int, std::string>;
  static_assert(!std::is_trivially_destructible_v<NonTrivialExpected>);
}

TEST(Expected, DefaultConstructor) {
  constexpr expected<int, Error> ex;
  static_assert(ex.has_value());
  EXPECT_EQ(ex.value(), 0);

  static_assert(std::is_default_constructible_v<expected<int, Error>>);
  static_assert(!std::is_default_constructible_v<expected<Strong<int>, Error>>);
}

TEST(Expected, CopyConstructor) {
  {
    constexpr expected<int, Error> ex1 = 42;
    constexpr expected<int, Error> ex2 = ex1;
    static_assert(ex2.has_value());
    // Note: In theory this could be constexpr, but is currently not due to
    // implementation details of absl::get [1].
    // TODO: Make this a static_assert once this is fixed in Abseil, or we use
    // std::variant. Similarly in the tests below.
    // [1] https://github.com/abseil/abseil-cpp/blob/50739/absl/types/internal/variant.h#L548
    EXPECT_EQ(ex2.value(), 42);
  }

  {
    constexpr expected<int, Error> ex1 = unexpected(Error::kFail);
    constexpr expected<int, Error> ex2 = ex1;
    static_assert(!ex2.has_value());
    EXPECT_EQ(ex2.error(), Error::kFail);
  }
}

TEST(Expected, MoveConstructor) {
  {
    expected<StrongMoveOnly<int>, int> ex1 = StrongMoveOnly(42);
    expected<StrongMoveOnly<int>, int> ex2 = std::move(ex1);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, StrongMoveOnly<int>> ex1 = unexpected(StrongMoveOnly(42));
    expected<int, StrongMoveOnly<int>> ex2 = std::move(ex1);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ExplicitConvertingCopyConstructor) {
  {
    expected<int, Error> ex1 = 42;
    expected<Strong<int>, Error> ex2(ex1);
    static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, Error> ex1 = unexpected(Error::kFail);
    expected<int, Strong<Error>> ex2(ex1);
    static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, Error::kFail);
  }
}

TEST(Expected, ImplicitConvertingCopyConstructor) {
  {
    expected<int, Error> ex1 = 42;
    expected<Weak<int>, Weak<Error>> ex2 = ex1;
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }
  {
    expected<int, Error> ex1 = unexpected(Error::kFail);
    expected<Weak<int>, Weak<Error>> ex2 = ex1;
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, Error::kFail);
  }
}

TEST(Expected, ExplicitConvertingMoveConstructor) {
  {
    expected<int, Error> ex1 = 42;
    expected<StrongMoveOnly<int>, Error> ex2(std::move(ex1));
    static_assert(
        !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, Error> ex1 = unexpected(Error::kFail);
    expected<int, StrongMoveOnly<Error>> ex2(std::move(ex1));
    static_assert(
        !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, Error::kFail);
  }
}

TEST(Expected, ImplicitConvertingMoveConstructor) {
  {
    expected<int, Error> ex1 = 42;
    expected<WeakMoveOnly<int>, Error> ex2 = std::move(ex1);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, Error> ex1 = unexpected(Error::kFail);
    expected<int, WeakMoveOnly<Error>> ex2 = std::move(ex1);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, Error::kFail);
  }
}

TEST(Expected, ExplicitValueConstructor) {
  {
    constexpr expected<Strong<int>, int> ex(42);
    static_assert(!std::is_convertible_v<int, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<StrongMoveOnly<int>, int> ex(42);
    static_assert(!std::is_constructible_v<decltype(ex), int&>);
    static_assert(!std::is_convertible_v<int, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ImplicitValueConstructor) {
  {
    constexpr expected<Weak<int>, Error> ex = 42;
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<WeakMoveOnly<int>, Error> ex = 42;
    static_assert(!std::is_convertible_v<int&, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ExplicitOkConstructor) {
  {
    constexpr expected<Strong<int>, int> ex(ok(42));
    static_assert(!std::is_convertible_v<ok<int>, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<StrongMoveOnly<int>, int> ex(ok(42));
    static_assert(!std::is_constructible_v<decltype(ex), ok<int>&>);
    static_assert(!std::is_convertible_v<ok<int>, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ImplicitOkConstructor) {
  {
    constexpr expected<Weak<int>, Error> ex = ok(42);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<WeakMoveOnly<int>, Error> ex = ok(42);
    static_assert(!std::is_convertible_v<ok<int>&, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ExplicitErrorConstructor) {
  {
    constexpr expected<int, Strong<int>> ex(unexpected(42));
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<int, StrongMoveOnly<int>> ex(unexpected(42));
    static_assert(!std::is_constructible_v<decltype(ex), unexpected<int>&>);
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(Expected, ImplicitErrorConstructor) {
  {
    constexpr expected<int, Weak<int>> ex = unexpected(42);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<int, WeakMoveOnly<int>> ex = unexpected(42);
    static_assert(!std::is_convertible_v<unexpected<int>&, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(Expected, InPlaceConstructor) {
  constexpr expected<Strong<int>, int> ex(std::in_place, 42);
  static_assert(ex.has_value());
  EXPECT_EQ(ex.value().value, 42);
}

TEST(Expected, InPlaceListConstructor) {
  expected<std::vector<int>, int> ex(std::in_place, {1, 2, 3});
  EXPECT_THAT(ex, test::ValueIs(std::vector({1, 2, 3})));
}

TEST(Expected, UnexpectConstructor) {
  constexpr expected<int, Strong<int>> ex(unexpect, 42);
  static_assert(!ex.has_value());
  EXPECT_EQ(ex.error().value, 42);
}

TEST(Expected, UnexpectListConstructor) {
  expected<int, std::vector<int>> ex(unexpect, {1, 2, 3});
  EXPECT_THAT(ex, test::ErrorIs(std::vector({1, 2, 3})));
}

TEST(Expected, AssignValue) {
  expected<int, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex = 42;
  EXPECT_THAT(ex, test::ValueIs(42));

  ex = 123;
  EXPECT_THAT(ex, test::ValueIs(123));
}

TEST(Expected, CopyAssignOk) {
  expected<int, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex = ok(42);
  EXPECT_THAT(ex, test::ValueIs(42));

  ex = ok(123);
  EXPECT_THAT(ex, test::ValueIs(123));
}

TEST(Expected, MoveAssignOk) {
  expected<StrongMoveOnly<int>, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex = ok(StrongMoveOnly(42));
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value().value, 42);

  ex = ok(StrongMoveOnly(123));
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value().value, 123);
}

TEST(Expected, CopyAssignUnexpected) {
  expected<int, int> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(42);
  EXPECT_THAT(ex, test::ErrorIs(42));

  ex = unexpected(123);
  EXPECT_THAT(ex, test::ErrorIs(123));
}

TEST(Expected, MoveAssignUnexpected) {
  expected<int, StrongMoveOnly<int>> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(StrongMoveOnly(42));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 42);

  ex = unexpected(StrongMoveOnly(123));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 123);
}

TEST(Expected, Emplace) {
  expected<StrongMoveOnly<int>, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace(42);
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value().value, 42);
}

TEST(Expected, EmplaceList) {
  expected<std::vector<int>, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace({1, 2, 3});
  EXPECT_THAT(ex, test::ValueIs(std::vector({1, 2, 3})));
}

TEST(Expected, MemberSwap) {
  expected<int, int> ex1(42);
  expected<int, int> ex2 = unexpected(123);

  ex1.swap(ex2);
  EXPECT_THAT(ex1, test::ErrorIs(123));
  EXPECT_THAT(ex2, test::ValueIs(42));
}

TEST(Expected, FreeSwap) {
  expected<int, int> ex1(42);
  expected<int, int> ex2 = unexpected(123);

  swap(ex1, ex2);
  EXPECT_THAT(ex1, test::ErrorIs(123));
  EXPECT_THAT(ex2, test::ValueIs(42));
}

TEST(Expected, OperatorArrow) {
  expected<Strong<int>, int> ex(0);
  EXPECT_EQ(ex->value, 0);

  ex->value = 1;
  EXPECT_EQ(ex->value, 1);

  constexpr expected<Strong<int>, int> c_ex(0);
  EXPECT_EQ(c_ex->value, 0);
  static_assert(std::is_same_v<decltype((c_ex->value)), const int&>);
}

TEST(Expected, OperatorStar) {
  expected<int, int> ex;
  EXPECT_EQ(*ex, 0);

  *ex = 1;
  EXPECT_EQ(*ex, 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(*std::declval<Ex&>()), int&>);
  static_assert(
      std::is_same_v<decltype(*std::declval<const Ex&>()), const int&>);
  static_assert(std::is_same_v<decltype(*std::declval<Ex&&>()), int&&>);
  static_assert(
      std::is_same_v<decltype(*std::declval<const Ex&&>()), const int&&>);
}

TEST(Expected, HasValue) {
  constexpr expected<int, int> ex;
  static_assert(ex.has_value());

  constexpr expected<int, int> unex = unexpected(0);
  static_assert(!unex.has_value());
}

TEST(Expected, Value) {
  expected<int, int> ex;
  EXPECT_EQ(ex.value(), 0);

  ex.value() = 1;
  EXPECT_EQ(ex.value(), 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().value()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().value()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().value()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().value()),
                               const int&&>);
}

TEST(Expected, Error) {
  expected<int, int> ex = unexpected(0);
  EXPECT_EQ(ex.error(), 0);

  ex.error() = 1;
  EXPECT_EQ(ex.error(), 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().error()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().error()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().error()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().error()),
                               const int&&>);
}

TEST(Expected, ToString) {
  // `expected` should have a custom string representation that prints the
  // contained value/error.
  const std::string value_str = ToString(expected<int, int>(123456));
  EXPECT_FALSE(base::Contains(value_str, "-byte object at "));
  EXPECT_TRUE(base::Contains(value_str, "123456"));
  const std::string error_str =
      ToString(expected<int, int>(unexpected(123456)));
  EXPECT_FALSE(base::Contains(error_str, "-byte object at "));
  EXPECT_TRUE(base::Contains(error_str, "123456"));
}

TEST(Expected, ValueOr) {
  {
    expected<int, int> ex;
    EXPECT_EQ(ex.value_or(123), 0);

    expected<int, int> unex = unexpected(0);
    EXPECT_EQ(unex.value_or(123), 123);
  }

  {
    expected<WeakMoveOnly<int>, int> ex(0);
    EXPECT_EQ(std::move(ex).value_or(123).value, 0);

    expected<WeakMoveOnly<int>, int> unex = unexpected(0);
    EXPECT_EQ(std::move(unex).value_or(123).value, 123);
  }
}

TEST(Expected, ErrorOr) {
  {
    expected<int, int> ex;
    EXPECT_EQ(ex.error_or(123), 123);

    expected<int, int> unex = unexpected(0);
    EXPECT_EQ(unex.error_or(123), 0);
  }

  {
    expected<int, WeakMoveOnly<int>> ex(0);
    EXPECT_EQ(std::move(ex).error_or(123).value, 123);

    expected<int, WeakMoveOnly<int>> unex = unexpected(0);
    EXPECT_EQ(std::move(unex).error_or(123).value, 0);
  }
}

TEST(Expected, AndThen) {
  using ExIn = expected<SaveCvRef, SaveCvRef>;
  using ExOut = expected<CvRef, SaveCvRef>;

  auto get_ex_cvref = [](auto&& x) -> ExOut {
    return SaveCvRef(std::forward<decltype(x)>(x)).cvref;
  };

  ExIn ex;
  EXPECT_EQ(ex.and_then(get_ex_cvref), CvRef::kRef);
  EXPECT_EQ(std::as_const(ex).and_then(get_ex_cvref), CvRef::kConstRef);
  EXPECT_EQ(std::move(ex).and_then(get_ex_cvref), CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(ex)).and_then(get_ex_cvref),
            CvRef::kConstRRef);

  ExIn unex(unexpect);
  EXPECT_EQ(unex.and_then(get_ex_cvref).error().cvref, CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).and_then(get_ex_cvref).error().cvref,
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).and_then(get_ex_cvref).error().cvref, CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).and_then(get_ex_cvref).error().cvref,
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().and_then(get_ex_cvref)),
                     ExOut>);
  static_assert(
      std::is_same_v<
          decltype(std::declval<const ExIn&>().and_then(get_ex_cvref)), ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&&>().and_then(get_ex_cvref)),
                     ExOut>);
  static_assert(std::is_same_v<decltype(std::declval<const ExIn&&>().and_then(
                                   get_ex_cvref)),
                               ExOut>);
}

TEST(Expected, OrElse) {
  using ExIn = expected<SaveCvRef, SaveCvRef>;
  using ExOut = expected<SaveCvRef, CvRef>;

  auto get_unex_cvref = [](auto&& x) -> ExOut {
    return unexpected(SaveCvRef(std::forward<decltype(x)>(x)).cvref);
  };

  ExIn ex;
  EXPECT_EQ(ex.or_else(get_unex_cvref).value().cvref, CvRef::kRef);
  EXPECT_EQ(std::as_const(ex).or_else(get_unex_cvref).value().cvref,
            CvRef::kConstRef);
  EXPECT_EQ(std::move(ex).or_else(get_unex_cvref).value().cvref, CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(ex)).or_else(get_unex_cvref).value().cvref,
            CvRef::kConstRRef);

  ExIn unex(unexpect);
  EXPECT_EQ(unex.or_else(get_unex_cvref).error(), CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).or_else(get_unex_cvref).error(),
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).or_else(get_unex_cvref).error(), CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).or_else(get_unex_cvref).error(),
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().or_else(get_unex_cvref)),
                     ExOut>);
  static_assert(std::is_same_v<decltype(std::declval<const ExIn&>().or_else(
                                   get_unex_cvref)),
                               ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&&>().or_else(get_unex_cvref)),
                     ExOut>);
  static_assert(std::is_same_v<decltype(std::declval<const ExIn&&>().or_else(
                                   get_unex_cvref)),
                               ExOut>);
}

TEST(Expected, Transform) {
  using ExIn = expected<SaveCvRef, SaveCvRef>;
  using ExOut = expected<CvRef, SaveCvRef>;

  auto get_cvref = [](auto&& x) {
    return SaveCvRef(std::forward<decltype(x)>(x)).cvref;
  };

  {
    ExIn ex;
    EXPECT_EQ(ex.transform(get_cvref), CvRef::kRef);
    EXPECT_EQ(std::as_const(ex).transform(get_cvref), CvRef::kConstRef);
    EXPECT_EQ(std::move(ex).transform(get_cvref), CvRef::kRRef);
    EXPECT_EQ(std::move(std::as_const(ex)).transform(get_cvref),
              CvRef::kConstRRef);

    ExIn unex(unexpect);
    EXPECT_EQ(unex.transform(get_cvref).error().cvref, CvRef::kRef);
    EXPECT_EQ(std::as_const(unex).transform(get_cvref).error().cvref,
              CvRef::kConstRef);
    EXPECT_EQ(std::move(unex).transform(get_cvref).error().cvref, CvRef::kRRef);
    EXPECT_EQ(std::move(std::as_const(unex)).transform(get_cvref).error().cvref,
              CvRef::kConstRRef);

    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&>().transform(get_cvref)),
                       ExOut>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ExIn&>().transform(get_cvref)), ExOut>);
    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&&>().transform(get_cvref)),
                       ExOut>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const ExIn&&>().transform(get_cvref)),
                  ExOut>);
  }

  // Test void transform.
  {
    using ExOutVoid = expected<void, SaveCvRef>;
    CvRef cvref = CvRef::kNone;
    auto write_cvref = [&cvref](auto&& x) {
      cvref = SaveCvRef(std::forward<decltype(x)>(x)).cvref;
    };

    ExIn ex;
    EXPECT_TRUE(ex.transform(write_cvref).has_value());
    EXPECT_EQ(cvref, CvRef::kRef);
    EXPECT_TRUE(std::as_const(ex).transform(write_cvref).has_value());
    EXPECT_EQ(cvref, CvRef::kConstRef);
    EXPECT_TRUE(std::move(ex).transform(write_cvref).has_value());
    EXPECT_EQ(cvref, CvRef::kRRef);
    EXPECT_TRUE(
        std::move(std::as_const(ex)).transform(write_cvref).has_value());
    EXPECT_EQ(cvref, CvRef::kConstRRef);

    cvref = CvRef::kNone;
    ExIn unex(unexpect);
    EXPECT_EQ(unex.transform(write_cvref).error().cvref, CvRef::kRef);
    EXPECT_EQ(cvref, CvRef::kNone);
    EXPECT_EQ(std::as_const(unex).transform(write_cvref).error().cvref,
              CvRef::kConstRef);
    EXPECT_EQ(cvref, CvRef::kNone);
    EXPECT_EQ(std::move(unex).transform(write_cvref).error().cvref,
              CvRef::kRRef);
    EXPECT_EQ(cvref, CvRef::kNone);
    EXPECT_EQ(
        std::move(std::as_const(unex)).transform(write_cvref).error().cvref,
        CvRef::kConstRRef);
    EXPECT_EQ(cvref, CvRef::kNone);

    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&>().transform(write_cvref)),
                       ExOutVoid>);
    static_assert(std::is_same_v<decltype(std::declval<const ExIn&>().transform(
                                     write_cvref)),
                                 ExOutVoid>);
    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&&>().transform(write_cvref)),
                       ExOutVoid>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const ExIn&&>().transform(write_cvref)),
                  ExOutVoid>);
  }
}

TEST(Expected, TransformError) {
  using ExIn = expected<SaveCvRef, SaveCvRef>;
  using ExOut = expected<SaveCvRef, CvRef>;

  auto get_cvref = [](auto&& x) {
    return SaveCvRef(std::forward<decltype(x)>(x)).cvref;
  };

  ExIn ex;
  EXPECT_EQ(ex.transform_error(get_cvref).value().cvref, CvRef::kRef);
  EXPECT_EQ(std::as_const(ex).transform_error(get_cvref).value().cvref,
            CvRef::kConstRef);
  EXPECT_EQ(std::move(ex).transform_error(get_cvref).value().cvref,
            CvRef::kRRef);
  EXPECT_EQ(
      std::move(std::as_const(ex)).transform_error(get_cvref).value().cvref,
      CvRef::kConstRRef);

  ExIn unex(unexpect);
  EXPECT_EQ(unex.transform_error(get_cvref).error(), CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).transform_error(get_cvref).error(),
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).transform_error(get_cvref).error(), CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).transform_error(get_cvref).error(),
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().transform_error(get_cvref)),
                     ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&>().transform_error(
                         get_cvref)),
                     ExOut>);
  static_assert(
      std::is_same_v<
          decltype(std::declval<ExIn&&>().transform_error(get_cvref)), ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&&>().transform_error(
                         get_cvref)),
                     ExOut>);
}

TEST(Expected, EqualityOperators) {
  using ExInt = expected<int, int>;
  using ExLong = expected<long, long>;

  EXPECT_EQ(ExInt(42), ExLong(42));
  EXPECT_EQ(ExLong(42), ExInt(42));
  EXPECT_EQ(ExInt(42), 42);
  EXPECT_EQ(42, ExInt(42));
  EXPECT_EQ(ExInt(42), ok(42));
  EXPECT_EQ(ok(42), ExInt(42));
  EXPECT_EQ(ExInt(unexpect, 42), unexpected(42));
  EXPECT_EQ(unexpected(42), ExInt(unexpect, 42));

  EXPECT_NE(ExInt(42), ExLong(123));
  EXPECT_NE(ExLong(123), ExInt(42));
  EXPECT_NE(ExInt(42), 123);
  EXPECT_NE(123, ExInt(42));
  EXPECT_NE(ExInt(42), ok(123));
  EXPECT_NE(ok(123), ExInt(42));
  EXPECT_NE(ExInt(unexpect, 123), unexpected(42));
  EXPECT_NE(unexpected(42), ExInt(unexpect, 123));
  EXPECT_NE(ExInt(123), unexpected(123));
  EXPECT_NE(unexpected(123), ExInt(123));
}

TEST(ExpectedDeathTest, UseAfterMove) {
  using ExpectedInt = expected<int, int>;
  using ExpectedDouble = expected<double, double>;

  ExpectedInt moved_from;
  ExpectedInt ex = std::move(moved_from);

  // Accessing moved from objects crashes.
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedInt{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedInt{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedDouble{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedDouble{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = std::move(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(ex.swap(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.swap(ex), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.operator->(), "");
  EXPECT_DEATH_IF_SUPPORTED(*moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.has_value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value_or(0), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (ex == moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (moved_from == ex), "");
  // NOLINTEND(bugprone-use-after-move)

  // Accessing inactive union-members crashes.
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{}.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{unexpect}.value(), "");
}

TEST(ExpectedVoid, Triviality) {
  using TrivialExpected = expected<void, int>;
  static_assert(std::is_trivially_destructible_v<TrivialExpected>);

  using NonTrivialExpected = expected<void, std::string>;
  static_assert(!std::is_trivially_destructible_v<NonTrivialExpected>);
}

TEST(ExpectedVoid, DefaultConstructor) {
  constexpr expected<void, int> ex;
  static_assert(ex.has_value());
  static_assert(std::is_default_constructible_v<expected<void, int>>);
}

TEST(ExpectedVoid, InPlaceConstructor) {
  constexpr expected<void, int> ex(std::in_place);
  static_assert(ex.has_value());
}

TEST(ExpectedVoid, CopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  constexpr expected<void, int> ex2 = ex1;
  static_assert(!ex2.has_value());
  EXPECT_EQ(ex2.error(), 42);
}

TEST(ExpectedVoid, MoveConstructor) {
  expected<void, StrongMoveOnly<int>> ex1 = unexpected(StrongMoveOnly(42));
  expected<void, StrongMoveOnly<int>> ex2 = std::move(ex1);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ExplicitConvertingCopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  expected<const void, Strong<int>> ex2(ex1);
  static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ImplicitConvertingCopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  expected<const void, Weak<int>> ex2 = ex1;
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ExplicitConvertingMoveConstructor) {
  expected<void, int> ex1 = unexpected(42);
  expected<const void, StrongMoveOnly<int>> ex2(std::move(ex1));
  static_assert(
      !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ImplicitConvertingMoveConstructor) {
  expected<void, int> ex1 = unexpected(42);
  expected<const void, WeakMoveOnly<int>> ex2 = std::move(ex1);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, OkConstructor) {
  constexpr expected<void, int> ex = ok();
  static_assert(ex.has_value());
}

TEST(ExpectedVoid, ExplicitErrorConstructor) {
  {
    constexpr expected<void, Strong<int>> ex(unexpected(42));
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<void, StrongMoveOnly<int>> ex(unexpected(42));
    static_assert(!std::is_constructible_v<decltype(ex), unexpected<int>&>);
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(ExpectedVoid, ImplicitErrorConstructor) {
  {
    constexpr expected<void, Weak<int>> ex = unexpected(42);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<void, WeakMoveOnly<int>> ex = unexpected(42);
    static_assert(!std::is_convertible_v<unexpected<int>&, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(ExpectedVoid, UnexpectConstructor) {
  constexpr expected<void, Strong<int>> ex(unexpect, 42);
  static_assert(!ex.has_value());
  EXPECT_EQ(ex.error().value, 42);
}

TEST(ExpectedVoid, UnexpectListConstructor) {
  expected<void, std::vector<int>> ex(unexpect, {1, 2, 3});
  EXPECT_THAT(ex, test::ErrorIs(std::vector({1, 2, 3})));
}

TEST(ExpectedVoid, CopyAssignUnexpected) {
  expected<void, int> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(42);
  EXPECT_THAT(ex, test::ErrorIs(42));

  ex = unexpected(123);
  EXPECT_THAT(ex, test::ErrorIs(123));
}

TEST(ExpectedVoid, MoveAssignUnexpected) {
  expected<void, StrongMoveOnly<int>> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(StrongMoveOnly(42));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 42);

  ex = unexpected(StrongMoveOnly(123));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 123);
}

TEST(ExpectedVoid, Emplace) {
  expected<void, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace();
  ASSERT_TRUE(ex.has_value());
}

TEST(ExpectedVoid, MemberSwap) {
  expected<void, int> ex1;
  expected<void, int> ex2 = unexpected(123);

  ex1.swap(ex2);
  EXPECT_THAT(ex1, test::ErrorIs(123));
  ASSERT_TRUE(ex2.has_value());
}

TEST(ExpectedVoid, FreeSwap) {
  expected<void, int> ex1;
  expected<void, int> ex2 = unexpected(123);

  swap(ex1, ex2);
  EXPECT_THAT(ex1, test::ErrorIs(123));
  ASSERT_TRUE(ex2.has_value());
}

TEST(ExpectedVoid, OperatorStar) {
  expected<void, int> ex;
  *ex;
  static_assert(std::is_void_v<decltype(*ex)>);
}

TEST(ExpectedVoid, HasValue) {
  constexpr expected<void, int> ex;
  static_assert(ex.has_value());

  constexpr expected<void, int> unex = unexpected(0);
  static_assert(!unex.has_value());
}

TEST(ExpectedVoid, Value) {
  expected<void, int> ex;
  ex.value();
  static_assert(std::is_void_v<decltype(ex.value())>);
}

TEST(ExpectedVoid, Error) {
  expected<void, int> ex = unexpected(0);
  EXPECT_EQ(ex.error(), 0);

  ex.error() = 1;
  EXPECT_EQ(ex.error(), 1);

  using Ex = expected<void, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().error()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().error()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().error()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().error()),
                               const int&&>);
}

TEST(ExpectedVoid, ToString) {
  // `expected<void, ...>` should have a custom string representation (that
  // prints the contained error, if applicable).
  const std::string value_str = ToString(expected<void, int>());
  EXPECT_FALSE(base::Contains(value_str, "-byte object at "));
  const std::string error_str =
      ToString(expected<void, int>(unexpected(123456)));
  EXPECT_FALSE(base::Contains(error_str, "-byte object at "));
  EXPECT_TRUE(base::Contains(error_str, "123456"));
}

TEST(ExpectedVoid, ErrorOr) {
  {
    expected<void, int> ex;
    EXPECT_EQ(ex.error_or(123), 123);

    expected<void, int> unex = unexpected(0);
    EXPECT_EQ(unex.error_or(123), 0);
  }

  {
    expected<void, WeakMoveOnly<int>> ex;
    EXPECT_EQ(std::move(ex).error_or(123).value, 123);

    expected<void, WeakMoveOnly<int>> unex = unexpected(0);
    EXPECT_EQ(std::move(unex).error_or(123).value, 0);
  }
}

TEST(ExpectedVoid, AndThen) {
  using ExIn = expected<void, SaveCvRef>;
  using ExOut = expected<bool, SaveCvRef>;

  auto get_true = []() -> ExOut { return ok(true); };

  ExIn ex;
  EXPECT_TRUE(ex.and_then(get_true).value());
  EXPECT_TRUE(std::as_const(ex).and_then(get_true).value());
  EXPECT_TRUE(std::move(ex).and_then(get_true).value());
  EXPECT_TRUE(std::move(std::as_const(ex)).and_then(get_true).value());

  ExIn unex = unexpected(SaveCvRef());
  EXPECT_EQ(unex.and_then(get_true).error().cvref, CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).and_then(get_true).error().cvref,
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).and_then(get_true).error().cvref, CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).and_then(get_true).error().cvref,
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().and_then(get_true)),
                     ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&>().and_then(get_true)),
                     ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&&>().and_then(get_true)),
                     ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&&>().and_then(get_true)),
                     ExOut>);
}

TEST(ExpectedVoid, OrElse) {
  using ExIn = expected<void, SaveCvRef>;
  using ExOut = expected<void, CvRef>;

  auto get_unex_cvref = [](auto&& x) -> ExOut {
    return unexpected(SaveCvRef(std::forward<decltype(x)>(x)).cvref);
  };

  ExIn ex;
  EXPECT_TRUE(ex.or_else(get_unex_cvref).has_value());
  EXPECT_TRUE(std::as_const(ex).or_else(get_unex_cvref).has_value());
  EXPECT_TRUE(std::move(ex).or_else(get_unex_cvref).has_value());
  EXPECT_TRUE(std::move(std::as_const(ex)).or_else(get_unex_cvref).has_value());

  ExIn unex(unexpect);
  EXPECT_EQ(unex.or_else(get_unex_cvref).error(), CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).or_else(get_unex_cvref).error(),
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).or_else(get_unex_cvref).error(), CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).or_else(get_unex_cvref).error(),
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().or_else(get_unex_cvref)),
                     ExOut>);
  static_assert(std::is_same_v<decltype(std::declval<const ExIn&>().or_else(
                                   get_unex_cvref)),
                               ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&&>().or_else(get_unex_cvref)),
                     ExOut>);
  static_assert(std::is_same_v<decltype(std::declval<const ExIn&&>().or_else(
                                   get_unex_cvref)),
                               ExOut>);
}

TEST(ExpectedVoid, Transform) {
  using ExIn = expected<void, SaveCvRef>;
  using ExOut = expected<bool, SaveCvRef>;
  auto get_true = [] { return true; };

  {
    ExIn ex;
    EXPECT_TRUE(ex.transform(get_true).value());
    EXPECT_TRUE(std::as_const(ex).transform(get_true).value());
    EXPECT_TRUE(std::move(ex).transform(get_true).value());
    EXPECT_TRUE(std::move(std::as_const(ex)).transform(get_true).value());

    ExIn unex(unexpect);
    EXPECT_EQ(unex.transform(get_true).error().cvref, CvRef::kRef);
    EXPECT_EQ(std::as_const(unex).transform(get_true).error().cvref,
              CvRef::kConstRef);
    EXPECT_EQ(std::move(unex).transform(get_true).error().cvref, CvRef::kRRef);
    EXPECT_EQ(std::move(std::as_const(unex)).transform(get_true).error().cvref,
              CvRef::kConstRRef);

    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&>().transform(get_true)),
                       ExOut>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ExIn&>().transform(get_true)), ExOut>);
    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&&>().transform(get_true)),
                       ExOut>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ExIn&&>().transform(get_true)), ExOut>);
  }

  // Test void transform.
  {
    auto do_nothing = [] {};

    ExIn ex;
    EXPECT_TRUE(ex.transform(do_nothing).has_value());
    EXPECT_TRUE(std::as_const(ex).transform(do_nothing).has_value());
    EXPECT_TRUE(std::move(ex).transform(do_nothing).has_value());
    EXPECT_TRUE(std::move(std::as_const(ex)).transform(do_nothing).has_value());

    ExIn unex(unexpect);
    EXPECT_EQ(unex.transform(do_nothing).error().cvref, CvRef::kRef);
    EXPECT_EQ(std::as_const(unex).transform(do_nothing).error().cvref,
              CvRef::kConstRef);
    EXPECT_EQ(std::move(unex).transform(do_nothing).error().cvref,
              CvRef::kRRef);
    EXPECT_EQ(
        std::move(std::as_const(unex)).transform(do_nothing).error().cvref,
        CvRef::kConstRRef);

    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&>().transform(do_nothing)),
                       ExIn>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ExIn&>().transform(do_nothing)), ExIn>);
    static_assert(
        std::is_same_v<decltype(std::declval<ExIn&&>().transform(do_nothing)),
                       ExIn>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const ExIn&&>().transform(do_nothing)),
                  ExIn>);
  }
}

TEST(ExpectedVoid, TransformError) {
  using ExIn = expected<void, SaveCvRef>;
  using ExOut = expected<void, CvRef>;

  auto get_cvref = [](auto&& x) {
    return SaveCvRef(std::forward<decltype(x)>(x)).cvref;
  };

  ExIn ex;
  EXPECT_TRUE(ex.transform_error(get_cvref).has_value());
  EXPECT_TRUE(std::as_const(ex).transform_error(get_cvref).has_value());
  EXPECT_TRUE(std::move(ex).transform_error(get_cvref).has_value());
  EXPECT_TRUE(
      std::move(std::as_const(ex)).transform_error(get_cvref).has_value());

  ExIn unex(unexpect);
  EXPECT_EQ(unex.transform_error(get_cvref).error(), CvRef::kRef);
  EXPECT_EQ(std::as_const(unex).transform_error(get_cvref).error(),
            CvRef::kConstRef);
  EXPECT_EQ(std::move(unex).transform_error(get_cvref).error(), CvRef::kRRef);
  EXPECT_EQ(std::move(std::as_const(unex)).transform_error(get_cvref).error(),
            CvRef::kConstRRef);

  static_assert(
      std::is_same_v<decltype(std::declval<ExIn&>().transform_error(get_cvref)),
                     ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&>().transform_error(
                         get_cvref)),
                     ExOut>);
  static_assert(
      std::is_same_v<
          decltype(std::declval<ExIn&&>().transform_error(get_cvref)), ExOut>);
  static_assert(
      std::is_same_v<decltype(std::declval<const ExIn&&>().transform_error(
                         get_cvref)),
                     ExOut>);
}

TEST(ExpectedVoid, EqualityOperators) {
  using Ex = expected<void, int>;
  using ConstEx = expected<const void, const int>;

  EXPECT_EQ(Ex(), ConstEx());
  EXPECT_EQ(ConstEx(), Ex());
  EXPECT_EQ(Ex(unexpect, 42), unexpected(42));
  EXPECT_EQ(unexpected(42), Ex(unexpect, 42));

  EXPECT_NE(Ex(unexpect, 123), unexpected(42));
  EXPECT_NE(unexpected(42), Ex(unexpect, 123));
  EXPECT_NE(Ex(), unexpected(0));
  EXPECT_NE(unexpected(0), Ex());
}

TEST(ExpectedVoidTest, DeathTests) {
  using ExpectedInt = expected<void, int>;
  using ExpectedDouble = expected<void, double>;

  ExpectedInt moved_from;
  ExpectedInt ex = std::move(moved_from);

  // Accessing moved from objects crashes.
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedInt{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedInt{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedDouble{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED((void)ExpectedDouble{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = std::move(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(ex.swap(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.swap(ex), "");
  EXPECT_DEATH_IF_SUPPORTED(*moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.has_value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (ex == moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (moved_from == ex), "");
  // NOLINTEND(bugprone-use-after-move)

  // Accessing inactive union-members crashes.
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{}.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{unexpect}.value(), "");
}

}  // namespace

}  // namespace base
