// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/functional/overloaded.h"

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

void LambdaMissingForVariantElement() {
  // `absl::visit()` may only be called on an `Overloaded` that can actually
  // handle all potential input variant types.
  struct A {};
  struct B {};
  absl::variant<A, B> var = A{};
  absl::visit(Overloaded{[](A& pack) { return "A"; }}, var);  // expected-error-re@*:* {{no type named 'type' in 'absl::type_traits_internal::result_of<base::Overloaded<(lambda at {{.*}})> (B &)>'}}
}

}  // namespace base
