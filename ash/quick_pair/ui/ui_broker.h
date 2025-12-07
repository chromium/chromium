// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_UI_BROKER_H_
#define ASH_QUICK_PAIR_UI_UI_BROKER_H_

#include "ash/quick_pair/ui/actions.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

class Device;

// The UIBroker is the entry point for the UI component in the Quick Pair
// system. It is responsible for brokering the 'show UI' calls to the correct
// Presenter implementation, and exposing user actions taken on that UI.
class UIBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDiscoveryAction(scoped_refptr<Device> device,
                                   DiscoveryAction action) = 0;
    virtual void OnCompanionAppAction(scoped_refptr<Device> device,
                                      CompanionAppAction action) = 0;
    virtual void OnPairingFailureAction(scoped_refptr<Device> device,
                                        PairingFailedAction action) = 0;
    virtual void OnAssociateAccountAction(scoped_refptr<Device> device,
                                          AssociateAccountAction action) = 0;
  };

  virtual ~UIBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void ShowDiscovery(scoped_refptr<Device> device) = 0;
  virtual void ShowPairing(scoped_refptr<Device> device) = 0;
  virtual void ShowPairingFailed(scoped_refptr<Device> device) = 0;
  virtual void ShowAssociateAccount(scoped_refptr<Device> device) = 0;
  virtual void ShowInstallCompanionApp(scoped_refptr<Device> device) = 0;
  virtual void ShowLaunchCompanionApp(scoped_refptr<Device> device) = 0;
  virtual void ShowPasskey(std::u16string device_name, uint32_t passkey) = 0;
  virtual void RemoveNotifications() = 0;
  virtual void ExtendNotification() = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_UI_BROKER_H_
