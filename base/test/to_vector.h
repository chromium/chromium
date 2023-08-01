// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TO_VECTOR_H_
#define BASE_TEST_TO_VECTOR_H_

#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/identity.h"
#include "base/ranges/algorithm.h"
#include "base/template_util.h"

namespace base::test {

// A handy helper for mapping any container into an std::vector<> with respect
// to the provided projection. The deduced vector element type is equal to the
// projection's return type with cv-qualifiers removed.
//
// In C++20 this is roughly equal to:
// auto vec = range | views:transform(proj) | ranges::to<std::vector>;
//
// Complexity: Exactly `size(range)` applications of `proj`.
template <typename Range, typename Proj = identity>
auto ToVector(Range&& range, Proj proj = {}) {
  using ProjectedType =
      std::invoke_result_t<Proj, decltype(*std::begin(range))>;
  std::vector<base::remove_cvref_t<ProjectedType>> container;
  container.reserve(std::size(range));
  base::ranges::transform(std::forward<Range>(range),
                          std::back_inserter(container), std::move(proj));
  return container;
}

}  // namespace base::test

#endif  // BASE_TEST_TO_VECTOR_H_
