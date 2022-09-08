// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_variable_static_library.h"
#include <iostream>

#if defined(RUST_ENABLED)
#include "build/rust/tests/test_variable_static_library/src/lib.rs.h"
#endif

#if !defined(RUST_ENABLED)
struct FooBars {
  size_t foos;
  size_t bars;
};

FooBars do_something_in_sandbox(const std::string& input) {
  std::cout << "Memory safe language not enabled: we would create a sandboxed "
               "utility process for this operation.\n";
  // We're not actually going to do this for the sake of this test/demo code.
  // In reality this would involve a call through Mojo to some service.
  FooBars foobars;
  foobars.foos = 0;
  foobars.bars = 0;
  return foobars;
}
#endif

void do_something_in_sandbox_or_memory_safe_language(const std::string& input) {
#if defined(RUST_ENABLED)
  FooBars foobars = do_something_in_memory_safe_language(input);
#else
  FooBars foobars = do_something_in_sandbox(input);
#endif
  std::cout << "Found " << foobars.foos << " foo[s] and " << foobars.bars
            << " bar[s]s.\n";
}
