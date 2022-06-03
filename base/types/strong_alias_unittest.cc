// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/strong_alias.h"

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/template_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// For test correctnenss, it's important that these getters return lexically
// incrementing values as |index| grows.
template <typename T>
T GetExampleValue(int index);

template <>
int GetExampleValue<int>(int index) {
  return 5 + index;
}
template <>
uint64_t GetExampleValue<uint64_t>(int index) {
  return 500U + index;
}

template <>
std::string GetExampleValue<std::string>(int index) {
  return std::string('a', index);
}

template <typename T, typename U>
bool StreamOutputSame(const T& a, const U& b) {
  std::stringstream ssa;
  ssa << a;
  std::stringstream ssb;
  ssb << b;
  return ssa.str() == ssb.str();
}

}  // namespace

template <typename T>
class StrongAliasTest : public ::testing::Test {};

using TestedTypes = ::testing::Types<int, uint64_t, std::string>;
TYPED_TEST_SUITE(StrongAliasTest, TestedTypes);

TYPED_TEST(StrongAliasTest, ValueAccessesUnderlyingValue) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;

  // Const value getter.
  const FooAlias const_alias(GetExampleValue<TypeParam>(1));
  EXPECT_EQ(GetExampleValue<TypeParam>(1), const_alias.value());
  static_assert(std::is_const<typename std::remove_reference<decltype(
                    const_alias.value())>::type>::value,
                "Reference returned by const value getter should be const.");
}

TYPED_TEST(StrongAliasTest, ExplicitConversionToUnderlyingValue) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;

  const FooAlias const_alias(GetExampleValue<TypeParam>(1));
  EXPECT_EQ(GetExampleValue<TypeParam>(1), static_cast<TypeParam>(const_alias));
}

TYPED_TEST(StrongAliasTest, CanBeCopyConstructed) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  FooAlias alias(GetExampleValue<TypeParam>(0));
  FooAlias copy_constructed = alias;
  EXPECT_EQ(copy_constructed, alias);

  FooAlias copy_assigned;
  copy_assigned = alias;
  EXPECT_EQ(copy_assigned, alias);
}

TYPED_TEST(StrongAliasTest, CanBeMoveConstructed) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  FooAlias alias(GetExampleValue<TypeParam>(0));
  FooAlias move_constructed = std::move(alias);
  EXPECT_EQ(move_constructed, FooAlias(GetExampleValue<TypeParam>(0)));

  FooAlias alias2(GetExampleValue<TypeParam>(2));
  FooAlias move_assigned;
  move_assigned = std::move(alias2);
  EXPECT_EQ(move_assigned, FooAlias(GetExampleValue<TypeParam>(2)));

  // Check that FooAlias is nothrow move constructible. This matters for
  // performance when used in std::vectors.
  static_assert(std::is_nothrow_move_constructible<FooAlias>::value,
                "Error: Alias is not nothow move constructible");
}

TYPED_TEST(StrongAliasTest, CanBeConstructedFromMoveOnlyType) {
  // Note, using a move-only unique_ptr to T:
  using FooAlias = StrongAlias<class FooTag, std::unique_ptr<TypeParam>>;

  FooAlias a(std::make_unique<TypeParam>(GetExampleValue<TypeParam>(0)));
  EXPECT_EQ(*a.value(), GetExampleValue<TypeParam>(0));

  auto bare_value = std::make_unique<TypeParam>(GetExampleValue<TypeParam>(1));
  FooAlias b(std::move(bare_value));
  EXPECT_EQ(*b.value(), GetExampleValue<TypeParam>(1));
}

TYPED_TEST(StrongAliasTest, MutableOperatorArrow) {
  // Note, using a move-only unique_ptr to T:
  using Ptr = std::unique_ptr<TypeParam>;
  using FooAlias = StrongAlias<class FooTag, Ptr>;

  FooAlias a(std::make_unique<TypeParam>());
  EXPECT_TRUE(a.value());

  // Check that `a` can be modified through the use of operator->.
  a->reset();

  EXPECT_FALSE(a.value());
}

TYPED_TEST(StrongAliasTest, MutableOperatorStar) {
  // Note, using a move-only unique_ptr to T:
  using Ptr = std::unique_ptr<TypeParam>;
  using FooAlias = StrongAlias<class FooTag, Ptr>;

  FooAlias a(std::make_unique<TypeParam>());
  FooAlias b(std::make_unique<TypeParam>());
  EXPECT_TRUE(*a);
  EXPECT_TRUE(*b);

  // Check that both the mutable l-value and r-value overloads work and we can
  // move out of the aliases.
  { Ptr ignore(*std::move(a)); }
  { Ptr ignore(std::move(*b)); }

  EXPECT_FALSE(a.value());
  EXPECT_FALSE(b.value());
}

