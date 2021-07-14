// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_UI_BROKER_H_
#define ASH_QUICK_PAIR_UI_UI_BROKER_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/ui/actions.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

// The UIBroker is the entry point for the UI component in the Quick Pair
// system. It is responsible for brokering the 'show UI' calls to the correct
// Presenter implementation, and exposing user actions taken on that UI.
class COMPONENT_EXPORT(QUICK_PAIR_UI) UIBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDiscoveryAction(const Device& device,
                                   DiscoveryAction action) = 0;
    virtual void OnCompanionAppAction(const Device& device,
                                      CompanionAppAction action) = 0;
    virtual void OnPairingFailureAction(const Device& device,
                                        PairingFailedAction action) = 0;
    virtual void OnAssociateAccountAction(const Device& device,
                                          AssociateAccountAction action) = 0;
  };

  virtual ~UIBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void ShowDiscovery(const Device& device) = 0;
  virtual void ShowPairing(const Device& device) = 0;
  virtual void ShowPairingFailed(const Device& device) = 0;
  virtual void ShowAssociateAccount(const Device& device) = 0;
  virtual void ShowCompanionApp(const Device& device) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_UI_BROKER_H_
