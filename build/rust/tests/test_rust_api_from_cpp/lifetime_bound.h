// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_LIFETIME_BOUND_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_LIFETIME_BOUND_H_

#include "base/compiler_specific.h"

struct LifetimeBoundStruct final {
  // The result may refer to `this` or to `other`.  Lifetime elision would
  // incorrectly tie the result only to `this`.  The `LIFETIME_BOUND`
  // annotations tie the result to both `this` and `other`.
  const int& GetLargerValue(const int& other LIFETIME_BOUND) const
      LIFETIME_BOUND {
    return value > other ? value : other;
  }

  int value;
};

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_LIFETIME_BOUND_H_
