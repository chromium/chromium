// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/hwid_checker.h"

#include "base/system/sys_info.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(HWIDCheckerTest, EmptyHWID) {
  EXPECT_FALSE(IsHWIDCorrect(""));
}

TEST(HWIDCheckerTest, HWIDv2) {
  EXPECT_TRUE(IsHWIDCorrect("SOME DATA 7861"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 7861 "));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786 1"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA7861"));
}

TEST(HWIDCheckerTest, ExceptionalHWID) {
  EXPECT_TRUE(IsHWIDCorrect("SPRING A7N3-BJKQ-E"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING A7N3-BJKK-3K"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING A7N3-BJKK-2GI"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING A7N3-BJKK-2MRO"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING A7N3-BJKK-2MDG-V"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING DAKB-NM"));
  EXPECT_TRUE(IsHWIDCorrect("FALCO APOM-3"));

  // Not exceptions.
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-BJKZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING LA7N-BJK7-H"));
  EXPECT_FALSE(IsHWIDCorrect("FALCO BPO6-C"));

  // Degenerate cases.
  EXPECT_FALSE(IsHWIDCorrect("SPRING"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING "));
  EXPECT_FALSE(IsHWIDCorrect("SPRING KD"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING T7"));

  // No board name.
  EXPECT_FALSE(IsHWIDCorrect(" CA7N-BJKV-T"));
  EXPECT_FALSE(IsHWIDCorrect("CA7N-BJKH-S"));

  // Excess fields.
  EXPECT_FALSE(IsHWIDCorrect("SPRING WINTER CA7N-BJK7-T"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-BJKN-D WINTER"));

  // Incorrect BOM format.
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7-NBJK-YO"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA-7NBJ-KYO"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING -CA7N-BJKY-O"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-BJKK-FS-UN"));

  // Incorrect characters.
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA9N-BJKL-P"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-B0KT-S"));
  EXPECT_FALSE(IsHWIDCorrect("SPrING CA7N-BJKH-W"));

  // Random changes.
  EXPECT_FALSE(IsHWIDCorrect("SPRUNG CA7N-BJKY-O"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-8JKY-O"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-BJSY-O"));
  EXPECT_FALSE(IsHWIDCorrect("SPRINGS CA7N-BJKY-O"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING CA7N-BJKM-L"));
  EXPECT_FALSE(IsHWIDCorrect("SPRINGXCA7N-BJKZ-F"));
}

TEST(HWIDCheckerTest, HWIDv3) {
  EXPECT_TRUE(IsHWIDCorrect("SPRING E2B-C3D-E8X"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING E2B-C3D-E8X-D8J"));
  EXPECT_TRUE(IsHWIDCorrect("FALCO B67-36Y"));
  // New HWIDv3 extended format.
  EXPECT_TRUE(
      IsHWIDCorrect("SARIEN-MCOO 0-20-1DC-180 B2B-A6J-23P-43A-B2L-A7I"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING-ZZCR E2B-C3D-E8Z"));

  // Exceptions.
  EXPECT_FALSE(IsHWIDCorrect("SPRING D2B-C3D-E5D"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING A2B-C3D-E8X-D7T"));
  EXPECT_FALSE(IsHWIDCorrect("FALCO A67-35W"));

  // Degenerate cases.
  EXPECT_FALSE(IsHWIDCorrect("SPRING"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING "));
  EXPECT_TRUE(IsHWIDCorrect("SPRING Z34"));

  // No board name.
  EXPECT_FALSE(IsHWIDCorrect(" C7N-J3V-T4J"));
  EXPECT_FALSE(IsHWIDCorrect("C7N-J3V-T2I"));

  // Excess fields.
  EXPECT_FALSE(IsHWIDCorrect("SPRING WINTER E2B-C3D-E3K"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C3D-E72 WINTER"));

  // Incorrect BOM format.
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2BC3D-E8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2-B-C3D-E8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C3D-E8X-"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C3D-E85-Y"));

  // Incorrect characters.
  EXPECT_FALSE(IsHWIDCorrect("SPrING E2B-C3D-E3P"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING EAB-C3D-E7Y"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C1D-E3W"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E28-C3D-E7Z"));

  // Random changes.
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2L-C3D-E8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C3D-X8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRINGZE2B-C3D-E8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRMNG E2B-C3D-E8X"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING E2B-C3D-EIX"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING-ZZCR E2B-C3D-E8X"));
}

TEST(HWIDCheckerTest, KnownHWIDs) {
  EXPECT_TRUE(IsHWIDCorrect("DELL HORIZON MAGENTA 8992"));
  EXPECT_FALSE(IsHWIDCorrect("DELL HORIZ0N MAGENTA 8992"));

  EXPECT_TRUE(IsHWIDCorrect("DELL HORIZON MAGENTA DVT 4770"));
  EXPECT_FALSE(IsHWIDCorrect("DELL MAGENTA HORIZON DVT 4770"));

  EXPECT_TRUE(IsHWIDCorrect("SAMS ALEX GAMMA DVT 9247"));
  EXPECT_FALSE(IsHWIDCorrect("SAMS ALPX GAMMA DVT 9247"));

  // New HWIDv3 extended format.
  EXPECT_TRUE(
      IsHWIDCorrect("SARIEN-MCOO 0-20-1DC-180 B2B-A6J-23P-43A-B2L-A7I"));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test logic for command line "test-type" switch.
TEST(MachineHWIDCheckerTest, TestSwitch) {
  // GIVEN test switch is active.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ::switches::kTestType);

  // THEN IsMachineHWIDCorrect() is always true.
  EXPECT_TRUE(IsMachineHWIDCorrect());
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  EXPECT_TRUE(IsMachineHWIDCorrect());

  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "INVALID_HWID");
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.ClearMachineStatistic(system::kHardwareClassKey);
  EXPECT_TRUE(IsMachineHWIDCorrect());
}

// Test logic when not running on Chrome OS.
TEST(MachineHWIDCheckerTest, NotOnChromeOS) {
  // GIVEN the OS is not Chrome OS.
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  // THEN IsMachineHWIDCorrect() is always true.
  EXPECT_TRUE(IsMachineHWIDCorrect());

  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "INVALID_HWID");
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.ClearMachineStatistic(system::kHardwareClassKey);
  EXPECT_TRUE(IsMachineHWIDCorrect());
}

// Test logic when running on Chrome OS but the HWID is not present.
TEST(MachineHWIDCheckerTest, OnCrosNoHWID) {
  // GIVEN the OS is Chrome OS.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;

  // GIVEN the HWID is not present.
  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.ClearMachineStatistic(system::kHardwareClassKey);

  // WHEN Chrome OS is running in a VM.
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueTrue);
  // THEN IsMachineHWIDCorrect() is true.
  EXPECT_TRUE(IsMachineHWIDCorrect());
  // WHEN Chrome OS is not running in a VM.
  fake_statistics_provider.ClearMachineStatistic(system::kIsVmKey);
  // THEN IsMachineHWIDCorrect() is always false.
  EXPECT_FALSE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueFalse);
  EXPECT_FALSE(IsMachineHWIDCorrect());
}

// Test logic when the HWID is valid.
TEST(MachineHWIDCheckerTest, ValidHWID) {
  // GIVEN the HWID is valid.
  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "DELL HORIZON MAGENTA DVT 4770");

  // THEN IsMachineHWIDCorrect() is always true.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueFalse);
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueTrue);
  EXPECT_TRUE(IsMachineHWIDCorrect());
}

// Test logic when running inside a VM.
TEST(MachineHWIDCheckerTest, InVM) {
  // GIVEN kIsVmKey is kIsVmValueTrue.
  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueTrue);

  // GIVEN the OS is Chrome OS.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  // THEN IsMachineHWIDCorrect() is always true.
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "INVALID_HWID");
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey, "");
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "DELL HORIZON MAGENTA DVT 4770");
  EXPECT_TRUE(IsMachineHWIDCorrect());
  fake_statistics_provider.ClearMachineStatistic(system::kHardwareClassKey);
  EXPECT_TRUE(IsMachineHWIDCorrect());
}

// Test logic when HWID is invalid and we're not in a VM.
TEST(MachineHWIDCheckerTest, InvalidHWIDInVMNotTrue) {
  // GIVEN the OS is Chrome OS.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;

  // GIVEN the HWID is invalid.
  system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(system::kHardwareClassKey,
                                               "INVALID_HWID");

  // GIVEN kIsVmKey is anything but kIsVmValueTrue.
  // THEN IsMachineHWIDCorrect() is always false.
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               system::kIsVmValueFalse);
  EXPECT_FALSE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey,
                                               "INVALID_VM_KEY");
  EXPECT_FALSE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey, "(error)");
  EXPECT_FALSE(IsMachineHWIDCorrect());
  fake_statistics_provider.SetMachineStatistic(system::kIsVmKey, "");
  EXPECT_FALSE(IsMachineHWIDCorrect());
  fake_statistics_provider.ClearMachineStatistic(system::kIsVmKey);
  EXPECT_FALSE(IsMachineHWIDCorrect());
}

#else  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test non-Google Chromium builds.
TEST(MachineHWIDCheckerTest, NonGoogleBuild) {
  // GIVEN this is not a Google build.
  // THEN IsMachineHWIDCorrect() is always true.
  EXPECT_TRUE(IsMachineHWIDCorrect());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ash
