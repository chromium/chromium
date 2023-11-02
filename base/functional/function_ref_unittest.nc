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

#if defined(NCTEST_NO_IMPLICIT_VOIDIFY)  // [r"note: candidate template ignored: requirement '[^']+' was not satisfied \[with Functor = \(lambda at [^)]+\)\]"]

void WontCompile() {
  auto returns_int = [] () { return 42; };
  FunctionRef<void()> ref(returns_int);
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
