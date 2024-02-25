// Copyright 2018 The Chromium Authors
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

  template <class... ArgTypes>
    requires trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>
  constexpr TestTraits(ArgTypes... args)
      : required_trait(trait_helpers::GetEnum<RequiredTrait>(args...)),
        boolean_trait(trait_helpers::HasTrait<BooleanTrait, ArgTypes...>()) {}

  const RequiredTrait required_trait;
  const bool boolean_trait;
};

constexpr TestTraits traits = {};  // expected-error {{constexpr variable 'traits' must be initialized by a constant expression}}
                                   // expected-error@base/traits_bag.h:* {{The traits bag is missing a required trait.}}
                                   // expected-error@*:* {{no matching constructor for initialization of 'base::trait_helpers::RequiredEnumTraitFilter<base::RequiredTrait>'}}

constexpr TestTraits traits2 = {RequiredTrait::A, NotAValidTrait{}};  // expected-error {{no matching constructor for initialization of 'const TestTraits'}}
                                                                      // expected-error@*:* {{type occurs more than once in type list}}

constexpr TestTraits traits3 = {RequiredTrait::A, RequiredTrait::B};  // expected-error {{constexpr variable 'traits3' must be initialized by a constant expression}}
                                                                      // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}

constexpr TestTraits traits4 = {RequiredTrait::A, BooleanTrait(),  // expected-error {{constexpr variable 'traits4' must be initialized by a constant expression}}
                                BooleanTrait()};                   // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}

}  // namespace base
