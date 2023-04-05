// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_token_hardware_checker.h"

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
  EXPECT_EQ(check("volteer", "lindar",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 8, ""),
            AllowStatus::kAllowed);

  // Insufficient ram/cpu
  EXPECT_EQ(check("volteer", "lindar",
                  "11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz", 2, ""),
            AllowStatus::kHardwareChecksFailed);
  EXPECT_EQ(check("volteer", "lindar",
                  "10th Gen Intel(R) Core(TM) i5-1045G7 @ 2.60GHz", 8, ""),
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

  EXPECT_EQ(
      check("majolica", "", "AMD Ryzen 5 5625C with Radeon Graphics", 1, ""),
      AllowStatus::kHardwareChecksFailed);
}

TEST(BorealisTokenHardwareCheckerTest, Draco) {
  EXPECT_EQ(check("draco", "", "", 0, ""), AllowStatus::kAllowed);
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
