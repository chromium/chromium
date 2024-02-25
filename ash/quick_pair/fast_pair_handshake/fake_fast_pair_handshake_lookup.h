// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"

#include <memory>

#include "base/functional/callback.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

class Device;
class FastPairHandshake;
class FastPairDataEncryptor;
class FastPairGattServiceClient;

// This class creates, deletes and exposes FastPairHandshake instances.
class FakeFastPairHandshakeLookup : public FastPairHandshakeLookup {
 public:
  using OnCompleteCallback =
      base::OnceCallback<void(scoped_refptr<Device>,
                              std::optional<PairFailure>)>;

  static FakeFastPairHandshakeLookup* GetFakeInstance();

  FakeFastPairHandshakeLookup(const FakeFastPairHandshakeLookup&) = delete;
  FakeFastPairHandshakeLookup& operator=(const FakeFastPairHandshakeLookup&) =
      delete;

  // FastPairHandshakeLookup:
  FastPairHandshake* Get(scoped_refptr<Device> device) override;
  FastPairHandshake* Get(const std::string& address) override;
  bool Erase(scoped_refptr<Device> device) override;
  bool Erase(const std::string& address) override;
  void Clear() override;
  void Create(scoped_refptr<device::BluetoothAdapter> adapter,
              scoped_refptr<Device> device,
              OnCompleteCallback on_complete) override;

  void InvokeCallbackForTesting(scoped_refptr<Device> device,
                                std::optional<PairFailure> failure);
  void CreateForTesting(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<Device> device,
      OnCompleteCallback on_complete,
      std::unique_ptr<FastPairGattServiceClient> gatt_service_client,
      std::unique_ptr<FastPairDataEncryptor> data_encryptor);

 protected:
  FakeFastPairHandshakeLookup();
  virtual ~FakeFastPairHandshakeLookup();

 private:
  friend struct base::DefaultSingletonTraits<FakeFastPairHandshakeLookup>;

  OnCompleteCallback on_complete_callback_;
  base::flat_map<scoped_refptr<Device>, std::unique_ptr<FastPairHandshake>>
      fast_pair_handshakes_;
  base::flat_map<scoped_refptr<Device>, int>
      fast_pair_handshake_attempt_counts_;
  base::WeakPtrFactory<FakeFastPairHandshakeLookup> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
