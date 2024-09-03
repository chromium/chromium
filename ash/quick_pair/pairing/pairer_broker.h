// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_H_
#define ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_H_

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

enum class AccountKeyFailure;

// The PairerBroker is the entry point for the Pairing component in the Quick
// pair system. It is responsible for brokering the 'pair to device' calls to
// the correct concrete Pairer implementation, and exposing an observer pattern
// for other components to become aware of pairing results.
class PairerBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPairingStart(scoped_refptr<Device> device) {}
    virtual void OnHandshakeComplete(scoped_refptr<Device> device) {}
    virtual void OnDevicePaired(scoped_refptr<Device> device) {}
    virtual void OnAccountKeyWrite(scoped_refptr<Device> device,
                                   std::optional<AccountKeyFailure> error) {}
    virtual void OnDisplayPasskey(std::u16string device_name,
                                  uint32_t passkey) {}
    virtual void OnPairingComplete(scoped_refptr<Device> device) {}
    virtual void OnPairFailure(scoped_refptr<Device> device,
                               PairFailure failure) {}
  };

  virtual ~PairerBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void PairDevice(scoped_refptr<Device> device) = 0;
  virtual bool IsPairing() = 0;
  virtual void StopPairing() = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_H_
