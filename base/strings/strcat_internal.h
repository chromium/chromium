// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRCAT_INTERNAL_H_
#define BASE_STRINGS_STRCAT_INTERNAL_H_

#include <ranges>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

namespace base::internal {

// Appends `strings` to `dest`. Instead of simply calling `dest.append()`
// `strings.size()` times, this method first resizes `dest` to be of the desired
// size, and then appends each string via `std::ranges::copy`. This achieves
// two goals:
// 1) Allocating the desired size all at once avoids other allocations that
//    could happen if intermediate allocations did not reserve enough capacity.
// 2) Invoking std::ranges::copy instead of std::basic_string::append
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
    auto write_it = to_overwrite.begin();

    // The first `initial_size` characters are guaranteed to be the previous
    // contents of `dest`.
    write_it += base::checked_cast<ptrdiff_t>(initial_size);

    // Copy each string into the destination, resetting `write_it` as we go.
    for (const StringT& str : strings) {
      write_it = std::ranges::copy(str, write_it).out;
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
