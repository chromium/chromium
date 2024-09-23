// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/to_value_list.h"

#include <tuple>
#include <utility>
#include <vector>

namespace base {

// ToValueList() doesn't implicitly deduce initializer lists.
void InitializerList() {
  std::ignore = ToValueList({"aaa", "bbb", "ccc"}); // expected-error@*:* {{no matching function for call to 'ToValueList'}}
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
  std::ignore = ToValueList(std::move(vec), [](MoveOnly arg) {
    return arg;
  }); // expected-error@*:* {{no matching function for call to 'ToValueList'}}
  std::ignore = ToValueList(std::move(vec), [](MoveOnly&& arg) {
    return std::move(arg);
  }); // expected-error@*:* {{no matching function for call to 'ToValueList'}}
}

// Return type of the projection must be compatible with Value::List::Append().
void AppendableToList() {
  std::vector<int> vec;
  std::ignore = ToValueList(vec, [](int) -> int* { return nullptr; }); // expected-error@*:* {{no matching function for call to 'ToValueList'}}
}

}  // namespace base
