// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/no_destructor.h"

#include <string>

namespace base {

#if defined(NCTEST_NODESTRUCTOR_REQUIRES_NONTRIVIAL_DESTRUCTOR) // [r"static assertion failed due to requirement '!std::is_trivially_destructible_v<bool>'"]

// Attempt to make a NoDestructor for a type with a trivial destructor.
void WontCompile() {
  NoDestructor<bool> nd;
}

#endif

}  // namespace base
