// Copyright 2021 The Chromium Authors
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

COMPONENT_EXPORT std::string bilingual_string() {
#if defined(RUST_ENABLED)
  return std::string(rust_get_an_uppercase_string());
#else
  return "sad panda, no Rust";
#endif
}

#if defined(RUST_ENABLED)
rust::String get_a_string_from_cpp() {
  return rust::String("Mixed Case String");
}
#endif
