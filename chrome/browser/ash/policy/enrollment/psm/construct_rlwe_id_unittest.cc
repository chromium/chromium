// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/construct_rlwe_id.h"

#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace {
constexpr char kTestSerialNumber[] = "111111";
constexpr char kTestBrandCode[] = "TEST";

// `kTestBrandCode` encoded in hex.
constexpr char kTestBrandCodeHex[] = "54455354";
}  // namespace

class ConstructRlweIdTest : public testing::Test {
 public:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(ConstructRlweIdTest, VerifyConstructedRlweId) {
  // Sets the values for serial number and RLZ brand code as the values must be
  // present to construct the RLWE ID without CHECK-failures.
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kTestSerialNumber);
  fake_statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                                kTestBrandCode);

  // RLZ brand code "TEST" (as hex), "/" separator, and serial number "111111".
  const std::string kExpectedRlweIdStr =
      std::string(kTestBrandCodeHex) + "/" + std::string(kTestSerialNumber);

  // Construct the PSM RLWE ID, and verify its value.
  psm_rlwe::RlwePlaintextId rlwe_id = policy::psm::ConstructRlweId();
  EXPECT_EQ(rlwe_id.sensitive_id(), kExpectedRlweIdStr);
}
