// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_H_

#include <memory>

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"

#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

class FakeFastPairHandshake : public FastPairHandshake {
 public:
  FakeFastPairHandshake(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<Device> device,
      OnCompleteCallback on_complete,
      std::unique_ptr<FastPairDataEncryptor> data_encryptor = nullptr,
      std::unique_ptr<FastPairGattServiceClient> gatt_service_client = nullptr);
  FakeFastPairHandshake(const FakeFastPairHandshake&) = delete;
  FakeFastPairHandshake& operator=(const FakeFastPairHandshake&) = delete;
  ~FakeFastPairHandshake() override;

  void InvokeCallback(absl::optional<PairFailure> failure = absl::nullopt);

  void set_completed_successfully(bool completed_successfully) {
    completed_successfully_ = completed_successfully;
  }
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_H_
