// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_ROOT_NAMESPACES_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_ROOT_NAMESPACES_H_

namespace test_outer::test_inner {

// Small test of `root_namespaces` of `rust_api_from_cpp`.
inline int GetValueFromNestedNamespace() {
  return 123;
}

}  // namespace test_outer::test_inner

namespace test_other {

inline int GetValueFromOtherNamespace() {
  return 456;
}

}  // namespace test_other

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_ROOT_NAMESPACES_H_