TYPED_TEST(StrongAliasTest, MutableValue) {
  // Note, using a move-only unique_ptr to T:
  using Ptr = std::unique_ptr<TypeParam>;
  using FooAlias = StrongAlias<class FooTag, Ptr>;

  FooAlias a(std::make_unique<TypeParam>());
  FooAlias b(std::make_unique<TypeParam>());
  EXPECT_TRUE(a.value());
  EXPECT_TRUE(b.value());

  // Check that both the mutable l-value and r-value overloads work and we can
  // move out of the aliases.
  { Ptr ignore(std::move(a).value()); }
  { Ptr ignore(std::move(b.value())); }

  EXPECT_FALSE(a.value());
  EXPECT_FALSE(b.value());
}

TYPED_TEST(StrongAliasTest, CanBeWrittenToOutputStream) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;

  const FooAlias a(GetExampleValue<TypeParam>(0));
  EXPECT_TRUE(StreamOutputSame(GetExampleValue<TypeParam>(0), a)) << a;
}

TYPED_TEST(StrongAliasTest, SizeSameAsUnderlyingType) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  static_assert(sizeof(FooAlias) == sizeof(TypeParam),
                "StrongAlias should be as large as the underlying type.");
}

TYPED_TEST(StrongAliasTest, IsDefaultConstructible) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  static_assert(std::is_default_constructible<FooAlias>::value,
                "Should be possible to default-construct a StrongAlias.");
  static_assert(
      std::is_trivially_default_constructible<FooAlias>::value ==
          std::is_trivially_default_constructible<TypeParam>::value,
      "Should be possible to trivially default-construct a StrongAlias iff the "
      "underlying type is trivially default constructible.");
}

TEST(StrongAliasTest, TrivialTypeAliasIsStandardLayout) {
  using FooAlias = StrongAlias<class FooTag, int>;
  static_assert(std::is_standard_layout<FooAlias>::value,
                "int-based alias should have standard layout. ");
  static_assert(std::is_trivially_copyable<FooAlias>::value,
                "int-based alias should be trivially copyable. ");
}

TYPED_TEST(StrongAliasTest, CannotBeCreatedFromDifferentAlias) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  using BarAlias = StrongAlias<class BarTag, TypeParam>;
  static_assert(!std::is_constructible<FooAlias, BarAlias>::value,
                "Should be impossible to construct FooAlias from a BarAlias.");
  static_assert(!std::is_convertible<BarAlias, FooAlias>::value,
                "Should be impossible to convert a BarAlias into FooAlias.");
}

TYPED_TEST(StrongAliasTest, CannotBeImplicitlyConverterToUnderlyingValue) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  static_assert(!std::is_convertible<FooAlias, TypeParam>::value,
                "Should be impossible to implicitly convert a StrongAlias into "
                "an underlying type.");
}

TYPED_TEST(StrongAliasTest, ComparesEqualToSameValue) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  // Comparison to self:
  const FooAlias a = FooAlias(GetExampleValue<TypeParam>(0));
  EXPECT_EQ(a, a);
  EXPECT_FALSE(a != a);
  EXPECT_TRUE(a >= a);
  EXPECT_TRUE(a <= a);
  EXPECT_FALSE(a > a);
  EXPECT_FALSE(a < a);
  // Comparison to other equal object:
  const FooAlias b = FooAlias(GetExampleValue<TypeParam>(0));
  EXPECT_EQ(a, b);
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a >= b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a > b);
  EXPECT_FALSE(a < b);
}

TYPED_TEST(StrongAliasTest, ComparesCorrectlyToDifferentValue) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  const FooAlias a = FooAlias(GetExampleValue<TypeParam>(0));
  const FooAlias b = FooAlias(GetExampleValue<TypeParam>(1));
  EXPECT_NE(a, b);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(b >= a);
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(b > a);
  EXPECT_TRUE(a < b);
}

TEST(StrongAliasTest, CanBeDerivedFrom) {
  // Aliases can be enriched by custom operations or validations if needed.
  // Ideally, one could go from a 'using' declaration to a derived class to add
  // those methods without the need to change any other code.
  class CountryCode : public StrongAlias<CountryCode, std::string> {
   public:
    CountryCode(const std::string& value)
        : StrongAlias<CountryCode, std::string>::StrongAlias(value) {
      if (value_.length() != 2) {
        // Country code invalid!
        value_.clear();  // is_null() will return true.
      }
    }

    bool is_null() const { return value_.empty(); }
  };

  CountryCode valid("US");
  EXPECT_FALSE(valid.is_null());

  CountryCode invalid("United States");
  EXPECT_TRUE(invalid.is_null());
}

