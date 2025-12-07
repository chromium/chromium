// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace ash {
namespace quick_pair {

class Device;
class FastPairPairer;

class PairerBrokerImpl final : public PairerBroker {
 public:
  PairerBrokerImpl();
  PairerBrokerImpl(const PairerBrokerImpl&) = delete;
  PairerBrokerImpl& operator=(const PairerBrokerImpl&) = delete;
  ~PairerBrokerImpl() override;

  // PairingBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void PairDevice(scoped_refptr<Device> device) override;
  bool IsPairing() override;
  void StopPairing() override;

  friend class PairerBrokerImplTest;

 private:
  void OnBleAddressRotation(scoped_refptr<Device> device);
  void PairFastPairDevice(scoped_refptr<Device> device);
  void OnFastPairDeviceBonded(scoped_refptr<Device> device);
  void OnFastPairBondingFailure(scoped_refptr<Device> device,
                                PairFailure failure);
  void OnAccountKeyFailure(scoped_refptr<Device> device,
                           AccountKeyFailure failure);
  void OnFastPairProcedureComplete(scoped_refptr<Device> device);
  void OnDisplayPasskey(std::u16string device_name, uint32_t passkey);
  void CreateHandshake(scoped_refptr<Device> device);
  void OnHandshakeComplete(scoped_refptr<Device> device,
                           std::optional<PairFailure> failure);
  void OnHandshakeFailure(scoped_refptr<Device> device, PairFailure failure);
  void StartBondingAttempt(scoped_refptr<Device> device);

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  void EraseHandshakeAndFromPairers(scoped_refptr<Device> device);

  // The key for all the following maps is a device model id.
  base::flat_map<std::string, std::unique_ptr<FastPairPairer>>
      fast_pair_pairers_;
  base::flat_map<std::string, int> pair_failure_counts_;
  base::flat_map<std::string, bool>
      did_handshake_previously_complete_successfully_map_;
  base::flat_map<std::string, int> num_handshake_attempts_;
  base::flat_map<std::string, std::string> model_id_to_current_ble_address_map_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::ObserverList<Observer> observers_;

  // Timer to provide a delay after cancelling pairing.
  base::OneShotTimer cancel_pairing_timer_;

  base::OneShotTimer retry_handshake_timer_;

  base::WeakPtrFactory<PairerBrokerImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_PAIRER_BROKER_IMPL_H_
