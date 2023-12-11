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

using AllowStatus = BorealisFeatures::AllowStatus;

AllowStatus check(std::string board,
                  std::string model,
                  std::string cpu,
                  uint64_t mem_gib,
                  std::string token) {
  std::string hashed_token =
      TokenHardwareChecker::H(std::move(token), kSaltForPrefStorage);
  AllowStatus stat = BorealisTokenHardwareChecker::BuildAndCheck(
      {std::move(hashed_token), std::move(board), std::move(model),
       std::move(cpu), mem_gib * 1024 * 1024 * 1024});
  return stat;
}

}  // namespace

TEST(BorealisTokenHardwareCheckerTest, Volteer) {
  // Previous CPU name branding
  EXPECT_EQ(check("volteer", "lindar",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8, ""),
            AllowStatus::kAllowed);
  EXPECT_EQ(check("volteer-foo", "lindar",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8, ""),
            AllowStatus::kAllowed);

  // New CPU name branding
  // Note: These are not real Board/Model/CPU combinations.
  EXPECT_EQ(check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 5 NOT_A_CPU",
                  8, ""),
            AllowStatus::kAllowed);

  // Insufficient ram/cpu - previous CPU name branding
  EXPECT_EQ(check("volteer", "lindar",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 2, ""),
            AllowStatus::kHardwareChecksFailed);
  EXPECT_EQ(check("volteer", "lindar",
                  "11th Gen Intel(R) Core(TM) i1-1145G7 @ 2.60GHz", 8, ""),
            AllowStatus::kHardwareChecksFailed);

  // Insufficient ram/cpu - new CPU name branding
  // Note: These are not real Board/Model/CPU combinations.
  EXPECT_EQ(check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 1 NOT_A_CPU",
                  8, ""),
            AllowStatus::kHardwareChecksFailed);
  EXPECT_EQ(check("volteer", "lindar", "11th Gen Intel(R) Core(TM) 5 NOT_A_CPU",
                  2, ""),
            AllowStatus::kHardwareChecksFailed);

  // Bad model
  EXPECT_EQ(check("volteer", "not_a_real_model",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8, ""),
            AllowStatus::kUnsupportedModel);
}

TEST(BorealisTokenHardwareCheckerTest, GuybrushMajolica) {
  EXPECT_EQ(
      check("guybrush", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8, ""),
      AllowStatus::kAllowed);
  EXPECT_EQ(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 8, ""),
      AllowStatus::kAllowed);
  EXPECT_EQ(check("guybrush-dash", "", "AMD Ryzen 5 5625C with Radeon Graphics",
                  8, ""),
            AllowStatus::kAllowed);

  EXPECT_EQ(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 1, ""),
      AllowStatus::kHardwareChecksFailed);
}

TEST(BorealisTokenHardwareCheckerTest, Aurora) {
  EXPECT_EQ(check("aurora", "", "", 0, ""), AllowStatus::kAllowed);
}

TEST(BorealisTokenHardwareCheckerTest, Myst) {
  EXPECT_EQ(check("myst", "", "", 0, ""), AllowStatus::kAllowed);
}

TEST(BorealisTokenHardwareCheckerTest, Nissa) {
  auto check_nissa = []() {
    return check("nissa", "", "Intel(R) Core(TM) i3-X105", 8, "");
  };

  // Nissa without FeatureManagement's flag are disallowed
  EXPECT_EQ(check_nissa(), AllowStatus::kUnsupportedModel);

  // With FeatureManagement's flag, they are allowed
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(ash::features::kFeatureManagementBorealis,
                                /*enabled=*/true);
  EXPECT_EQ(check_nissa(), AllowStatus::kAllowed);
}

TEST(BorealisTokenHardwareCheckerTest, Skyrim) {
  auto check_skyrim = []() {
    return check("skyrim", "", "AMD Ryzen 5 X303 with Radeon Graphics", 8, "");
  };

  EXPECT_EQ(check_skyrim(), AllowStatus::kUnsupportedModel);
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(ash::features::kFeatureManagementBorealis,
                                /*enabled=*/true);
  EXPECT_EQ(check_skyrim(), AllowStatus::kAllowed);
}

TEST(BorealisTokenHardwareCheckerTest, Rex) {
  // TODO(307825451): Put the real CPU here.
  EXPECT_EQ(check("rex", "", "Fake Cpu", 8, ""), AllowStatus::kAllowed);
  EXPECT_EQ(check("rex", "", "Fake Cpu", 4, ""),
            AllowStatus::kHardwareChecksFailed);
}

// Procedure for adding and new token:
//  - uncomment the below test
//  - Fill it in with details of the board you're trying to bypass
//    - you can do more than one at once
//  - Add a LOG statement in TokenHashMatches() to print the token
//  - Run this test
//  - RESTORE THIS TEST TO ITS ORIGINAL STATE!
//    - If you accidentally upload the token in plaintext you're going to have
//      to create a new token.
//
// TEST(BorealisTokenHardwareCheckerTest, BypassTokens) {
//   EXPECT_EQ(check("", "", "", 0, ""), AllowStatus::kAllowed);
// }

}  // namespace borealis
