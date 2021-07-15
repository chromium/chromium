// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include <memory>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairPairerTest : public testing::Test {
 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairer>(
        device_, paired_callback_.Get(), pair_failed_callback_.Get(),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  Device device_{"test_id", "test_address", Protocol::kFastPair};
  base::MockCallback<base::OnceCallback<void(const Device&)>> paired_callback_;
  base::MockCallback<base::OnceCallback<void(const Device&, PairFailure)>>
      pair_failed_callback_;
  base::MockCallback<base::OnceCallback<void(const Device&, AccountKeyFailure)>>
      account_key_failure_callback_;
  base::MockCallback<base::OnceCallback<void(const Device&)>>
      pairing_procedure_complete_;
  std::unique_ptr<FastPairPairer> pairer_;
};

TEST_F(FastPairPairerTest, PairingProcedureCompleteCallbackIsInvoked) {
  EXPECT_CALL(pairing_procedure_complete_, Run);
  CreatePairer();
}

}  // namespace quick_pair
}  // namespace ash
