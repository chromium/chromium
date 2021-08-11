// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetothAdapter;
class BluetoothDevice;

}  // namespace device

namespace ash {
namespace quick_pair {

// This class fakes FastPairGattServiceClient and permits setting which
// PairFailure, if any, is run with the callback.
class FakeFastPairGattServiceClient : public FastPairGattServiceClient {
 public:
  FakeFastPairGattServiceClient(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(absl::optional<PairFailure>)>
          on_initialized_callback);
  ~FakeFastPairGattServiceClient() override;

  device::BluetoothRemoteGattService* gatt_service() override;

  void WriteRequestAsync(uint8_t message_type,
                         uint8_t flags,
                         const std::string& provider_address,
                         const std::string& seekers_address,
                         base::OnceCallback<void(std::vector<uint8_t>,
                                                 absl::optional<PairFailure>)>
                             write_response_callback) override;

  void RunOnGattClientInitializedCallback(
      absl::optional<PairFailure> failure = absl::nullopt);

 private:
  base::OnceCallback<void(absl::optional<PairFailure>)>
      on_initialized_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
      key_based_write_response_callback_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
