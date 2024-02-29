// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/compiler_specific.h"
#include "build/config/clang/unsafe_buffers_buildflags.h"

namespace base {

UNSAFE_BUFFER_USAGE int uses_pointer_as_array(int* i) {
  return UNSAFE_BUFFERS(i[1]);
}

void CallToUnsafeBufferFunctionDisallowed() {
  int arr[] = {1, 2};
#if BUILDFLAG(UNSAFE_BUFFERS_WARNING_ENABLED)
  uses_pointer_as_array(arr);  // expected-error {{function introduces unsafe buffer manipulation}}
#else
  uses_pointer_as_array(arr);  // expected-no-diagnostics: No error when not enabled.
#endif
}

}  // namespace base
