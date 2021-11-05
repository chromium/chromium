// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_MIXED_SOURCE_SET_TEST_MIXED_SOURCE_SET_H_
#define BUILD_RUST_TESTS_TEST_MIXED_SOURCE_SET_TEST_MIXED_SOURCE_SET_H_

#include <stdint.h>
#include "build/rust/tests/test_mixed_source_set/dependency_header.h"
#include "build/rust/tests/test_mixed_source_set/src/lib.rs.h"

CustomIntType cpp_addition(uint32_t a, uint32_t b);
void cpp_callback();

void say_hello_via_callbacks();

#endif  // BUILD_RUST_TESTS_TEST_MIXED_SOURCE_SET_TEST_MIXED_SOURCE_SET_H_
