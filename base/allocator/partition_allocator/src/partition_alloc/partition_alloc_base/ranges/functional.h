// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_FUNCTIONAL_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_FUNCTIONAL_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace partition_alloc::internal::base::ranges {

// Simplified implementations of C++20's std::ranges comparison function
// objects. As opposed to the std::ranges implementation, these versions do not
// constrain the passed-in types.
//
// Reference: https://wg21.link/range.cmp
using equal_to = std::equal_to<>;
using not_equal_to = std::not_equal_to<>;
using greater = std::greater<>;
using less = std::less<>;
using greater_equal = std::greater_equal<>;
using less_equal = std::less_equal<>;

}  // namespace partition_alloc::internal::base::ranges

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_FUNCTIONAL_H_
