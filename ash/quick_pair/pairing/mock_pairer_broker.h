// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_MOCK_PAIRER_BROKER_H_
#define ASH_QUICK_PAIR_PAIRING_MOCK_PAIRER_BROKER_H_

#include <optional>

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "base/observer_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class Device;
enum class AccountKeyFailure;

class MockPairerBroker : public PairerBroker {
 public:
  MockPairerBroker();
  MockPairerBroker(const MockPairerBroker&) = delete;
  MockPairerBroker& operator=(const MockPairerBroker&) = delete;
  ~MockPairerBroker() override;

  MOCK_METHOD(void, PairDevice, (scoped_refptr<Device>), (override));
  MOCK_METHOD(bool, IsPairing, (), (override));
  MOCK_METHOD(void, StopPairing, (), (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyPairingStart(scoped_refptr<Device> device);
  void NotifyHandshakeComplete(scoped_refptr<Device> device);
  void NotifyDevicePaired(scoped_refptr<Device> device);
  void NotifyPairFailure(scoped_refptr<Device> device, PairFailure failure);
  void NotifyPairComplete(scoped_refptr<Device> device);
  void NotifyAccountKeyWrite(scoped_refptr<Device> device,
                             std::optional<AccountKeyFailure> error);
  void NotifyDisplayPasskey(std::u16string device_name, uint32_t passkey);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_MOCK_PAIRER_BROKER_H_
