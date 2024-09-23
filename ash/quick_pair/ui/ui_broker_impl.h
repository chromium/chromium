// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_UI_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_UI_UI_BROKER_IMPL_H_

#include <memory>

#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

class FastPairPresenter;
class Device;

class UIBrokerImpl final : public UIBroker {
 public:
  UIBrokerImpl();
  UIBrokerImpl(const UIBrokerImpl&) = delete;
  UIBrokerImpl& operator=(const UIBrokerImpl&) = delete;
  ~UIBrokerImpl() override;

  // UIBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void ShowDiscovery(scoped_refptr<Device> device) override;
  void ShowPairing(scoped_refptr<Device> device) override;
  void ShowPairingFailed(scoped_refptr<Device> device) override;
  void ShowAssociateAccount(scoped_refptr<Device> device) override;
  void ShowInstallCompanionApp(scoped_refptr<Device> device) override;
  void ShowLaunchCompanionApp(scoped_refptr<Device> device) override;
  void ShowPasskey(std::u16string device_name, uint32_t passkey) override;
  void RemoveNotifications() override;
  void ExtendNotification() override;

 private:
  void NotifyDiscoveryAction(scoped_refptr<Device> device,
                             DiscoveryAction action);
  void NotifyPairingFailedAction(scoped_refptr<Device> device,
                                 PairingFailedAction action);
  void NotifyAssociateAccountAction(scoped_refptr<Device> device,
                                    AssociateAccountAction action);
  void NotifyCompanionAppAction(scoped_refptr<Device> device,
                                CompanionAppAction action);

  std::unique_ptr<FastPairPresenter> fast_pair_presenter_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<UIBrokerImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_UI_BROKER_IMPL_H_
