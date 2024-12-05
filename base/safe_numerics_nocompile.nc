// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <type_traits>

#include "base/numerics/safe_conversions.h"

namespace base {

void StrictCastToNonSubsuming() {
  // Should not be able to `strict_cast` to a non-subsuming type.
  [[maybe_unused]] const auto a = strict_cast<int>(size_t{0});  // expected-error {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] const auto b = strict_cast<size_t>(0);       // expected-error {{no matching function for call to 'strict_cast'}}
}

void StrictNumericConstruction() {
  // Should not be able to construct `StrictNumeric` from a non-subsuming type.
  [[maybe_unused]] StrictNumeric<size_t> a(1);       // expected-error@*:* {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] StrictNumeric<int> b(size_t{1});  // expected-error@*:* {{no matching function for call to 'strict_cast'}}
  [[maybe_unused]] StrictNumeric<int> c(1.0f);       // expected-error@*:* {{no matching function for call to 'strict_cast'}}
}

}  // namespace base
