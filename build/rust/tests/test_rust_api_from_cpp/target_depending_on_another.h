// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TARGET_DEPENDING_ON_ANOTHER_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TARGET_DEPENDING_ON_ANOTHER_H_

#include "build/rust/tests/test_rust_api_from_cpp/self_contained_target_header2.h"

inline CcPodStruct CreateCcPodStructFromValue(int x) {
  return CcPodStruct{.value = x};
}

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TARGET_DEPENDING_ON_ANOTHER_H_
