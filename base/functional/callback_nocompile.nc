// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <utility>

#include "base/functional/callback.h"

namespace base {

void ComparingDifferentCallbackTypes() {
  // Comparing callbacks requires that they be the same type.
  RepeatingCallback<void()> c1;
  RepeatingCallback<int()> c2;
  c1 == c2;  // expected-error {{invalid operands to binary expression ('RepeatingCallback<void ()>' and 'RepeatingCallback<int ()>')}}
}

void ConvertingSuperclassReturn() {
  // A callback that returns a `Derived` should not be implicitly converted to a
  // callback that returns a `Base`. This is technically safe, but it surprises
  // users and generally means the author is doing something other than what
  // they intended.
  struct Base {};
  struct Derived : public Base {};
  RepeatingCallback<Derived()> cb_derived;
  RepeatingCallback<Base()> cb_base = cb_derived;  // expected-error {{no viable conversion from 'RepeatingCallback<Derived ()>' to 'RepeatingCallback<Base ()>'}}
  cb_base = cb_derived;                            // expected-error {{no viable overloaded '='}}
}

void ChainingWithTypeMismatch() {
  // Calling `.Then()` requires that the return type of the first callback be
  // convertible to the arg type of the second callback.

  // non-void -> incompatible non-void
  OnceCallback<int*()> returns_ptr_int_once;
  OnceCallback<void(float*)> takes_ptr_float_once;
  RepeatingCallback<int*()> returns_ptr_int_repeating;
  // Using distinct return types causes distinct `RepeatingCallback` template
  // instantiations, so we get assertion failures below where we expect.
  RepeatingCallback<void(float*)> takes_ptr_float_repeating1;
  RepeatingCallback<int(float*)> takes_ptr_float_repeating2;
  std::move(returns_ptr_int_once).Then(std::move(takes_ptr_float_once));             // expected-error@*:* {{|then| callback's parameter must be constructible from return type of |this|.}}
  returns_ptr_int_repeating.Then(takes_ptr_float_repeating1);                        // expected-error@*:* {{|then| callback's parameter must be constructible from return type of |this|.}}
  std::move(returns_ptr_int_repeating).Then(std::move(takes_ptr_float_repeating2));  // expected-error@*:* {{|then| callback's parameter must be constructible from return type of |this|.}}

  // void -> non-void
  OnceCallback<void()> returns_void_once;
  OnceCallback<void(float)> takes_float_once;
  RepeatingCallback<void()> returns_void_repeating;
  RepeatingCallback<void(float)> takes_float_repeating1;
  RepeatingCallback<int(float)> takes_float_repeating2;
  std::move(returns_void_once).Then(std::move(takes_float_once));             // expected-error@*:* {{|then| callback cannot accept parameters if |this| has a void return type.}}
  returns_void_repeating.Then(takes_float_repeating1);                        // expected-error@*:* {{|then| callback cannot accept parameters if |this| has a void return type.}}
  std::move(returns_void_repeating).Then(std::move(takes_float_repeating2));  // expected-error@*:* {{|then| callback cannot accept parameters if |this| has a void return type.}}

  // non-void -> void
  OnceCallback<int()> returns_int_once;
  OnceCallback<void()> takes_void_once;
  RepeatingCallback<int()> returns_int_repeating;
  RepeatingCallback<void()> takes_void_repeating1;
  RepeatingCallback<int()> takes_void_repeating2;
  std::move(returns_int_once).Then(std::move(takes_void_once));             // expected-error@*:* {{|then| callback must accept exactly one parameter if |this| has a non-void return type.}}
  returns_int_repeating.Then(takes_void_repeating1);                        // expected-error@*:* {{|then| callback must accept exactly one parameter if |this| has a non-void return type.}}
  std::move(returns_int_repeating).Then(std::move(takes_void_repeating2));  // expected-error@*:* {{|then| callback must accept exactly one parameter if |this| has a non-void return type.}}
}

}  // namespace base
