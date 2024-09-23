// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_NEW_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_NEW_H_

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"

namespace ash::quick_pair {

struct DecryptedResponse;

class FastPairHandshakeImplNew : public FastPairHandshake {
 public:
  FastPairHandshakeImplNew(scoped_refptr<device::BluetoothAdapter> adapter,
                           scoped_refptr<Device> device);
  FastPairHandshakeImplNew(const FastPairHandshakeImplNew&) = delete;
  FastPairHandshakeImplNew& operator=(const FastPairHandshakeImplNew&) = delete;
  ~FastPairHandshakeImplNew() override;

  void SetUpHandshake(OnFailureCallback on_failure_callback,
                      OnCompleteCallbackNew on_success_callback) override;
  void Reset() override;

 private:
  void OnGattClientInitializedCallback(std::optional<PairFailure> failure);
  void OnDataEncryptorCreateAsync(
      base::TimeTicks encryptor_create_start_time,
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor);
  void OnKeybasedPairingWriteResponse(std::vector<uint8_t> response_bytes,
                                      std::optional<PairFailure> failure);
  void OnParseKeybasedPairingDecryptedResponse(
      base::TimeTicks decrypt_start_time,
      const std::optional<DecryptedResponse>& response);

  base::WeakPtrFactory<FastPairHandshakeImplNew> weak_ptr_factory_{this};
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_NEW_H_
