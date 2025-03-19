// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/functional/overloaded.h"

#include <variant>

namespace base {

void LambdaMissingForVariantElement() {
  // `std::visit()` may only be called on an `Overloaded` that can actually
  // handle all potential input variant types.
  struct A {};
  struct B {};
  std::variant<A, B> var = A{};
  std::visit(Overloaded{[](A& pack) { return "A"; }}, var);  // expected-error-re@*:* {{static assertion failed due to requirement {{.*}} `std::visit` requires the visitor to be exhaustive}}
  // expected-error@*:* {{attempt to use a deleted function}}
  // expected-error@*:* {{attempt to use a deleted function}}
  // expected-error@*:* {{cannot deduce return type 'auto' from returned value of type '<overloaded function type>'}}
}

}  // namespace base
