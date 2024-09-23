// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/traits_bag.h"

#include <optional>

#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace trait_helpers {
namespace {

struct ExampleTrait {};

struct ExampleTrait2 {};

enum class EnumTraitA { A, B, C };

enum class EnumTraitB { ONE, TWO };

struct TestTraits {
  // List of traits that are valid inputs for the constructor below.
  struct ValidTrait {
    ValidTrait(ExampleTrait);
    ValidTrait(EnumTraitA);
    ValidTrait(EnumTraitB);
  };

  template <class... ArgTypes>
    requires trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr TestTraits(ArgTypes... args)
      : has_example_trait(trait_helpers::HasTrait<ExampleTrait, ArgTypes...>()),
        enum_trait_a(
            trait_helpers::GetEnum<EnumTraitA, EnumTraitA::A>(args...)),
        enum_trait_b(
            trait_helpers::GetEnum<EnumTraitB, EnumTraitB::ONE>(args...)) {}

  const bool has_example_trait;
  const EnumTraitA enum_trait_a;
  const EnumTraitB enum_trait_b;
};

// Like TestTraits, except ExampleTrait is filtered away.
struct FilteredTestTraits : public TestTraits {
  template <class... ArgTypes>
    requires trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr FilteredTestTraits(ArgTypes... args)
      : TestTraits(Exclude<ExampleTrait>::Filter(args)...) {}
};

struct RequiredEnumTestTraits {
  // List of traits that are required inputs for the constructor below.
  struct ValidTrait {
    ValidTrait(EnumTraitA);
  };

  // We require EnumTraitA to be specified.
  template <class... ArgTypes>
    requires trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr RequiredEnumTestTraits(ArgTypes... args)
      : enum_trait_a(trait_helpers::GetEnum<EnumTraitA>(args...)) {}

  const EnumTraitA enum_trait_a;
};

struct OptionalEnumTestTraits {
  // List of traits that are optional inputs for the constructor below.
  struct ValidTrait {
    ValidTrait(EnumTraitA);
  };

  // EnumTraitA can optionally be specified.
  template <class... ArgTypes>
    requires trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr OptionalEnumTestTraits(ArgTypes... args)
      : enum_trait_a(trait_helpers::GetOptionalEnum<EnumTraitA>(args...)) {}

  const std::optional<EnumTraitA> enum_trait_a;
};

}  // namespace

TEST(TraitsBagTest, DefaultConstructor) {
  constexpr TestTraits trait_test_class;

  EXPECT_FALSE(trait_test_class.has_example_trait);
}

TEST(TraitsBagTest, HasTrait) {
  constexpr TestTraits with_trait(ExampleTrait{});
  constexpr TestTraits without_trait;

  EXPECT_TRUE(with_trait.has_example_trait);
  EXPECT_FALSE(without_trait.has_example_trait);
}

TEST(TraitsBagTest, GetEnumWithDefault) {
  constexpr TestTraits defaults;

  EXPECT_EQ(defaults.enum_trait_a, EnumTraitA::A);
  EXPECT_EQ(defaults.enum_trait_b, EnumTraitB::ONE);

  constexpr TestTraits a(EnumTraitA::A);
  constexpr TestTraits b(EnumTraitA::B);
  constexpr TestTraits c(EnumTraitA::C);

  EXPECT_EQ(a.enum_trait_a, EnumTraitA::A);
  EXPECT_EQ(a.enum_trait_b, EnumTraitB::ONE);

  EXPECT_EQ(b.enum_trait_a, EnumTraitA::B);
  EXPECT_EQ(b.enum_trait_b, EnumTraitB::ONE);

  EXPECT_EQ(c.enum_trait_a, EnumTraitA::C);
  EXPECT_EQ(c.enum_trait_b, EnumTraitB::ONE);

  constexpr TestTraits a_one(EnumTraitA::A, EnumTraitB::ONE);
  constexpr TestTraits b_one(EnumTraitA::B, EnumTraitB::ONE);
  constexpr TestTraits c_one(EnumTraitA::C, EnumTraitB::ONE);

  EXPECT_EQ(a_one.enum_trait_a, EnumTraitA::A);
  EXPECT_EQ(a_one.enum_trait_b, EnumTraitB::ONE);

  EXPECT_EQ(b_one.enum_trait_a, EnumTraitA::B);
  EXPECT_EQ(b_one.enum_trait_b, EnumTraitB::ONE);

  EXPECT_EQ(c_one.enum_trait_a, EnumTraitA::C);
  EXPECT_EQ(c_one.enum_trait_b, EnumTraitB::ONE);

  constexpr TestTraits a_two(EnumTraitA::A, EnumTraitB::TWO);
  constexpr TestTraits b_two(EnumTraitA::B, EnumTraitB::TWO);
  constexpr TestTraits c_two(EnumTraitA::C, EnumTraitB::TWO);

  EXPECT_EQ(a_two.enum_trait_a, EnumTraitA::A);
  EXPECT_EQ(a_two.enum_trait_b, EnumTraitB::TWO);

  EXPECT_EQ(b_two.enum_trait_a, EnumTraitA::B);
  EXPECT_EQ(b_two.enum_trait_b, EnumTraitB::TWO);

  EXPECT_EQ(c_two.enum_trait_a, EnumTraitA::C);
  EXPECT_EQ(c_two.enum_trait_b, EnumTraitB::TWO);
}

