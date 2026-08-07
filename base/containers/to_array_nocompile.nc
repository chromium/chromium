// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/to_array.h"

#include <tuple>
#include <vector>

#include "base/containers/span.h"

namespace base {

void DynamicSpanWithoutExplicitSize() {
  std::vector<int> vec = {1, 2, 3};
  base::span<const int> dynamic_span(vec);
  // Attempting to convert dynamic span without specifying template size N must fail.
  std::ignore = ToArray(dynamic_span);  // expected-error@*:* {{no matching function for call to 'ToArray'}}
}

void MismatchedFixedExtent() {
  const int kArray[3] = {1, 2, 3};
  base::span<const int, 3> fixed_span(kArray);
  // Specifying a size N that does not match fixed extent 3 must fail.
  std::ignore = ToArray<4>(fixed_span);  // expected-error@*:* {{no matching function for call to 'ToArray'}}
}

void NonSizedRangeFails() {
  auto non_sized_view = std::views::iota(0) | std::views::filter([](int x) { return x % 2 == 0; });
  // Attempting to convert a non-sized range to std::array<int, 5> must fail.
  std::ignore = ToArray<5>(non_sized_view);  // expected-error@*:* {{no matching function for call to 'ToArray'}}
}

}  // namespace base

