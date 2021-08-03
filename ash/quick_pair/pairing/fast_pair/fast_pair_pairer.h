// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace ash {
namespace quick_pair {

struct Device;
enum class AccountKeyFailure;
enum class PairFailure;

// A FastPairPairer instance is responsible for the pairing procedure to a
// single device.  Pairing begins on instantiation.
class FastPairPairer {
 public:
  FastPairPairer(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<Device> device,
      base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
      base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(scoped_refptr<Device>)>
          pairing_procedure_complete);
  FastPairPairer(const FastPairPairer&) = delete;
  FastPairPairer& operator=(const FastPairPairer&) = delete;
  FastPairPairer(FastPairPairer&&) = delete;
  FastPairPairer& operator=(FastPairPairer&&) = delete;
  ~FastPairPairer();

 private:
  void OnGattClientInitializedCallback(absl::optional<PairFailure> failure);

  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  base::OnceCallback<void(scoped_refptr<Device>)> paired_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
      pair_failed_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete_;
  std::unique_ptr<FastPairGattServiceClient> fast_pair_gatt_service_client_;
  base::WeakPtrFactory<FastPairPairer> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
