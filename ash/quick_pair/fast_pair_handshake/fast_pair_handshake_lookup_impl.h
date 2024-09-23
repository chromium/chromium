// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"

namespace ash {
namespace quick_pair {

// This class creates, deletes and exposes FastPairHandshake instances.
class FastPairHandshakeLookupImpl : public FastPairHandshakeLookup {
 public:
  static FastPairHandshakeLookupImpl* GetImplInstance();

  static void SetImplCreateFunctionForTesting(CreateFunction create_function);

  FastPairHandshakeLookupImpl(const FastPairHandshakeLookupImpl&) = delete;
  FastPairHandshakeLookupImpl& operator=(const FastPairHandshakeLookupImpl&) =
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

 protected:
  FastPairHandshakeLookupImpl();
  virtual ~FastPairHandshakeLookupImpl();

 private:
  friend struct base::DefaultSingletonTraits<FastPairHandshakeLookupImpl>;

  base::flat_map<scoped_refptr<Device>, std::unique_ptr<FastPairHandshake>>
      fast_pair_handshakes_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_IMPL_H_
