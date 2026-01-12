// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRCAT_INTERNAL_H_
#define BASE_STRINGS_STRCAT_INTERNAL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace base::internal {

// Trims the first `n` elements of `span`.
template <typename T>
void RemovePrefix(base::span<T>& span, size_t n) {
  span = span.subspan(n);
}

// Appends `strings` to `dest`. Instead of simply calling `dest.append()`
// `strings.size()` times, this method first resizes `dest` to be of the desired
// size, and then appends each string via `std::ranges::copy`. This achieves
// two goals:
// 1) Allocating the desired size all at once avoids other allocations that
//    could happen if intermediate allocations did not reserve enough capacity.
// 2) Invoking base::span::copy_prefix_from instead of std::basic_string::append
//    avoids having to write the terminating '\0' character n times.
template <typename CharT, typename StringT>
void StrAppendT(std::basic_string<CharT>& dest, span<const StringT> strings) {
  const size_t initial_size = dest.size();
  size_t total_size = initial_size;
  for (const StringT& str : strings) {
    total_size += str.size();
  }

  dest.resize_and_overwrite(total_size, [&](CharT* p, size_t n) {
    // SAFETY: `std::basic_string::resize_and_overwrite` guarantees that the
    // range `[p, p + n]` is valid.
    UNSAFE_BUFFERS(base::span to_overwrite(p, n));

    // The first `initial_size` characters are guaranteed to be the previous
    // contents of `dest`.
    RemovePrefix(to_overwrite, initial_size);

    // Copy each string into the destination, trimming the written prefix after
    // every iteration.
    for (const StringT& str : strings) {
      to_overwrite.copy_prefix_from(str);
      RemovePrefix(to_overwrite, str.size());
    }
    return n;
  });
}

template <typename StringT>
auto StrCatT(span<const StringT> pieces) {
  std::basic_string<typename StringT::value_type> result;
  StrAppendT(result, pieces);
  return result;
}

}  // namespace base::internal

#endif  // BASE_STRINGS_STRCAT_INTERNAL_H_
