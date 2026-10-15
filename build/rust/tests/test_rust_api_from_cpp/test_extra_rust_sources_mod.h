// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_EXTRA_RUST_SOURCES_MOD_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_EXTRA_RUST_SOURCES_MOD_H_

namespace test_outer::test_inner {

// Small test of `extra_rust_sources_mod` of `rust_api_from_cpp`.
struct TestMeters {
  int value;
};

}  // namespace test_outer::test_inner

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_EXTRA_RUST_SOURCES_MOD_H_
