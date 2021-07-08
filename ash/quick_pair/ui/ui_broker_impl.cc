// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/ui_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "base/bind.h"

namespace ash {
namespace quick_pair {

UIBrokerImpl::UIBrokerImpl()
    : fast_pair_presenter_(std::make_unique<FastPairPresenter>()) {}

UIBrokerImpl::~UIBrokerImpl() = default;

void UIBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UIBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UIBrokerImpl::ShowDiscovery(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      fast_pair_presenter_->ShowDiscovery(
          device,
          base::BindOnce(&UIBrokerImpl::NotifyDiscoveryAction,
                         weak_pointer_factory_.GetWeakPtr(), std::ref(device)));
      break;
  }
}

void UIBrokerImpl::ShowPairing(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      fast_pair_presenter_->ShowPairing(device);
      break;
  }
}

void UIBrokerImpl::ShowPairingFailed(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      fast_pair_presenter_->ShowPairingFailed(
          device,
          base::BindOnce(&UIBrokerImpl::NotifyPairingFailedAction,
                         weak_pointer_factory_.GetWeakPtr(), std::ref(device)));
      break;
  }
}

void UIBrokerImpl::ShowAssociateAccount(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      fast_pair_presenter_->ShowAssociateAccount(
          device,
          base::BindOnce(&UIBrokerImpl::NotifyAssociateAccountAction,
                         weak_pointer_factory_.GetWeakPtr(), std::ref(device)));
      break;
  }
}

void UIBrokerImpl::ShowCompanionApp(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      fast_pair_presenter_->ShowCompanionApp(
          device,
          base::BindOnce(&UIBrokerImpl::NotifyCompanionAppAction,
                         weak_pointer_factory_.GetWeakPtr(), std::ref(device)));
      break;
  }
}

void UIBrokerImpl::NotifyDiscoveryAction(const Device& device,
                                         DiscoveryAction action) {
  for (auto& observer : observers_)
    observer.OnDiscoveryAction(device, action);
}

void UIBrokerImpl::NotifyPairingFailedAction(const Device& device,
                                             PairingFailedAction action) {
  for (auto& observer : observers_)
    observer.OnPairingFailureAction(device, action);
}

void UIBrokerImpl::NotifyAssociateAccountAction(const Device& device,
                                                AssociateAccountAction action) {
  for (auto& observer : observers_)
    observer.OnAssociateAccountAction(device, action);
}

void UIBrokerImpl::NotifyCompanionAppAction(const Device& device,
                                            CompanionAppAction action) {
  for (auto& observer : observers_)
    observer.OnCompanionAppAction(device, action);
}

}  // namespace quick_pair
}  // namespace ash
