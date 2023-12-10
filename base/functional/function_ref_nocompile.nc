// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

namespace base {

void NoImplicitVoidify() {
  // Return values cannot be implicitly discarded.
  FunctionRef<void()> ref([] { return 0; });  // expected-error {{no matching constructor for initialization of 'FunctionRef<void ()>'}}
}

void CannotBindFunctionRefs() {
  // `Bind{Once,Repeating}` do not accept `FunctionRef` args due to potential
  // lifetime concerns.
  [](absl::FunctionRef<void()> ref) { BindOnce(ref); }([] {});       // expected-error@*:* {{base::Bind{Once,Repeating} require strong ownership: non-owning function references may not be bound as the functor due to potential lifetime issues.}}
  [](absl::FunctionRef<void()> ref) { BindRepeating(ref); }([] {});  // expected-error@*:* {{base::Bind{Once,Repeating} require strong ownership: non-owning function references may not be bound as the functor due to potential lifetime issues.}}
  [](FunctionRef<void()> ref) { BindOnce(ref); }([] {});             // expected-error@*:* {{base::Bind{Once,Repeating} require strong ownership: non-owning function references may not be bound as the functor due to potential lifetime issues.}}
  [](FunctionRef<void()> ref) { BindRepeating(ref); }([] {});        // expected-error@*:* {{base::Bind{Once,Repeating} require strong ownership: non-owning function references may not be bound as the functor due to potential lifetime issues.}}
}

}  // namespace base
