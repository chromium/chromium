// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_rust_calling_cpp/rust_calling_cpp_rlib.rs.h"

int main() {
  rust_calling_cpp();
  return 0;
}
