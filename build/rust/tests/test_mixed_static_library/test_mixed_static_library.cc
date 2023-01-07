// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_mixed_static_library.h"
#include <iostream>

#include "build/rust/tests/test_mixed_static_library/src/lib.rs.h"

uint32_t cpp_addition(uint32_t a, uint32_t b) {
  return a + b;
}
