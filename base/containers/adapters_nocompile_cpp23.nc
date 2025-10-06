// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/adapters.h"
#include "base/containers/span.h"

#include <utility>
#include <vector>

namespace base {

void RangeAsRvaluesRequiresNonBorrowedRange() {
  std::vector<int> v;
  RangeAsRvalues(v);  // expected-error {{no matching function for call to 'RangeAsRvalues'}}
}

void RangeAsRvaluesRequiresMutableRange() {
  // A non-mutable range can't be moved from.
  const std::vector<int> v;
  RangeAsRvalues(std::move(v));  // expected-error {{no matching function for call to 'RangeAsRvalues'}}
}

}  // namespace base
