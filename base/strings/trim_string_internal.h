// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_TRIM_STRING_INTERNAL_H_
#define BASE_STRINGS_TRIM_STRING_INTERNAL_H_

#include <algorithm>
#include <string_view>

#include "base/strings/whitespace_constants.h"

namespace base::internal {

template <typename T, typename CharT = typename T::value_type>
constexpr T TrimStringPieceT(T input,
                             T trim_chars,
                             bool trim_leading,
                             bool trim_trailing) {
  size_t begin = trim_leading ? input.find_first_not_of(trim_chars) : 0;
  size_t end =
      trim_trailing ? input.find_last_not_of(trim_chars) + 1 : input.size();
  return input.substr(std::min(begin, input.size()), end - begin);
}

}  // namespace base::internal

#endif  // BASE_STRINGS_TRIM_STRING_INTERNAL_H_