TEST(TraitsBagTest, RequiredEnum) {
  constexpr RequiredEnumTestTraits a(EnumTraitA::A);
  constexpr RequiredEnumTestTraits b(EnumTraitA::B);
  constexpr RequiredEnumTestTraits c(EnumTraitA::C);

  EXPECT_EQ(a.enum_trait_a, EnumTraitA::A);
  EXPECT_EQ(b.enum_trait_a, EnumTraitA::B);
  EXPECT_EQ(c.enum_trait_a, EnumTraitA::C);
}

TEST(TraitsBagTest, OptionalEnum) {
  constexpr OptionalEnumTestTraits not_set;
  constexpr OptionalEnumTestTraits set(EnumTraitA::B);

  EXPECT_FALSE(not_set.enum_trait_a.has_value());
  ASSERT_TRUE(set.enum_trait_a.has_value());
  EXPECT_EQ(*set.enum_trait_a, EnumTraitA::B);
}

TEST(TraitsBagTest, ValidTraitInheritance) {
  struct ValidTraitsA {
    ValidTraitsA(EnumTraitA);
  };

  struct ValidTraitsB {
    ValidTraitsB(ValidTraitsA);
    ValidTraitsB(EnumTraitB);
  };

  static_assert(AreValidTraits<ValidTraitsA, EnumTraitA>, "");
  static_assert(AreValidTraits<ValidTraitsB, EnumTraitA, EnumTraitB>, "");
}

TEST(TraitsBagTest, Filtering) {
  using Predicate = Exclude<ExampleTrait, EnumTraitA>;
  static_assert(std::is_same_v<ExampleTrait2,
                               decltype(Predicate::Filter(ExampleTrait2{}))>,
                "ExampleTrait2 should not be filtered");

  static_assert(
      std::is_same_v<EmptyTrait, decltype(Predicate::Filter(ExampleTrait{}))>,
      "ExampleTrait should be filtered");

  static_assert(
      std::is_same_v<EmptyTrait, decltype(Predicate::Filter(EnumTraitA::A))>,
      "EnumTraitA should be filtered");

  static_assert(
      std::is_same_v<EnumTraitB, decltype(Predicate::Filter(EnumTraitB::TWO))>,
      "EnumTraitB should not be filtered");

  static_assert(
      std::is_same_v<EmptyTrait, decltype(Predicate::Filter(EmptyTrait{}))>,
      "EmptyTrait should not be filtered");
}

TEST(TraitsBagTest, FilteredTestTraits) {
  FilteredTestTraits filtered(ExampleTrait(), EnumTraitA::C, EnumTraitB::TWO);

  // ExampleTrait should have been filtered away.
  EXPECT_FALSE(filtered.has_example_trait);

  // The other traits should have been set however.
  EXPECT_EQ(filtered.enum_trait_a, EnumTraitA::C);
  EXPECT_EQ(filtered.enum_trait_b, EnumTraitB::TWO);
}

TEST(TraitsBagTest, EmptyTraitIsValid) {
  static_assert(IsValidTrait<TestTraits::ValidTrait, EmptyTrait>, "");
}

}  // namespace trait_helpers
}  // namespace base
