// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_ALGORITHM_H_
#define BASE_RANGES_ALGORITHM_H_

#include <algorithm>

// TODO(crbug.com/40240443): Remove this and use std::ranges:: directly.

namespace base::ranges {

using std::ranges::adjacent_find;
using std::ranges::all_of;
using std::ranges::any_of;
using std::ranges::binary_search;
using std::ranges::clamp;
using std::ranges::copy;
using std::ranges::copy_backward;
using std::ranges::copy_if;
using std::ranges::copy_n;
using std::ranges::count;
using std::ranges::count_if;
using std::ranges::equal;
using std::ranges::equal_range;
using std::ranges::fill;
using std::ranges::fill_n;
using std::ranges::find;
using std::ranges::find_end;
using std::ranges::find_first_of;
using std::ranges::find_if;
using std::ranges::find_if_not;
using std::ranges::for_each;
using std::ranges::for_each_n;
using std::ranges::generate;
using std::ranges::generate_n;
using std::ranges::includes;
using std::ranges::inplace_merge;
using std::ranges::is_heap;
using std::ranges::is_heap_until;
using std::ranges::is_partitioned;
using std::ranges::is_permutation;
using std::ranges::is_sorted;
using std::ranges::is_sorted_until;
using std::ranges::lexicographical_compare;
using std::ranges::lower_bound;
using std::ranges::make_heap;
using std::ranges::max;
using std::ranges::max_element;
using std::ranges::merge;
using std::ranges::min;
using std::ranges::min_element;
using std::ranges::minmax;
using std::ranges::minmax_element;
using std::ranges::mismatch;
using std::ranges::move;
using std::ranges::move_backward;
using std::ranges::next_permutation;
using std::ranges::none_of;
using std::ranges::nth_element;
using std::ranges::partial_sort;
using std::ranges::partial_sort_copy;
using std::ranges::partition;
using std::ranges::partition_copy;
using std::ranges::partition_point;
using std::ranges::pop_heap;
using std::ranges::prev_permutation;
using std::ranges::push_heap;
using std::ranges::remove;
using std::ranges::remove_copy;
using std::ranges::remove_copy_if;
using std::ranges::remove_if;
using std::ranges::replace;
using std::ranges::replace_copy;
using std::ranges::replace_copy_if;
using std::ranges::replace_if;
using std::ranges::reverse;
using std::ranges::reverse_copy;
using std::ranges::rotate;
using std::ranges::rotate_copy;
using std::ranges::search;
using std::ranges::search_n;
using std::ranges::set_difference;
using std::ranges::set_intersection;
using std::ranges::set_symmetric_difference;
using std::ranges::set_union;
using std::ranges::shuffle;
using std::ranges::sort;
using std::ranges::sort_heap;
using std::ranges::stable_partition;
using std::ranges::stable_sort;
using std::ranges::swap_ranges;
using std::ranges::transform;
using std::ranges::unique;
using std::ranges::unique_copy;
using std::ranges::upper_bound;

}  // namespace base::ranges

#endif  // BASE_RANGES_ALGORITHM_H_
