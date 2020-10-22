// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <intrin.h>

#include "base/compiler_specific.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

bool IsHardwareEnforcedShadowStacksEnabled() {
  // Only supported post Win 10 2004.
  if (base::win::GetVersion() < base::win::Version::WIN10_20H1)
    return false;

  auto get_process_mitigation_policy =
      reinterpret_cast<decltype(&GetProcessMitigationPolicy)>(::GetProcAddress(
          ::GetModuleHandleA("kernel32.dll"), "GetProcessMitigationPolicy"));

  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY uss_policy;
  if (!get_process_mitigation_policy(GetCurrentProcess(),
                                     ProcessUserShadowStackPolicy, &uss_policy,
                                     sizeof(uss_policy))) {
    return false;
  }

  if (uss_policy.EnableUserShadowStack)
    return true;
  else
    return false;
}

void* return_address;

// Bug() simulates a ROP. The first time we are called we save the
// address we will return to and return to it (like a normal function
// call). The second time we return to the saved address. If called
// from a different function the second time, this redirects control
// flow and should be different from the return address in the shadow
// stack.
NOINLINE void Bug() {
  void* pvAddressOfReturnAddress = _AddressOfReturnAddress();
  if (!return_address)
    return_address = *(void**)pvAddressOfReturnAddress;
  else
    *(void**)pvAddressOfReturnAddress = return_address;
}

NOINLINE void A() {
  Bug();
}

NOINLINE void B() {
  Bug();
}

}  // namespace

TEST(CET, ShadowStack) {
  if (IsHardwareEnforcedShadowStacksEnabled()) {
    A();
    EXPECT_DEATH(B(), "");
  }
}

}  // namespace win
}  // namespace base
