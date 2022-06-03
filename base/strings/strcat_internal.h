// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRCAT_INTERNAL_H_
#define BASE_STRINGS_STRCAT_INTERNAL_H_

#include <string>

#include "base/containers/span.h"
#include "base/template_util.h"

namespace base {

namespace internal {

// Optimized version of `std::basic_string::resize()` that skips zero
// initialization of appended characters. Reading from the newly allocated
// characters results in undefined behavior if they are not explicitly
// initialized afterwards. Currently proposed for standardization as
// std::basic_string::resize_and_overwrite: https://wg21.link/P1072R6
template <typename CharT>
auto Resize(std::basic_string<CharT>& str, size_t total_size, priority_tag<1>)
    -> decltype(str.__resize_default_init(total_size)) {
  str.__resize_default_init(total_size);
}

// Fallback to regular std::basic_string::resize() if invoking
// __resize_default_init is ill-formed.
template <typename CharT>
void Resize(std::basic_string<CharT>& str, size_t total_size, priority_tag<0>) {
  str.resize(total_size);
}

// Appends `pieces` to `dest`. Instead of simply calling `dest.append()`
// `pieces.size()` times, this method first resizes `dest` to be of the desired
// size, and then appends each piece via `std::char_traits::copy`. This achieves
// two goals:
// 1) Allocating the desired size all at once avoids other allocations that
//    could happen if intermediate allocations did not reserve enough capacity.
// 2) Invoking std::char_traits::copy instead of std::basic_string::append
//    avoids having to write the terminating '\0' character n times.
template <typename CharT, typename StringT>
void StrAppendT(std::basic_string<CharT>& dest, span<const StringT> pieces) {
  const size_t initial_size = dest.size();
  size_t total_size = initial_size;
  for (const auto& cur : pieces)
    total_size += cur.size();

  // Note: As opposed to `reserve()` calling `resize()` with an argument smaller
  // than the current `capacity()` does not result in the string releasing spare
  // capacity. Furthermore, common std::string implementations apply a geometric
  // growth strategy if the current capacity is not sufficient for the newly
  // added characters. Since this codepath is also triggered by `resize()`, we
  // don't have to manage the std::string's capacity ourselves here to avoid
  // performance hits in case `StrAppend()` gets called in a loop.
  Resize(dest, total_size, priority_tag<1>());
  CharT* dest_char = &dest[initial_size];
  for (const auto& cur : pieces) {
    std::char_traits<CharT>::copy(dest_char, cur.data(), cur.size());
    dest_char += cur.size();
  }
}

template <typename StringT>
auto StrCatT(span<const StringT> pieces) {
  std::basic_string<typename StringT::value_type> result;
  StrAppendT(result, pieces);
  return result;
}

}  // namespace internal

}  // namespace base

#endif  // BASE_STRINGS_STRCAT_INTERNAL_H_
