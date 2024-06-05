// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/enum_set.h"

namespace base {
namespace {

size_t LargeSparseEnum() {
  enum class TestEnumSparse {
    kTest1 = 1,
    kTestMin = 1,
    kTest50 = 50,
    kTest100 = 100,
    kTestMax = kTest100,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;

  // TestEnumSparseSet::All() does not compile as constexpr because there are
  // more than 64 possible values.
  constexpr auto set = TestEnumSparseSet::All();  // expected-error {{constexpr variable 'set' must be initialized by a constant expression}}
  return set.size();
}

}  // namespace
}  // namespace base
