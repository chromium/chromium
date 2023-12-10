// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_ASYNC_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_ASYNC_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_

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

// This class creates, deletes and exposes FastPairHandshake instances.
class AsyncFastPairHandshakeLookupImpl : public FastPairHandshakeLookup {
 public:
  static AsyncFastPairHandshakeLookupImpl* GetAsyncInstance();

  AsyncFastPairHandshakeLookupImpl(const AsyncFastPairHandshakeLookupImpl&) =
      delete;
  AsyncFastPairHandshakeLookupImpl& operator=(
      const AsyncFastPairHandshakeLookupImpl&) = delete;

  // FastPairHandshakeLookup:
  FastPairHandshake* Get(scoped_refptr<Device> device) override;
  FastPairHandshake* Get(const std::string& address) override;
  bool Erase(scoped_refptr<Device> device) override;
  bool Erase(const std::string& address) override;
  void Clear() override;
  void Create(scoped_refptr<device::BluetoothAdapter> adapter,
              scoped_refptr<Device> device,
              OnCompleteCallback on_complete) override;

 protected:
  AsyncFastPairHandshakeLookupImpl();
  virtual ~AsyncFastPairHandshakeLookupImpl();
  void AttemptHandshakeWithRetries(scoped_refptr<Device> device,
                                   OnCompleteCallback on_complete_callback,
                                   std::optional<PairFailure> failure);

  void OnHandshakeComplete(OnCompleteCallback on_complete_callback,
                           scoped_refptr<Device> device);

 private:
  friend struct base::DefaultSingletonTraits<AsyncFastPairHandshakeLookupImpl>;
  FRIEND_TEST_ALL_PREFIXES(AsyncFastPairHandshakeLookupImplTest,
                           FailThenSuccessfullyCompleteHandshake);

  base::flat_map<scoped_refptr<Device>, std::unique_ptr<FastPairHandshake>>
      fast_pair_handshakes_;
  base::flat_map<scoped_refptr<Device>, int>
      fast_pair_handshake_attempt_counts_;
  base::WeakPtrFactory<AsyncFastPairHandshakeLookupImpl> weak_ptr_factory_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_ASYNC_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_
