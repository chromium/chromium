// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_JOIN_STRING_INTERNAL_H_
#define BASE_STRINGS_JOIN_STRING_INTERNAL_H_

#include <algorithm>
#include <string>

#include "base/check_op.h"

namespace base::strings_internal {

// Generic version for all JoinString overloads. |list_type| must be a sequence
// (base::span or std::initializer_list) of strings/StringPieces (std::string,
// std::u16string, std::string_view or std::u16string_view). |CharT| is either
// char or char16_t.
//
// |LazyInstantiation| is explicitly passed by the callers in string_util.h,
// which are templates on it: that makes their calls dependent, so that this
// function is only instantiated in files that call JoinString(). (Clang
// instantiates constexpr functions as soon as something references them.)
template <typename LazyInstantiation,
          typename list_type,
          typename T,
          typename CharT = typename T::value_type>
constexpr std::basic_string<CharT> JoinStringT(list_type parts, T sep) {
  if (std::empty(parts)) {
    return std::basic_string<CharT>();
  }

  // Pre-allocate the eventual size of the string. Start with the size of all of
  // the separators (note that this *assumes* parts.size() > 0).
  // The counter is initialized with separator sizes.
  const size_t total_size = std::ranges::fold_left(
      parts, (std::size(parts) - 1) * sep.size(),
      [](size_t acc, const auto& part) { return acc + std::size(part); });
  std::basic_string<CharT> result;
  result.reserve(total_size);

  bool first = true;
  for (const auto& part : parts) {
    if (!first) {
      result.append(sep);
    }
    result.append(part);
    first = false;
  }

  // Sanity-check that we pre-allocated correctly.
  DCHECK_EQ(total_size, result.size());

  return result;
}

}  // namespace base::strings_internal

#endif  // BASE_STRINGS_JOIN_STRING_INTERNAL_H_
