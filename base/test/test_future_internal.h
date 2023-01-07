// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_FUTURE_INTERNAL_H_
#define BASE_TEST_TEST_FUTURE_INTERNAL_H_

namespace base {
namespace test {

namespace internal {

// Helper to only implement a method if the future holds a single value
template <typename Tuple>
using EnableIfSingleValue =
    std::enable_if_t<(std::tuple_size<Tuple>::value <= 1), bool>;

// Helper to only implement a method if the future holds multiple values
template <typename Tuple>
using EnableIfMultiValue =
    std::enable_if_t<(std::tuple_size<Tuple>::value > 1), bool>;

}  // namespace internal

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_TEST_FUTURE_INTERNAL_H_
