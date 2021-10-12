// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_mixed_source_set.h"
#include <stdint.h>
#include <iostream>

extern "C" void rust_code();

void say_hello_via_callbacks() {
  rust_code();
}

extern "C" void cpp_callback() {
  std::cout << "Hello from C++ callback from Rust" << std::endl;
}

extern "C" uint32_t cpp_addition(uint32_t a, uint32_t b) {
  return a + b;
}
