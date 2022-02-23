// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider_impl.h"

#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy {

class PsmRlweIdProviderImplTest : public testing::Test {
 protected:
  PsmRlweIdProviderImplTest() = default;

  PsmRlweIdProviderImplTest(const PsmRlweIdProviderImplTest&) = delete;
  PsmRlweIdProviderImplTest& operator=(const PsmRlweIdProviderImplTest&) =
      delete;
  ~PsmRlweIdProviderImplTest() override = default;

  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  PsmRlweIdProviderImpl psm_rlwe_impl;
  const std::string kTestSerialNumber = "111111";
  const std::string kTestBrandCode = "TEST";

  // `kTestBrandCode` encoded in hex.
  const std::string kTestBrandCodeHex = "54455354";
};

TEST_F(PsmRlweIdProviderImplTest, VerifyConstructedRlweId) {
  // Sets the values for serial number and RLZ brand code as the values must be
  // present to construct the RLWE ID without CHECK-failures.
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kSerialNumberKeyForTest, kTestSerialNumber);
  fake_statistics_provider_.SetMachineStatistic(
      chromeos::system::kRlzBrandCodeKey, kTestBrandCode);

  // RLZ brand code "TEST" (as hex), "/" seperator, and serial number "111111".
  const std::string kExpectedPsmRlweIdStr =
      kTestBrandCodeHex + "/" + kTestSerialNumber;

  // Construct the PSM RLWE ID, and verify its value.
  psm_rlwe::RlwePlaintextId rlwe_id = psm_rlwe_impl.ConstructRlweId();
  EXPECT_EQ(rlwe_id.sensitive_id(), kExpectedPsmRlweIdStr);
}

}  // namespace policy
