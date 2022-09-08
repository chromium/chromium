// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/bind.h"
#include "base/callback.h"
#include "base/functional/function_ref.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

namespace base {

#if defined(NCTEST_NO_IMPLICIT_RETURN_TYPE_CONVERSIONS)  // [r"note: candidate template ignored: requirement 'std::is_same_v<void \(\), int \(\)>' was not satisfied \[with Functor = \(lambda at [^)]+\)\]"]

void WontCompile() {
  auto returns_int = [] () { return 42; };
  FunctionRef<void()> ref(returns_int);
}

#elif defined(NCTEST_NO_IMPLICIT_DERIVED_TO_BASE_CONVERSIONS)  // [r"note: candidate template ignored: requirement 'std::is_same_v<void \(base::Derived \*\), void \(base::Base \*\)>' was not satisfied \[with Functor = \(lambda at [^)]+\)\]"]

class Base { };
class Derived : public Base { };

void WontCompile() {
  auto takes_base = [] (Base*) { };
  FunctionRef<void(Derived*)> ref(takes_base);
}

#elif defined(NCTEST_BIND_ONCE_TO_ABSL_FUNCTION_REF)  // [r"fatal error: static assertion failed due to requirement 'AlwaysFalse<void \(\)>': base::Bind{Once,Repeating} require strong ownership: non-owning function references may not bound as the functor due to potential lifetime issues\."]

void WontCompile() {
  [] (absl::FunctionRef<void()> ref) {
    BindOnce(ref).Run();
  }([] {});
}

#elif defined(NCTEST_BIND_REPEATING_TO_ABSL_FUNCTION_REF)  // [r"fatal error: static assertion failed due to requirement 'AlwaysFalse<void \(\)>': base::Bind{Once,Repeating} require strong ownership: non-owning function references may not bound as the functor due to potential lifetime issues\."]

void WontCompile() {
  [] (FunctionRef<void()> ref) {
    BindRepeating(ref).Run();
  }([] {});
}

#elif defined(NCTEST_BIND_ONCE_TO_BASE_FUNCTION_REF)  // [r"fatal error: static assertion failed due to requirement 'AlwaysFalse<void \(\)>': base::Bind{Once,Repeating} require strong ownership: non-owning function references may not bound as the functor due to potential lifetime issues\."]

void WontCompile() {
  [] (FunctionRef<void()> ref) {
    BindOnce(ref).Run();
  }([] {});
}

#elif defined(NCTEST_BIND_REPEATING_TO_BASE_FUNCTION_REF)  // [r"fatal error: static assertion failed due to requirement 'AlwaysFalse<void \(\)>': base::Bind{Once,Repeating} require strong ownership: non-owning function references may not bound as the functor due to potential lifetime issues\."]

void WontCompile() {
  [] (FunctionRef<void()> ref) {
    BindRepeating(ref).Run();
  }([] {});
}

#endif

}  // namespace base
