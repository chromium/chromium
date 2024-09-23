// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <int A = 0, int B = 0>
UNSAFE_BUFFER_USAGE int uses_pointer_as_array(int* i) {
  return UNSAFE_BUFFERS(i[1]);
}

TEST(UnsafeBuffers, Macro) {
  int arr[] = {1, 2};

  // Should compile even with -Wunsafe-buffer-usage.
  int x = UNSAFE_BUFFERS(uses_pointer_as_array(arr));
  EXPECT_EQ(x, 2);

  // Should compile even with -Wunsafe-buffer-usage.
  UNSAFE_BUFFERS({
    uses_pointer_as_array(arr);
    uses_pointer_as_array(arr);
  });

  // Commas don't break things. This comma is not wrapped in `()` which verifies
  // the macro handles the comma correctly. `()` would hide the comma from the
  // macro.
  int y = UNSAFE_BUFFERS(uses_pointer_as_array<1, 1>(arr));
  EXPECT_EQ(y, 2);
}

}  // namespace
