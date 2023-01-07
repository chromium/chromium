// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_MIXED_SHARED_LIBRARY_TEST_MIXED_SHARED_LIBRARY_H_
#define BUILD_RUST_TESTS_TEST_MIXED_SHARED_LIBRARY_TEST_MIXED_SHARED_LIBRARY_H_

#include <stdint.h>
#include "build/rust/tests/test_mixed_shared_library/dependency_header.h"
#include "build/rust/tests/test_mixed_shared_library/src/lib.rs.h"

CustomIntType cpp_addition(uint32_t a, uint32_t b);

#endif  // BUILD_RUST_TESTS_TEST_MIXED_SHARED_LIBRARY_TEST_MIXED_SHARED_LIBRARY_H_
