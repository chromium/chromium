// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <unistd.h>

#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(SysUtils, AmountOfPhysicalMemory) {
  // Check that the RAM size reported by sysconf() matches the one
  // computed by base::SysInfo::AmountOfPhysicalMemory().
  size_t sys_ram_size =
      static_cast<size_t>(sysconf(_SC_PHYS_PAGES) * PAGE_SIZE);
  EXPECT_EQ(sys_ram_size,
            static_cast<size_t>(SysInfo::AmountOfPhysicalMemory()));
}

}  // namespace android
}  // namespace base
