// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "partition_alloc/bounds_checks.h"

namespace partition_alloc {

bool FailToCompileWithMismatchedSignedness() {
  const int* ptr = nullptr;
  const int signed_index = 0;
  return IsExtentInBounds(ptr, signed_index);  // expected-error@*:* {{The source type is out of range for the destination type.}}
}

}  // namespace partition_alloc
