// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_UI_MOCK_UI_BROKER_H_
#define ASH_QUICK_PAIR_UI_MOCK_UI_BROKER_H_

#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/observer_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class Device;

class MockUIBroker : public UIBroker {
 public:
  MockUIBroker();
  MockUIBroker(const MockUIBroker&) = delete;
  MockUIBroker& operator=(const MockUIBroker&) = delete;
  ~MockUIBroker() override;

  MOCK_METHOD(void, ShowDiscovery, (scoped_refptr<Device>), (override));
  MOCK_METHOD(void, ShowPairing, (scoped_refptr<Device>), (override));
  MOCK_METHOD(void, ShowPairingFailed, (scoped_refptr<Device>), (override));
  MOCK_METHOD(void, ShowAssociateAccount, (scoped_refptr<Device>), (override));
  MOCK_METHOD(void,
              ShowInstallCompanionApp,
              (scoped_refptr<Device>),
              (override));
  MOCK_METHOD(void,
              ShowLaunchCompanionApp,
              (scoped_refptr<Device>),
              (override));
  MOCK_METHOD(void, ShowPasskey, (std::u16string, uint32_t), (override));
  MOCK_METHOD(void, RemoveNotifications, (), (override));
  MOCK_METHOD(void, ExtendNotification, (), (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyDiscoveryAction(scoped_refptr<Device> device,
                             DiscoveryAction action);
  void NotifyCompanionAppAction(scoped_refptr<Device> device,
                                CompanionAppAction action);
  void NotifyPairingFailedAction(scoped_refptr<Device> device,
                                 PairingFailedAction action);
  void NotifyAssociateAccountAction(scoped_refptr<Device> device,
                                    AssociateAccountAction action);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_MOCK_UI_BROKER_H_
