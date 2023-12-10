// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <utility>

#include "base/test/bind.h"

namespace base {

void BindMutableLambdaForTesting() {
  // `BindLambdaForTesting()` requires mutable lambdas to be non-const rvalues.
  auto l = []() mutable {};
  const auto const_l = []() mutable {};
  BindLambdaForTesting(l);                   // expected-error@*:* {{BindLambdaForTesting() requires non-const rvalue for mutable lambda binding, i.e. base::BindLambdaForTesting(std::move(lambda)).}}
  BindLambdaForTesting(std::move(const_l));  // expected-error@*:* {{BindLambdaForTesting() requires non-const rvalue for mutable lambda binding, i.e. base::BindLambdaForTesting(std::move(lambda)).}}
}

}  // namespace base
