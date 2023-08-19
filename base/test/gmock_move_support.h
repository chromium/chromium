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

// Moves the `I`th argument to `*out` and returns `return_value`.
// This addresses that `DoAll(MoveArg(), Return())` does not work for moveable
// types passed by value, since the first action only receives a read-only view
// of its arguments.
//
// Example:
//
// using MoveOnly = std::unique<int>;
// struct MockFoo {
//   MOCK_METHOD(bool, Fun, (MoveOnly), ());
// };
//
// MoveOnly result;
// MockFoo foo;
// EXPECT_CALL(foo, Fun).WillOnce(MoveArgAndReturn(&result, true));
// EXPECT_TRUE(foo.Fun(std::make_unique<int>(123)));
// EXPECT_THAT(result, testing::Pointee(123));
template <size_t I = 0, typename T1, typename T2>
auto MoveArgAndReturn(T1* out, T2&& return_value) {
  return [out, value = std::forward<T2>(return_value)](auto&&... args) mutable {
    *out = std::move(std::get<I>(std::tie(args...)));
    return std::forward<T2>(value);
  };
}

#endif  // BASE_TEST_GMOCK_MOVE_SUPPORT_H_