TEST(StrongAliasTest, CanWrapComplexStructures) {
  // A pair of strings implements odering and can, in principle, be used as
  // a base of StrongAlias.
  using PairOfStrings = std::pair<std::string, std::string>;
  using ComplexAlias = StrongAlias<class FooTag, PairOfStrings>;

  ComplexAlias a1{std::make_pair("aaa", "bbb")};
  ComplexAlias a2{std::make_pair("ccc", "ddd")};
  EXPECT_TRUE(a1 < a2);

  EXPECT_TRUE(a1.value() == PairOfStrings("aaa", "bbb"));

  // Note a caveat, an std::pair doesn't have an overload of operator<<, and it
  // cannot be easily added since ADL rules would require it to be in the std
  // namespace. So we can't print ComplexAlias.
}

TYPED_TEST(StrongAliasTest, CanBeKeysInStdUnorderedMap) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  std::unordered_map<FooAlias, std::string, typename FooAlias::Hasher> map;

  FooAlias k1(GetExampleValue<TypeParam>(0));
  FooAlias k2(GetExampleValue<TypeParam>(1));

  map[k1] = "value1";
  map[k2] = "value2";

  EXPECT_EQ(map[k1], "value1");
  EXPECT_EQ(map[k2], "value2");
}

TYPED_TEST(StrongAliasTest, CanBeKeysInStdMap) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  std::map<FooAlias, std::string> map;

  FooAlias k1(GetExampleValue<TypeParam>(0));
  FooAlias k2(GetExampleValue<TypeParam>(1));

  map[k1] = "value1";
  map[k2] = "value2";

  EXPECT_EQ(map[k1], "value1");
  EXPECT_EQ(map[k2], "value2");
}

TYPED_TEST(StrongAliasTest, CanDifferentiateOverloads) {
  using FooAlias = StrongAlias<class FooTag, TypeParam>;
  using BarAlias = StrongAlias<class BarTag, TypeParam>;
  class Scope {
   public:
    static std::string Overload(FooAlias) { return "FooAlias"; }
    static std::string Overload(BarAlias) { return "BarAlias"; }
  };
  EXPECT_EQ("FooAlias", Scope::Overload(FooAlias()));
  EXPECT_EQ("BarAlias", Scope::Overload(BarAlias()));
}

TEST(StrongAliasTest, EnsureConstexpr) {
  using FooAlias = StrongAlias<class FooTag, int>;
  using BarAlias = StrongAlias<class BarTag, base::StringPiece>;

  // Check constructors.
  static constexpr FooAlias kZero{};
  static constexpr FooAlias kOne(1);
  static constexpr BarAlias kHello("Hello");

  // Check operator->.
  static_assert(kHello->size() == 5, "");

  // Check operator*.
  static_assert(*kZero == 0, "");
  static_assert(*kOne == 1, "");
  static_assert(*kHello == "Hello", "");

  // Check value().
  static_assert(kZero.value() == 0, "");
  static_assert(kOne.value() == 1, "");

  // Check explicit conversions to underlying type.
  static_assert(static_cast<int>(kZero) == 0, "");
  static_assert(static_cast<int>(kOne) == 1, "");

  // Check comparison operations.
  static_assert(kZero == kZero, "");
  static_assert(kZero != kOne, "");
  static_assert(kZero < kOne, "");
  static_assert(kZero <= kOne, "");
  static_assert(kOne > kZero, "");
  static_assert(kOne >= kZero, "");
}

// This next test is compile-time, and thus not in an actual TEST since it would
// just result in running an empty test.
void StreamOperatorExists() {
  // Aliases of ints should be streamable because ints are streamable.
  using StreamableAlias = StrongAlias<class IntTag, int>;
  static_assert(base::internal::SupportsOstreamOperator<StreamableAlias>::value,
                "");

  // Aliases of a class which does not expose a stream operator should
  // themselves not be streamable.
  class Scope {
   public:
    Scope() = default;
  };
  using NonStreamableAlias = StrongAlias<class ScopeTag, Scope>;
  static_assert(
      !base::internal::SupportsOstreamOperator<NonStreamableAlias>::value, "");
}

}  // namespace base
