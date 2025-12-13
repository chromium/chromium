// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <type_traits>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"

namespace base {

void StrictCastToNonSubsuming() {
  // Should not be able to `strict_cast` to a non-subsuming type.
  [[maybe_unused]] const auto a = strict_cast<int>(size_t{0});  // expected-error {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] const auto b =
      strict_cast<int>(std::integral_constant<size_t, 0>());    // expected-error {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] const auto c = strict_cast<size_t>(0);       // expected-error {{no matching function for call to 'strict_cast'}}
}

void StrictNumericConstruction() {
  // Should not be able to construct `StrictNumeric` from a non-subsuming type.
  [[maybe_unused]] StrictNumeric<size_t> a(1);                                 // expected-error@*:* {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] StrictNumeric<size_t> b{std::integral_constant<int, 1>()};  // expected-error@*:* {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] StrictNumeric<int> c(size_t{1});                            // expected-error@*:* {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] StrictNumeric<int> d(1.0f);                                 // expected-error@*:* {{no matching function for call to 'strict_cast'}}

  // Can't use `std::integral_constant<non-integral T>` to construct
  // `StrictNumeric`.
  //
  // TODO(pkasting): There's no particular reason we couldn't support this.
  [[maybe_unused]] StrictNumeric<float> e{
      std::integral_constant<float, 1.0f>()};  // expected-error@*:* {{no matching function for call to 'strict_cast'}}
}

void IsValidAndWithBadPredicate() {
  base::CheckedNumeric<int> value;

  // No argument.
  value.IsValidAnd([] { return true; });  // expected-error {{no matching member function for call to 'IsValidAnd'}}
  // Too many arguments.
  value.IsValidAnd([] (int, float) { return true; });  // expected-error {{no matching member function for call to 'IsValidAnd'}}
  // Non-arithmetic argument.
  value.IsValidAnd([] (const void*) { return true; });  // expected-error {{no matching member function for call to 'IsValidAnd'}}
  // Non-bool return type.
  value.IsValidAnd([] {});  // expected-error {{no matching member function for call to 'IsValidAnd'}}
  value.IsValidAnd([] { return 2; });  // expected-error {{no matching member function for call to 'IsValidAnd'}}
}

void IsInvalidOrWithBadPredicate() {
  base::CheckedNumeric<int> value;

  // No argument.
  value.IsInvalidOr([] { return true; });  // expected-error {{no matching member function for call to 'IsInvalidOr'}}
  // Too many arguments.
  value.IsInvalidOr([] (int, float) { return true; });  // expected-error {{no matching member function for call to 'IsInvalidOr'}}
  // Non-arithmetic argument.
  value.IsInvalidOr([] (const void*) { return true; });  // expected-error {{no matching member function for call to 'IsInvalidOr'}}
  // Non-bool return type.
  value.IsInvalidOr([] {});  // expected-error {{no matching member function for call to 'IsInvalidOr'}}
  value.IsInvalidOr([] { return 2; });  // expected-error {{no matching member function for call to 'IsInvalidOr'}}
}
}  // namespace base
