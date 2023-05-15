// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_SUPPORTS_OSTREAM_OPERATOR_H_
#define BASE_TYPES_SUPPORTS_OSTREAM_OPERATOR_H_

#include <ostream>
#include <type_traits>
#include <utility>

namespace base::internal {

// Uses expression SFINAE to detect whether using operator<< would work.
//
// Note that the above #include of <ostream> is necessary to guarantee
// consistent results here for basic types.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

}  // namespace base::internal

#endif  // BASE_TYPES_SUPPORTS_OSTREAM_OPERATOR_H_
