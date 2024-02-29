// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TO_VECTOR_H_
#define BASE_TEST_TO_VECTOR_H_

#include <functional>

#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"

namespace base::test {

// Maps a container to a std::vector<> with respect to the provided projection.
// The deduced vector element type is equal to the projection's return type with
// cv-qualifiers removed.
//
// In C++20 this is roughly equal to:
// auto vec = range | std::views:transform(proj) | std::ranges::to<std::vector>;
//
// Complexity: Exactly `size(range)` applications of `proj`.
//
// TODO(crbug.com/326392658): Replace existing callsites with base::ToVector<>.
template <typename Range, typename Proj = std::identity>
  requires requires { typename base::internal::range_category_t<Range>; }
auto ToVector(Range&& range, Proj proj = {}) {
  return base::ToVector(std::forward<Range>(range), std::move(proj));
}

}  // namespace base::test

#endif  // BASE_TEST_TO_VECTOR_H_
