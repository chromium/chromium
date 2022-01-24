// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/quick_pair_process.h"

#include <cstdint>
#include <vector>

#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {
namespace quick_pair_process {

class QuickPairProcessTest : public testing::Test {};

TEST_F(QuickPairProcessTest,
       GetHexModelIdFromServiceData_NoValueIfNoProcessManagerSet) {
  GetHexModelIdFromServiceData(
      std::vector<uint8_t>(),
      base::BindLambdaForTesting([](const absl::optional<std::string>& result) {
        EXPECT_FALSE(result.has_value());
      }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseDecryptedResponse_NoValueIfNoProcessManagerSet) {
  ParseDecryptedResponse(
      std::vector<uint8_t>(), std::vector<uint8_t>(),
      base::BindLambdaForTesting(
          [](const absl::optional<DecryptedResponse>& result) {
            EXPECT_FALSE(result.has_value());
          }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseDecryptedPasskey_NoValueIfNoProcessManagerSet) {
  ParseDecryptedPasskey(std::vector<uint8_t>(), std::vector<uint8_t>(),
                        base::BindLambdaForTesting(
                            [](const absl::optional<DecryptedPasskey>& result) {
                              EXPECT_FALSE(result.has_value());
                            }),
                        base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseNotDiscoverableAdvertisement_NoValueIfNoProcessManagerSet) {
  ParseNotDiscoverableAdvertisement(
      std::vector<uint8_t>(),
      base::BindLambdaForTesting(
          [](const absl::optional<NotDiscoverableAdvertisement>& result) {
            EXPECT_FALSE(result.has_value());
          }),
      base::DoNothing());
}

}  // namespace quick_pair_process
}  // namespace quick_pair
}  // namespace ash
