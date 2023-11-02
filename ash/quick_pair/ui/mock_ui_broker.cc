// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/mock_ui_broker.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/ui/actions.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

MockUIBroker::MockUIBroker() = default;

MockUIBroker::~MockUIBroker() = default;

void MockUIBroker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockUIBroker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockUIBroker::NotifyDiscoveryAction(scoped_refptr<Device> device,
                                         DiscoveryAction action) {
  for (auto& obs : observers_)
    obs.OnDiscoveryAction(device, action);
}

void MockUIBroker::NotifyCompanionAppAction(scoped_refptr<Device> device,
                                            CompanionAppAction action) {
  for (auto& obs : observers_)
    obs.OnCompanionAppAction(device, action);
}

void MockUIBroker::NotifyPairingFailedAction(scoped_refptr<Device> device,
                                             PairingFailedAction action) {
  for (auto& obs : observers_)
    obs.OnPairingFailureAction(device, action);
}

void MockUIBroker::NotifyAssociateAccountAction(scoped_refptr<Device> device,
                                                AssociateAccountAction action) {
  for (auto& obs : observers_)
    obs.OnAssociateAccountAction(device, action);
}

}  // namespace quick_pair
}  // namespace ash
