// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_

#include <memory>
#include <string>

#include "ash/quick_pair/pairing/pairer_broker.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

struct Device;
class FastPairPairer;

class PairerBrokerImpl : public PairerBroker {
 public:
  PairerBrokerImpl();
  PairerBrokerImpl(const PairerBrokerImpl&) = delete;
  PairerBrokerImpl& operator=(const PairerBrokerImpl&) = delete;
  ~PairerBrokerImpl() final;

  // PairingBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void PairDevice(const Device& device) override;

 private:
  void PairFastPairDevice(const Device& device);
  void OnFastPairDevicePaired(const Device& device);
  void OnFastPairPairingFailure(const Device& device, PairFailure failure);
  void OnAccountKeyFailure(const Device& device, AccountKeyFailure failure);
  void OnFastPairProcedureComplete(const Device& device);

  base::flat_map<std::string, std::unique_ptr<FastPairPairer>>
      fast_pair_pairers_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<PairerBrokerImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_
