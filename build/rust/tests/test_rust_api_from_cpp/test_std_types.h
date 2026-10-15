// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_STD_TYPES_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_STD_TYPES_H_

#include <optional>

inline std::optional<int> AddOneIfPresent(std::optional<int> x) {
  if (!x.has_value()) {
    return std::nullopt;
  }
  return *x + 1;
}

struct StructWithOptionalField {
  std::optional<int> value;
};

inline StructWithOptionalField CreateStructWithOptionalField(int value) {
  return StructWithOptionalField{value};
}

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_TEST_STD_TYPES_H_
