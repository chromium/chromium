// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_rust_static_library/src/lib.rs.h"

int main(int argc, char* argv[]) {
  say_hello();
  add_two_ints_via_rust(3, 4);
  return 0;
}
