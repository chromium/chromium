// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides minimal implementation of STL algorithms. e.g.
// std::max() so various PartitionAlloc headers can calculate constexpr values
// without using <algorithm>, as that can potentially cause <algorithm> to be
// transitively included in every translation unit that includes raw_ptr.h.
// As such, this file deliberately have very simple implementations and does not
// even try to include headers like <concepts>.

#ifndef PARTITION_ALLOC_SIMPLE_ALGORITHMS_H_
#define PARTITION_ALLOC_SIMPLE_ALGORITHMS_H_

#include <cstddef>

namespace partition_alloc::internal {

consteval size_t IntegralMax(size_t a, size_t b) {
  return (a < b) ? b : a;
}

template <typename T, typename U>
void IntegralMax(T, U) = delete;

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_SIMPLE_ALGORITHMS_H_
