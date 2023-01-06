// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_

#include <memory>

#include "ash/quick_pair/common/pair_failure.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

class Device;
class FastPairHandshake;

// This class creates, deletes and exposes FastPairHandshake instances.
class FastPairHandshakeLookup {
 public:
  using OnCompleteCallback =
      base::OnceCallback<void(scoped_refptr<Device>,
                              absl::optional<PairFailure>)>;

  using CreateFunction =
      base::RepeatingCallback<std::unique_ptr<FastPairHandshake>(
          scoped_refptr<Device> device,
          OnCompleteCallback callback)>;

  static FastPairHandshakeLookup* GetInstance();
  static void SetCreateFunctionForTesting(CreateFunction create_function);

  FastPairHandshakeLookup(const FastPairHandshakeLookup&) = delete;
  FastPairHandshakeLookup& operator=(const FastPairHandshakeLookup&) = delete;

  // Get an existing instance for |device|.
  FastPairHandshake* Get(scoped_refptr<Device> device);

  // Get an existing instance for |address|.
  FastPairHandshake* Get(const std::string& address);

  // Erases the FastPairHandshake instance for |device| if exists.
  bool Erase(scoped_refptr<Device> device);

  // Erases the FastPairHandshake instance for |address| if exists.
  bool Erase(const std::string& address);

  // Deletes all existing FastPairHandshake instances.
  void Clear();

  // Creates and returns a new instance for |device| if no instance already
  // exists. Returns the existing instance if there is one.
  FastPairHandshake* Create(scoped_refptr<device::BluetoothAdapter> adapter,
                            scoped_refptr<Device> device,
                            OnCompleteCallback on_complete);

 protected:
  FastPairHandshakeLookup();
  virtual ~FastPairHandshakeLookup();

 private:
  friend struct base::DefaultSingletonTraits<FastPairHandshakeLookup>;

  base::flat_map<scoped_refptr<Device>, std::unique_ptr<FastPairHandshake>>
      fast_pair_handshakes_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
