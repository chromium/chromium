// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXTRA_RUST_SOURCES_TIME_DELTA_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXTRA_RUST_SOURCES_TIME_DELTA_H_

#include <stdint.h>

namespace test_base {

// Mini-replica of `base::TimeDelta`, meant for a small test of Crubit (of
// `extra_rust_sources` of `rust_api_from_cpp` in particular).
class TestTimeDelta {
 public:
  static constexpr TestTimeDelta FromMicroseconds(int64_t us) {
    return TestTimeDelta(us);
  }

  constexpr int64_t InMicroseconds() const { return delta_; }

 private:
  constexpr explicit TestTimeDelta(int64_t delta_us) : delta_(delta_us) {}

  // Delta in microseconds.  Can be negative.
  int64_t delta_;
};

}  // namespace test_base

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_EXTRA_RUST_SOURCES_TIME_DELTA_H_
