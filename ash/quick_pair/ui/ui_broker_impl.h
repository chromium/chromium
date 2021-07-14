// Copyright 2021 The Chromium Authors. All rights reserved.
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

class COMPONENT_EXPORT(QUICK_PAIR_UI) UIBrokerImpl : public UIBroker {
 public:
  UIBrokerImpl();
  UIBrokerImpl(const UIBrokerImpl&) = delete;
  UIBrokerImpl& operator=(const UIBrokerImpl&) = delete;
  ~UIBrokerImpl() final;

  // UIBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void ShowDiscovery(const Device& device) override;
  void ShowPairing(const Device& device) override;
  void ShowPairingFailed(const Device& device) override;
  void ShowAssociateAccount(const Device& device) override;
  void ShowCompanionApp(const Device& device) override;

 private:
  void NotifyDiscoveryAction(const Device& device, DiscoveryAction action);
  void NotifyPairingFailedAction(const Device& device,
                                 PairingFailedAction action);
  void NotifyAssociateAccountAction(const Device& device,
                                    AssociateAccountAction action);
  void NotifyCompanionAppAction(const Device& device,
                                CompanionAppAction action);

  std::unique_ptr<FastPairPresenter> fast_pair_presenter_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<UIBrokerImpl> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_UI_BROKER_IMPL_H_
