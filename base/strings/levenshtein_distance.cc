// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/levenshtein_distance.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

namespace base {

namespace {

template <typename CharT>
size_t LevenshteinDistanceImpl(std::basic_string_view<CharT> a,
                               std::basic_string_view<CharT> b,
                               std::optional<size_t> max_distance) {
  if (a.size() > b.size()) {
    a.swap(b);
  }

  // max(a.size(), b.size()) steps always suffice.
  const size_t k = max_distance.value_or(b.size());
  // If the string's lengths differ by more than `k`, so does their
  // Levenshtein distance.
  if (a.size() + k < b.size()) {
    return k + 1;
  }
  // The classical Levenshtein distance DP defines dp[i][j] as the minimum
  // number of insert, remove and replace operation to convert a[:i] to b[:j].
  // To make this more efficient, one can define dp[i][d] as the distance of
  // a[:i] and b[:i + d]. Intuitively, d represents the delta between j and i in
  // the former dp. Since the Levenshtein distance is restricted by `k`, abs(d)
  // can be bounded by `k`. Since dp[i][d] only depends on values from dp[i-1],
  // it is not necessary to store the entire 2D table. Instead, this code just
  // stores the d-dimension, which represents "the distance with the current
  // prefix of the string, for a given delta d". Since d is between `-k` and
  // `k`, the implementation shifts the d-index by `k`, bringing it in range
  // [0, `2*k`].

  // The algorithm only cares if the Levenshtein distance is at most `k`. Thus,
  // any unreachable states and states in which the distance is certainly larger
  // than `k` can be set to any value larger than `k`, without affecting the
  // result.
  const size_t kInfinity = k + 1;
  std::vector<size_t> dp(2 * k + 1, kInfinity);
  // Initially, `dp[d]` represents the Levenshtein distance of the empty prefix
  // of `a` and the first j = d - k characters of `b`. Their distance is j,
  // since j removals are required. States with negative d are not reachable,
  // since that corresponds to a negative index into `b`.
  std::iota(dp.begin() + static_cast<long>(k), dp.end(), 0);
  for (size_t i = 0; i < a.size(); i++) {
    // Right now, `dp` represents the Levenshtein distance when considering the
    // first `i` characters (up to index `i-1`) of `a`. After the next loop,
    // `dp` will represent the Levenshtein distance when considering the first
    // `i+1` characters.
    for (size_t d = 0; d <= 2 * k; d++) {
      if (i + d < k || i + d >= b.size() + k) {
        // `j = i + d - k` is out of range of `b`. Since j == -1 corresponds to
        // the empty prefix of `b`, the distance is i + 1 in this case.
        dp[d] = i + d + 1 == k ? i + 1 : kInfinity;
        continue;
      }
      const size_t j = i + d - k;
      // If `a[i] == `b[j]` the Levenshtein distance for `d` remained the same.
      if (a[i] != b[j]) {
        // (i, j) -> (i-1, j-1), `d` stays the same.
        const size_t replace = dp[d];
        // (i, j) -> (i-1, j), `d` increases by 1.
        // If the distance between `i` and `j` becomes larger than `k`, their
        // distance is at least `k + 1`. Same in the `insert` case.
        const size_t remove = d != 2 * k ? dp[d + 1] : kInfinity;
        // (i, j) -> (i, j-1), `d` decreases by 1. Since `i` stays the same,
        // this is intentionally using the dp value updated in the previous
        // iteration.
        const size_t insert = d != 0 ? dp[d - 1] : kInfinity;
        dp[d] = 1 + std::min({replace, remove, insert});
      }
    }
  }
  return std::min(dp[b.size() + k - a.size()], k + 1);
}

}  // namespace

size_t LevenshteinDistance(std::string_view a,
                           std::string_view b,
                           std::optional<size_t> max_distance) {
  return LevenshteinDistanceImpl(a, b, max_distance);
}
size_t LevenshteinDistance(std::u16string_view a,
                           std::u16string_view b,
                           std::optional<size_t> max_distance) {
  return LevenshteinDistanceImpl(a, b, max_distance);
}

}  // namespace base
