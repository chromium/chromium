// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_mixed_source_set/test_mixed_source_set.h"
#include "build/rust/tests/test_rust_source_set/src/lib.rs.h"

int main(int argc, char* argv[]) {
  say_hello();
  say_hello_via_callbacks();
  return 0;
}
