// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/bindgen_cpp_test_with_cpp_linkage/cpp.h"

namespace functions {

int normal_fn(int i) {
  return template_fn(i);
}

}  // namespace functions
