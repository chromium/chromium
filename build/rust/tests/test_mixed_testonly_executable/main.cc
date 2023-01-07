// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#if defined(RUST_ENABLED)
#include "build/rust/tests/test_mixed_testonly_executable/src/lib.rs.h"
#endif

int main(int argc, const char* argv[]) {
#if defined(RUST_ENABLED)
  print_message_from_rust();
#else
  std::cout << "Here is a message from C++.\n";
#endif
  return 0;
}
