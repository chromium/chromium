// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/bindgen_static_fns_test/lib.h"

#include <stdint.h>

COMPONENT_EXPORT uint32_t mul_two_numbers(uint32_t a, uint32_t b) {
  return a * b;
}
