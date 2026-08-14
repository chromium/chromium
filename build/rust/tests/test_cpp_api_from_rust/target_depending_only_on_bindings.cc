// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

// This file is used to verify that the C++ bindings can be `#include`d
// without manually depending on `//build/rust/crubit`. See also
// https://crbug.com/546044516
#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"

namespace target_depending_only_on_bindings {

// We don't need to call anything, just including the header is enough to
// trigger the compilation error if include paths are missing (i.e. if
// `include_dirs` from `//build/rust/crubit/BUILD.gn` are not propagated).
// But, for completeness, we call one of the included APIs to double-check
// that the `#include` above actually works.
void SmokeTestThatIncludedBindingsArePresent() {
  std::ignore = rust_lib::mul_two_ints_via_rust(3, 4);
}

}  // namespace target_depending_only_on_bindings
