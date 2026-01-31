// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <unistd.h>

#include <cstdint>

#include "base/byte_size.h"
#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(SysUtils, AmountOfTotalPhysicalMemory) {
  // Check that the RAM size reported by sysconf() matches the one
  // computed by base::SysInfo::AmountOfTotalPhysicalMemory().
  // The sysconf() calls should never return negative, but if they do the test
  // will fail instead of crashing since they're stored in ByteSizeDelta.
  const auto sys_ram_size =
      base::ByteSizeDelta(sysconf(_SC_PHYS_PAGES)) * sysconf(_SC_PAGESIZE);
  EXPECT_EQ(sys_ram_size,
            SysInfo::AmountOfTotalPhysicalMemory().AsByteSizeDelta());
}

}  // namespace android
}  // namespace base
