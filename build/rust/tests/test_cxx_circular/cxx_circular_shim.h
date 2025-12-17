// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_CXX_CIRCULAR_CXX_CIRCULAR_SHIM_H_
#define BUILD_RUST_TESTS_TEST_CXX_CIRCULAR_CXX_CIRCULAR_SHIM_H_

#include <cstdint>

#include "build/rust/tests/test_cxx_circular/cxx_circular.rs.h"

namespace cxx_circular_test {

int32_t AddRustI32s(const RustI32& n1, const RustI32& n2) {
  return n1.get() + n2.get();
}

}  // namespace cxx_circular_test

#endif  // BUILD_RUST_TESTS_TEST_CXX_CIRCULAR_CXX_CIRCULAR_SHIM_H_
