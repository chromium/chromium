// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/to_vector.h"

#include <tuple>
#include <utility>
#include <vector>

namespace base {

// ToVector() doesn't implicitly deduce initializer lists.
void InitializerList() {
  std::ignore = ToVector({"aaa", "bbb", "ccc"}); // expected-error@*:* {{no matching function for call to 'ToVector'}}
}

// Lambdas operating on rvalue ranges of move-only elements expect lvalue
// references to the element type.
void MoveOnlyProjections() {
  struct MoveOnly {
    MoveOnly() = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
  };

  std::vector<MoveOnly> vec;
  std::ignore = ToVector(std::move(vec), [](MoveOnly arg) {
    return arg;
  }); // expected-error@*:* {{no matching function for call to 'ToVector'}}
  std::ignore = ToVector(std::move(vec), [](MoveOnly&& arg) {
    return std::move(arg);
  }); // expected-error@*:* {{no matching function for call to 'ToVector'}}
}

}  // namespace base
