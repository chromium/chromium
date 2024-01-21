// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_token_hardware_checker.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {

namespace {

bool check(std::string board,
           std::string model,
           std::string cpu,
           uint64_t mem_gib) {
  return BorealisTokenHardwareChecker::BuildAndCheck(
      {std::move(board), std::move(model), std::move(cpu),
       mem_gib * 1024 * 1024 * 1024});
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

TEST(BorealisTokenHardwareCheckerTest, Volteer) {
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

  // Bad model
  EXPECT_FALSE(check("volteer", "not_a_real_model",
                     "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8));
}

TEST(BorealisTokenHardwareCheckerTest, GuybrushMajolica) {
  EXPECT_TRUE(
      check("guybrush", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));
  EXPECT_TRUE(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));
  EXPECT_TRUE(
      check("guybrush-dash", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8));

  EXPECT_FALSE(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 1));
}

TEST(BorealisTokenHardwareCheckerTest, Aurora) {
  EXPECT_TRUE(check("aurora", "", "", 0));
}

TEST(BorealisTokenHardwareCheckerTest, Myst) {
  EXPECT_TRUE(check("myst", "", "", 0));
}

TEST(BorealisTokenHardwareCheckerTest, Nissa) {
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

TEST(BorealisTokenHardwareCheckerTest, Skyrim) {
  auto check_skyrim = []() {
    return check("skyrim", "", "AMD Ryzen 5 X303 with Radeon Graphics", 8);
  };

  EXPECT_FALSE(check_skyrim());
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(ash::features::kFeatureManagementBorealis,
                                /*enabled=*/true);
  EXPECT_TRUE(check_skyrim());
}

TEST(BorealisTokenHardwareCheckerTest, Rex) {
  // TODO(307825451): Put the real CPU here.
  EXPECT_TRUE(check("rex", "", "Fake Cpu", 8));
  EXPECT_FALSE(check("rex", "", "Fake Cpu", 4));
}

}  // namespace borealis
