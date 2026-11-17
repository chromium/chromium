// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_MUTUAL_RECURSION_PARITY_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_MUTUAL_RECURSION_PARITY_H_

namespace parity {

// Returns true if `n` is even.  Requires `n >= 0`.
//
// Implemented in C++ (parity.cc), by calling `IsOdd`.
bool IsEven(int n);

// Returns true if `n` is odd.  Requires `n >= 0`.
//
// Implemented in C++ (parity.cc), by forwarding to the Rust function
// `is_odd::is_odd` (is_odd.rs), which calls `IsEven`.
bool IsOdd(int n);

}  // namespace parity

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_MUTUAL_RECURSION_PARITY_H_
