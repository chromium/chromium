// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_RANGES_H_
#define BASE_RANGES_RANGES_H_

#include <ranges>

namespace base::ranges {

template <typename Range>
using iterator_t = std::ranges::iterator_t<Range>;
template <typename Range>
using range_value_t = std::ranges::range_value_t<Range>;

}  // namespace base::ranges

#endif  // BASE_RANGES_RANGES_H_
