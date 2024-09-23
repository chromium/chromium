// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/ui_broker_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"
#include "base/functional/bind.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace quick_pair {

UIBrokerImpl::UIBrokerImpl()
    : fast_pair_presenter_(FastPairPresenterImpl::Factory::Create(
          message_center::MessageCenter::Get())) {}

UIBrokerImpl::~UIBrokerImpl() = default;

void UIBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UIBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UIBrokerImpl::ShowDiscovery(scoped_refptr<Device> device) {
  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      fast_pair_presenter_->ShowDiscovery(
          device,
          base::BindRepeating(&UIBrokerImpl::NotifyDiscoveryAction,
                              weak_pointer_factory_.GetWeakPtr(), device));
      break;
    case Protocol::kFastPairRetroactive:
      NOTREACHED();
  }
}

void UIBrokerImpl::ShowPairing(scoped_refptr<Device> device) {
  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      fast_pair_presenter_->ShowPairing(std::move(device));
      break;
  }
}

void UIBrokerImpl::ShowPairingFailed(scoped_refptr<Device> device) {
  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      fast_pair_presenter_->ShowPairingFailed(
          device,
          base::BindRepeating(&UIBrokerImpl::NotifyPairingFailedAction,
                              weak_pointer_factory_.GetWeakPtr(), device));
      break;
    case Protocol::kFastPairRetroactive:
      // In this scenario, we don't show the error UI because it would be
      // misleading, since a pair failure is a retroactive pair failure, and
      // guiding the user back to settings doesn't make sense.
      break;
  }
}

void UIBrokerImpl::ShowAssociateAccount(scoped_refptr<Device> device) {
  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
      fast_pair_presenter_->ShowAssociateAccount(
          device,
          base::BindRepeating(&UIBrokerImpl::NotifyAssociateAccountAction,
                              weak_pointer_factory_.GetWeakPtr(), device));
      break;
    case Protocol::kFastPairSubsequent:
      NOTREACHED();
  }
}

void UIBrokerImpl::ShowInstallCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      fast_pair_presenter_->ShowInstallCompanionApp(
          device,
          base::BindRepeating(&UIBrokerImpl::NotifyCompanionAppAction,
                              weak_pointer_factory_.GetWeakPtr(), device));
      break;
  }
}

void UIBrokerImpl::ShowLaunchCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  switch (device->protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      fast_pair_presenter_->ShowLaunchCompanionApp(
          device,
          base::BindRepeating(&UIBrokerImpl::NotifyCompanionAppAction,
                              weak_pointer_factory_.GetWeakPtr(), device));
      break;
  }
}

void UIBrokerImpl::ShowPasskey(std::u16string device_name, uint32_t passkey) {
  fast_pair_presenter_->ShowPasskey(device_name, passkey);
}

void UIBrokerImpl::RemoveNotifications() {
  fast_pair_presenter_->RemoveNotifications();
}

void UIBrokerImpl::ExtendNotification() {
  fast_pair_presenter_->ExtendNotification();
}

void UIBrokerImpl::NotifyDiscoveryAction(scoped_refptr<Device> device,
                                         DiscoveryAction action) {
  for (auto& observer : observers_)
    observer.OnDiscoveryAction(device, action);
}

void UIBrokerImpl::NotifyPairingFailedAction(scoped_refptr<Device> device,
                                             PairingFailedAction action) {
  for (auto& observer : observers_)
    observer.OnPairingFailureAction(device, action);
}

void UIBrokerImpl::NotifyAssociateAccountAction(scoped_refptr<Device> device,
                                                AssociateAccountAction action) {
  for (auto& observer : observers_)
    observer.OnAssociateAccountAction(device, action);
}

void UIBrokerImpl::NotifyCompanionAppAction(scoped_refptr<Device> device,
                                            CompanionAppAction action) {
  for (auto& observer : observers_)
    observer.OnCompanionAppAction(device, action);
}

}  // namespace quick_pair
}  // namespace ash
