// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_

#include <memory>
#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

// This class creates, deletes and exposes FastPairHandshake instances.
class FastPairHandshakeLookup {
 public:
  using OnCompleteCallback =
      base::OnceCallback<void(scoped_refptr<Device>,
                              std::optional<PairFailure>)>;

  using CreateFunction =
      base::RepeatingCallback<std::unique_ptr<FastPairHandshake>(
          scoped_refptr<Device> device,
          OnCompleteCallback callback)>;

  static FastPairHandshakeLookup* GetInstance();

  static void UseFakeInstance();

  // Get an existing instance for |device|.
  virtual FastPairHandshake* Get(scoped_refptr<Device> device) = 0;

  // Get an existing instance for |address|.
  virtual FastPairHandshake* Get(const std::string& address) = 0;

  // Erases the FastPairHandshake instance for |device| if exists.
  virtual bool Erase(scoped_refptr<Device> device) = 0;

  // Erases the FastPairHandshake instance for |address| if exists.
  virtual bool Erase(const std::string& address) = 0;

  // Deletes all existing FastPairHandshake instances.
  virtual void Clear() = 0;

  // Creates and returns a new instance for |device| if no instance already
  // exists. Returns the existing instance if there is one.
  virtual void Create(scoped_refptr<device::BluetoothAdapter> adapter,
                      scoped_refptr<Device> device,
                      OnCompleteCallback on_complete) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
