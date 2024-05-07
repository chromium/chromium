// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_rust_calling_cpp/cpp_library.h"

#include <cstdint>

int32_t mul_by_2_in_cpp_library(int32_t a) {
  return a * 2;
}
