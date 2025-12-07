// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/windows_version.h"

#include "base/check_op.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(WindowsVersion, GetVersionExAndKernelOsVersionMatch) {
  // If this fails, we're running in compatibility mode, or need to update the
  // application manifest.
  // Note: not all versions of Windows return identical build numbers e.g.
  // 1909/19H2 kernel32.dll has build number 18362 but OS version build number
  // 18363.
  EXPECT_EQ(OSInfo::Kernel32VersionNumber().major,
            OSInfo::GetInstance()->version_number().major);
  EXPECT_EQ(OSInfo::Kernel32VersionNumber().minor,
            OSInfo::GetInstance()->version_number().minor);
}

TEST(WindowsVersion, CheckDbgHelpVersion) {
  // Make sure that dbghelp.dll is present and is a recent enough version to
  // handle large-page PDBs. This requires dbghelp.dll from the Windows 11 SDK
  // or later.
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  FilePath dbghelp_path = exe_dir.Append(FILE_PATH_LITERAL("dbghelp.dll"));
  ASSERT_TRUE(base::PathExists(dbghelp_path));
  auto file_version =
      FileVersionInfoWin::CreateFileVersionInfoWin(dbghelp_path);
  ASSERT_TRUE(file_version);
  auto version = file_version->GetFileVersion();
  // Check against Windows 11 SDK version.
  EXPECT_GE(version, base::Version({10, 0, 22621, 755}));
}

TEST(OSInfo, MajorMinorBuildToVersion) {
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(11, 0, 0), Version::WIN11);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 32767),
            Version::WIN11_24H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 26100),
            Version::WIN11_24H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 22631),
            Version::WIN11_23H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 22621),
            Version::WIN11_22H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 22000), Version::WIN11);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 21999),
            Version::SERVER_2022);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 20348),
            Version::SERVER_2022);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 20347),
            Version::WIN10_22H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 19045),
            Version::WIN10_22H2);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(10, 0, 19044),
            Version::WIN10_21H2);
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
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(12, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(9, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(8, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(7, 0, 0), Version::WIN_LAST);
  EXPECT_EQ(OSInfo::MajorMinorBuildToVersion(6, 4, 0), Version::WIN8_1);
#endif  // !DCHECK_IS_ON()
}

// For more info on what processor information is defined, see:
// http://msdn.microsoft.com/en-us/library/b0084kay.aspx
TEST(OSInfo, GetWowStatusForProcess) {
#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(OSInfo::GetInstance()->IsWowDisabled());
#elif defined(ARCH_CPU_X86)
  if (OSInfo::GetArchitecture() == OSInfo::X86_ARCHITECTURE) {
    EXPECT_TRUE(OSInfo::GetInstance()->IsWowDisabled());
  } else if (OSInfo::GetArchitecture() == OSInfo::X64_ARCHITECTURE) {
    EXPECT_TRUE(OSInfo::GetInstance()->IsWowX86OnAMD64());
  } else {
    // Currently, the only way to determine if a WOW emulation is
    // running on an ARM64 device is via the function that the
    // |IsWow*| helper functions rely on.  As such, it is not possible
    // to separate out x86 running on ARM64 machines vs x86 running on
    // other host machines.
    EXPECT_TRUE(OSInfo::GetInstance()->IsWowX86OnARM64() ||
                OSInfo::GetInstance()->IsWowX86OnOther());
  }
#else
  ADD_FAILURE()
      << "This test fails when we're using a process or host machine that is "
         "not being considered by our helper functions.  If you're seeing this "
         "error, please add a helper function for determining WOW emulation "
         "for your process and host machine.";
#endif
}

}  // namespace win
}  // namespace base
