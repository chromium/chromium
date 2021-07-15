// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_

#include <functional>
#include "ash/quick_pair/common/device.h"
#include "base/callback.h"

namespace ash {
namespace quick_pair {

enum class AccountKeyFailure;
enum class PairFailure;

// A FastPairPairer instance is responsible for the pairing procedure to a
// single device.  Pairing begins on instantiation.
class FastPairPairer {
 public:
  FastPairPairer(
      const Device& device,
      base::OnceCallback<void(const Device&)> paired_callback,
      base::OnceCallback<void(const Device&, PairFailure)> pair_failed_callback,
      base::OnceCallback<void(const Device&, AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(const Device&)> pairing_procedure_complete);
  FastPairPairer(const FastPairPairer&) = delete;
  FastPairPairer& operator=(const FastPairPairer&) = delete;
  FastPairPairer(FastPairPairer&&);
  FastPairPairer& operator=(FastPairPairer&&);
  ~FastPairPairer();

 private:
  void StartPairing();

  std::reference_wrapper<const Device> device_;
  base::OnceCallback<void(const Device&)> paired_callback_;
  base::OnceCallback<void(const Device&, PairFailure)> pair_failed_callback_;
  base::OnceCallback<void(const Device&, AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(const Device&)> pairing_procedure_complete_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
