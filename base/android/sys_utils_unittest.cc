// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <unistd.h>

#include <cstdint>

#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(SysUtils, AmountOfPhysicalMemory) {
  // Check that the RAM size reported by sysconf() matches the one
  // computed by base::SysInfo::AmountOfPhysicalMemory().
  int64_t sys_ram_size =
      static_cast<int64_t>(sysconf(_SC_PHYS_PAGES)) * sysconf(_SC_PAGESIZE);
  EXPECT_EQ(sys_ram_size, SysInfo::AmountOfPhysicalMemory().InBytes());
}

}  // namespace android
}  // namespace base
