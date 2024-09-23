// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/posix/sysctl.h"

#include <sys/sysctl.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using SysctlTest = testing::Test;

TEST(SysctlTest, MibSuccess) {
  std::optional<std::string> result1 = StringSysctl({CTL_HW, HW_MACHINE});
  EXPECT_TRUE(result1);

#if !BUILDFLAG(IS_OPENBSD)
  std::optional<std::string> result2 = StringSysctlByName("hw.machine");
  EXPECT_TRUE(result2);

  EXPECT_EQ(result1, result2);
#endif
}

TEST(SysctlTest, MibFailure) {
  std::optional<std::string> result = StringSysctl({-1});
  EXPECT_FALSE(result);

#if !BUILDFLAG(IS_OPENBSD)
  result = StringSysctlByName("banananananananana");
  EXPECT_FALSE(result);
#endif
}

}  // namespace

}  // namespace base
