// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <intrin.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {
void* return_address;

__attribute__((noinline)) void Bug() {
  void* pvAddressOfReturnAddress = _AddressOfReturnAddress();
  if (!return_address)
    return_address = *(void**)pvAddressOfReturnAddress;
  else
    *(void**)pvAddressOfReturnAddress = return_address;
}

__attribute__((noinline)) void A() {
  Bug();
}

__attribute__((noinline)) void B() {
  Bug();
}

TEST(CET, ShadowStack) {
  // TODO(ajgo): Check that it's enabled by OS.
  A();
  EXPECT_DEATH(B(), "");
}
}  // namespace
}  // namespace win
}  // namespace base
