// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/expected_macros.h"

#include "base/types/expected.h"

namespace base {

base::expected<void, int> TernaryInAssignOrReturn() {
  base::expected<int, int> e = base::ok(1);
  int a, b;
  ASSIGN_OR_RETURN((false ? a : b), e);  // expected-error {{Identified possible ternary in `lhs`}}
}

}  // namespace base
