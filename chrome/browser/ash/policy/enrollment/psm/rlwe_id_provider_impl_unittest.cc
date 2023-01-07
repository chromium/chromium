// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_id_provider_impl.h"

#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy::psm {

class RlweIdProviderImplTest : public testing::Test {
 public:
  RlweIdProviderImplTest() = default;

  RlweIdProviderImplTest(const RlweIdProviderImplTest&) = delete;
  RlweIdProviderImplTest& operator=(const RlweIdProviderImplTest&) = delete;
  ~RlweIdProviderImplTest() override = default;

  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  RlweIdProviderImpl psm_rlwe_impl;
  const std::string kTestSerialNumber = "111111";
  const std::string kTestBrandCode = "TEST";

  // `kTestBrandCode` encoded in hex.
  const std::string kTestBrandCodeHex = "54455354";
};

TEST_F(RlweIdProviderImplTest, VerifyConstructedRlweId) {
  // Sets the values for serial number and RLZ brand code as the values must be
  // present to construct the RLWE ID without CHECK-failures.
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kSerialNumberKeyForTest, kTestSerialNumber);
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kRlzBrandCodeKey, kTestBrandCode);

  // RLZ brand code "TEST" (as hex), "/" separator, and serial number "111111".
  const std::string kExpectedRlweIdStr =
      kTestBrandCodeHex + "/" + kTestSerialNumber;

  // Construct the PSM RLWE ID, and verify its value.
  psm_rlwe::RlwePlaintextId rlwe_id = psm_rlwe_impl.ConstructRlweId();
  EXPECT_EQ(rlwe_id.sensitive_id(), kExpectedRlweIdStr);
}

}  // namespace policy::psm
