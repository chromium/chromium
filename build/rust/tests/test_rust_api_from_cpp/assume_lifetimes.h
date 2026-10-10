// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ASSUME_LIFETIMES_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ASSUME_LIFETIMES_H_

// Rust bindings for methods of this struct are generated with Crubit's
// `assume_lifetimes` feature (enabled by default in `rust_api_from_cpp`).
struct AssumeLifetimesStruct final {
  int GetValue() const { return value; }
  void SetValue(int new_value) { value = new_value; }

  int value;
};

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ASSUME_LIFETIMES_H_
