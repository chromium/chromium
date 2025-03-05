// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hardware_check.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(HardwareEvaluationResult, Eligible) {
  HardwareEvaluationResult result{
      .cpu = true, .memory = true, .disk = true, .firmware = true, .tpm = true};
  EXPECT_TRUE(result.IsEligible());

  result.cpu = false;
  EXPECT_FALSE(result.IsEligible());
}

TEST(EvaluateWin11HardwareRequirements, ExpectNoCrash) {
  // It's not worthwhile to check the validity of the return value
  // so just check for crashes.
  EvaluateWin11HardwareRequirements();
}

}  // namespace base::win
