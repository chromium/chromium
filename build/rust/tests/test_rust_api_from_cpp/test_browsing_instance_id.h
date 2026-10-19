// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_BROWSING_INSTANCE_ID_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_BROWSING_INSTANCE_ID_H_

#include <cstdint>

#include "base/types/id_type.h"

// Type alias for a class template specialization.  Mimics
// `content::BrowsingInstanceId`.
using TestBrowsingInstanceId = base::IdType32<class TestBrowsingInstanceIdTag>;

inline TestBrowsingInstanceId CreateTestBrowsingInstanceId(int32_t value) {
  return TestBrowsingInstanceId(value);
}

inline int32_t GetTestBrowsingInstanceIdValue(TestBrowsingInstanceId id) {
  return id.value();
}

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_BROWSING_INSTANCE_ID_H_
