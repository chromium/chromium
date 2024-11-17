// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/adapters.h"

#include <vector>

namespace base {

void DanglingInRangeBasedForLoop() {
  // This is not safe prior to C++23, since the temporary vector does not have
  // its lifetime extended.
  for (int& x : Reversed(std::vector({1, 2, 3}))) {  // expected-error {{temporary implicitly bound to local reference will be destroyed at the end of the full-expression}}
    x *= x;
  }
}

}  // namespace base
