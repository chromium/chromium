// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_STD_STD_TYPES_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_STD_STD_TYPES_H_

#include <memory>
#include <optional>
#include <vector>

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

inline std::unique_ptr<int> CreateUniquePtr(int value) {
  return std::make_unique<int>(value);
}

inline int GetValueFromUniquePtr(std::unique_ptr<int> ptr) {
  return *ptr;
}

inline std::vector<int> CreateVector(int a, int b, int c) {
  return {a, b, c};
}

inline int SumVector(std::vector<int> v) {
  int sum = 0;
  for (int x : v) {
    sum += x;
  }
  return sum;
}

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_STD_STD_TYPES_H_
