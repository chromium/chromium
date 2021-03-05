// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"

#include "base/check_op.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(WindowsVersion, GetVersionExAndKernelOsVersionMatch) {
  // If this fails, we're running in compatibility mode, or need to update the
  // application manifest.
  // Note: not all versions of Windows return identical build numbers e.g.
  // 1909/19H2 kernel32.dll has build number 18362 but OS version build number
  // 18363.
  EXPECT_EQ(OSInfo::GetInstance()->Kernel32VersionNumber().major,
            OSInfo::GetInstance()->version_number().major);
  EXPECT_EQ(OSInfo::GetInstance()->Kernel32VersionNumber().minor,
            OSInfo::GetInstance()->version_number().minor);
}

TEST(OSInfo, MajorMinorBuildToVersion) {
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 32767),
            Version::WIN10_21H1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 19043),
            Version::WIN10_21H1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 19042),
            Version::WIN10_20H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 19041),
            Version::WIN10_20H1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 18363),
            Version::WIN10_19H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 18362),
            Version::WIN10_19H1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 17763), Version::WIN10_RS5);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 17134), Version::WIN10_RS4);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 16299), Version::WIN10_RS3);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 15063), Version::WIN10_RS2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 14393), Version::WIN10_RS1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 10586), Version::WIN10_TH2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 10240), Version::WIN10);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 0), Version::WIN10);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 3, 0), Version::WIN8_1);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 2, 0), Version::WIN8);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 1, 0), Version::WIN7);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 0, 0), Version::VISTA);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(5, 3, 0), Version::SERVER_2003);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(5, 2, 0), Version::SERVER_2003);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(5, 1, 0), Version::XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(5, 0, 0), Version::PRE_XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(4, 0, 0), Version::PRE_XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(3, 0, 0), Version::PRE_XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(2, 0, 0), Version::PRE_XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(1, 0, 0), Version::PRE_XP);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(0, 0, 0), Version::PRE_XP);

#if !DCHECK_IS_ON()
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(11, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(9, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(8, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(7, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 4, 0), Version::WIN8_1);
#endif  // !DCHECK_IS_ON()
}

}  // namespace win
}  // namespace base
