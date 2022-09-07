// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_GMOCK_MOVE_SUPPORT_H_
#define BASE_TEST_GMOCK_MOVE_SUPPORT_H_

#include <cstddef>
#include <tuple>
#include <utility>

// A similar action as testing::SaveArg, but it does an assignment with
// std::move() instead of always performing a copy.
template <size_t I = 0, typename T>
auto MoveArg(T* out) {
  return [out](auto&&... args) {
    *out = std::move(std::get<I>(std::tie(args...)));
  };
}

#endif  // BASE_TEST_GMOCK_MOVE_SUPPORT_H_
