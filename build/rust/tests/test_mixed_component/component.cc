// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_mixed_component/component.h"
#if defined(RUST_ENABLED)
#include "build/rust/tests/test_mixed_component/component.rs.h"
#endif

COMPONENT_EXPORT uint32_t bilingual_math(uint32_t a, uint32_t b) {
#if defined(RUST_ENABLED)
  return rust_math(a, b);
#else
  return a + b;
#endif
}
