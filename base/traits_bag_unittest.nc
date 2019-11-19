// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/traits_bag.h"

namespace base {

enum class RequiredTrait {
  A,
  B,
  C
};

struct BooleanTrait {};

struct NotAValidTrait {};

struct TestTraits {
  // List of traits that are valid inputs for the constructor below.
  struct ValidTrait {
    ValidTrait(RequiredTrait);
    ValidTrait(BooleanTrait);
  };

  template <class... ArgTypes,
            class CheckArgumentsAreValid = std::enable_if_t<
                trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value>>
  constexpr TestTraits(ArgTypes... args)
      : required_trait(trait_helpers::GetEnum<RequiredTrait>(args...)),
        boolean_trait(trait_helpers::HasTrait<BooleanTrait, ArgTypes...>()) {}

  const RequiredTrait required_trait;
  const bool boolean_trait;
};

#if defined(NCTEST_TRAITS_BAG_REQUIRED_TRAIT_NOT_SET)  // [r"The traits bag is missing a required trait."]
constexpr TestTraits traits = {};
#elif defined(NCTEST_TRAITS_BAG_INVALID_TRAIT)  // [r"no matching constructor for initialization of 'const base::TestTraits'"]
constexpr TestTraits traits = {RequiredTrait::A, NotAValidTrait{}};
#elif defined(NCTEST_TASK_TRAITS_MULTIPLE_REQUIRED_TRAIT)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TestTraits traits = {RequiredTrait::A, RequiredTrait::B};
#elif defined(NCTEST_TASK_TRAITS_REDUNDANT_BOOLEAN_TRAIT)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TestTraits traits = {RequiredTrait::A, BooleanTrait(),
                               BooleanTrait()};
#endif

}  // namespace base
