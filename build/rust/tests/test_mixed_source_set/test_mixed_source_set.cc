// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_mixed_source_set.h"
#include <iostream>

#include "build/rust/tests/test_mixed_source_set/src/lib.rs.h"

void say_hello_via_callbacks() {
  say_hello_from_a_cpp_callback_from_rust();
}

void cpp_callback() {
  std::cout << "Hello from C++ callback from Rust" << std::endl;
}

uint32_t cpp_addition(uint32_t a, uint32_t b) {
  return a + b;
}
