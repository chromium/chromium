// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXPLICIT_LIFETIMES_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXPLICIT_LIFETIMES_H_

#include "third_party/crubit/support/lifetime_annotations.h"

struct ExplicitLifetimesStruct final {
  // The result may refer to `this` or to `other`.  Lifetime elision would
  // incorrectly tie the result only to `this`.  The explicit `$a` annotations
  // tie the result to both `this` and `other`.
  const int& $a GetLargerValue(const int& $a other) const $a {
    return value > other ? value : other;
  }

  int value;
};

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXPLICIT_LIFETIMES_H_
