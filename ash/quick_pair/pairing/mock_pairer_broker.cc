
// Copyright 2021 The Chromium Authors. All rights reserved.
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

void MockPairerBroker::NotifyDevicePaired(const Device& device) {
  for (auto& obs : observers_)
    obs.OnDevicePaired(device);
}

void MockPairerBroker::NotifyPairFailure(const Device& device,
                                         PairFailure failure) {
  for (auto& obs : observers_)
    obs.OnPairFailure(device, failure);
}

void MockPairerBroker::NotifyAccountKeyWrite(
    const Device& device,
    absl::optional<AccountKeyFailure> failure) {
  for (auto& obs : observers_)
    obs.OnAccountKeyWrite(device, failure);
}

}  // namespace quick_pair
}  // namespace ash
