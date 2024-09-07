// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/mock_pairer_broker.h"

#include "ash/quick_pair/common/device.h"

namespace ash {
namespace quick_pair {

MockPairerBroker::MockPairerBroker() = default;

MockPairerBroker::~MockPairerBroker() = default;

void MockPairerBroker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockPairerBroker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockPairerBroker::NotifyDevicePaired(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnDevicePaired(device);
}

void MockPairerBroker::NotifyPairFailure(scoped_refptr<Device> device,
                                         PairFailure failure) {
  for (auto& obs : observers_)
    obs.OnPairFailure(device, failure);
}

void MockPairerBroker::NotifyAccountKeyWrite(
    scoped_refptr<Device> device,
    std::optional<AccountKeyFailure> failure) {
  for (auto& obs : observers_)
    obs.OnAccountKeyWrite(device, failure);
}

void MockPairerBroker::NotifyPairingStart(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnPairingStart(device);
}

void MockPairerBroker::NotifyDisplayPasskey(std::u16string device_name,
                                            uint32_t passkey) {
  for (auto& obs : observers_) {
    obs.OnDisplayPasskey(device_name, passkey);
  }
}

void MockPairerBroker::NotifyHandshakeComplete(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnHandshakeComplete(device);
}

void MockPairerBroker::NotifyPairComplete(scoped_refptr<Device> device) {
  for (auto& obs : observers_)
    obs.OnPairingComplete(device);
}

}  // namespace quick_pair
}  // namespace ash
