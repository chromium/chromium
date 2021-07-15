// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "base/callback.h"

namespace ash {
namespace quick_pair {

FastPairPairer::FastPairPairer(
    const Device& device,
    base::OnceCallback<void(const Device&)> paired_callback,
    base::OnceCallback<void(const Device&, PairFailure)> pair_failed_callback,
    base::OnceCallback<void(const Device&, AccountKeyFailure)>
        account_key_failure_callback,
    base::OnceCallback<void(const Device&)> pairing_procedure_complete)
    : device_(device),
      paired_callback_(std::move(paired_callback)),
      pair_failed_callback_(std::move(pair_failed_callback)),
      account_key_failure_callback_(std::move(account_key_failure_callback)),
      pairing_procedure_complete_(std::move(pairing_procedure_complete)) {
  StartPairing();
}

FastPairPairer::FastPairPairer(FastPairPairer&&) = default;

FastPairPairer& FastPairPairer::operator=(FastPairPairer&&) = default;

FastPairPairer::~FastPairPairer() = default;

void FastPairPairer::StartPairing() {
  QP_LOG(INFO) << __func__ << ": " << device_;
  std::move(paired_callback_).Run(device_);
  std::move(pairing_procedure_complete_).Run(device_);
}

}  // namespace quick_pair
}  // namespace ash
