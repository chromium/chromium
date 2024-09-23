// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_hardware_checker.h"

#include "ash/constants/ash_features.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {

namespace {

bool check(std::string board,
           std::string model,
           std::string cpu,
           uint64_t mem_gib) {
  // Fake out the values that will be checked
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_BOARD=" + board + "\n", base::Time());
  ash::system::FakeStatisticsProvider fsp;
  fsp.SetMachineStatistic(ash::system::kCustomizationIdKey, model);
  ash::system::StatisticsProvider::SetTestProvider(&fsp);
  SetCpuForTesting(&cpu);
  // For some reason we're not allowed to pretend to have 0 memory.
  base::test::ScopedAmountOfPhysicalMemoryOverride mem_override(
      std::max(1024 * mem_gib, uint64_t(1)));

  // Now do the actual check
  base::test::TestFuture<bool> result_f;
  HasSufficientHardware(result_f.GetCallback());
  return result_f.Get();
}

}  // namespace

TEST(BorealisHardwareCheckerTest, Hatch) {
  // Valid case, we don't care about model.
  EXPECT_TRUE(
      check("hatch", "fake_model", "Intel(R) Core(TM) i3-10110U CPU", 8));

  // Insufficient ram
  EXPECT_FALSE(
      check("hatch", "fake_model", "Intel(R) Core(TM) i3-10110U CPU", 6));

  // Insufficient CPU
  EXPECT_FALSE(
      check("hatch", "fake_model", "Intel(R) Celeron(R) CPU 5205U", 8));
}

TEST(BorealisHardwareCheckerTest, Volteer) {
  // Previous CPU name branding
  EXPECT_TRUE(check("volteer", "lindar",
                    "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8));
  EXPECT_TRUE(check("volteer-foo", "lindar",
                    "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8));

  // New CPU name branding
  // Note: These are not real Board/Model/CPU combinations.
  EXPECT_TRUE(
      check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 5 NOT_A_CPU", 8));

  // Insufficient ram/cpu - previous CPU name branding
  EXPECT_FALSE(check("volteer", "lindar",
                     "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 2));
  EXPECT_FALSE(check("volteer", "lindar",
                     "11th Gen Intel(R) Core(TM) i1-1145G7 @ 2.60GHz", 8));

  // Insufficient ram/cpu - new CPU name branding
  // Note: These are not real Board/Model/CPU combinations.
  EXPECT_FALSE(
      check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 1 NOT_A_CPU", 8));
  EXPECT_FALSE(
      check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 5 NOT_A_CPU", 2));

  // Any random model
  EXPECT_TRUE(check("volteer", "not_a_real_model",
                    "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8));
}

TEST(BorealisHardwareCheckerTest, GuybrushMajolica) {
  EXPECT_TRUE(
      check("guybrush", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));
  EXPECT_TRUE(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));
  EXPECT_TRUE(
      check("guybrush-dash", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));

  EXPECT_FALSE(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 1));
}

TEST(BorealisHardwareCheckerTest, Aurora) {
  EXPECT_TRUE(check("aurora", "", "", 0));
}

TEST(BorealisHardwareCheckerTest, Myst) {
  EXPECT_TRUE(check("myst", "", "", 0));
}

TEST(BorealisHardwareCheckerTest, Nissa) {
  auto check_nissa = []() {
    return check("nissa", "", "Intel(R) Core(TM) i3-X105", 8);
  };

  // Nissa without FeatureManagement's flag are disallowed
  EXPECT_FALSE(check_nissa());

  // With FeatureManagement's flag, they are allowed
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(ash::features::kFeatureManagementBorealis,
                                /*enabled=*/true);
  EXPECT_TRUE(check_nissa());
}

TEST(BorealisHardwareCheckerTest, Skyrim) {
  auto check_skyrim = []() {
    return check("skyrim", "", "AMD Ryzen 5 X303 with Radeon Graphics", 8);
  };

  EXPECT_FALSE(check_skyrim());
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(ash::features::kFeatureManagementBorealis,
                                /*enabled=*/true);
  EXPECT_TRUE(check_skyrim());
}

TEST(BorealisHardwareCheckerTest, Rex) {
  // TODO(307825451): Put the real CPU here.
  EXPECT_TRUE(check("rex", "", "Fake Cpu", 8));
  EXPECT_FALSE(check("rex", "", "Fake Cpu", 4));
}

}  // namespace borealis
