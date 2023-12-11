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
  [](absl::FunctionRef<void()> ref) { BindOnce(ref); }([] {});       // expected-error@*:* {{Functor may not be a FunctionRef, since that is a non-owning reference that may go out of scope before the callback executes.}}
  [](absl::FunctionRef<void()> ref) { BindRepeating(ref); }([] {});  // expected-error@*:* {{Functor may not be a FunctionRef, since that is a non-owning reference that may go out of scope before the callback executes.}}
  [](FunctionRef<void()> ref) { BindOnce(ref); }([] {});             // expected-error@*:* {{Functor may not be a FunctionRef, since that is a non-owning reference that may go out of scope before the callback executes.}}
  [](FunctionRef<void()> ref) { BindRepeating(ref); }([] {});        // expected-error@*:* {{Functor may not be a FunctionRef, since that is a non-owning reference that may go out of scope before the callback executes.}}
}

}  // namespace base
