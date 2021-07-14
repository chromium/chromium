// Copyright 2021 The Chromium Authors. All rights reserved.
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

struct Device;

class MockUIBroker : public UIBroker {
 public:
  MockUIBroker();
  MockUIBroker(const MockUIBroker&) = delete;
  MockUIBroker& operator=(const MockUIBroker&) = delete;
  ~MockUIBroker() override;

  MOCK_METHOD(void, ShowDiscovery, (const Device&), (override));
  MOCK_METHOD(void, ShowPairing, (const Device&), (override));
  MOCK_METHOD(void, ShowPairingFailed, (const Device&), (override));
  MOCK_METHOD(void, ShowAssociateAccount, (const Device&), (override));
  MOCK_METHOD(void, ShowCompanionApp, (const Device&), (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyDiscoveryAction(const Device& device, DiscoveryAction action);
  void NotifyCompanionAppAction(const Device& device,
                                CompanionAppAction action);
  void NotifyPairingFailedAction(const Device& device,
                                 PairingFailedAction action);
  void NotifyAssociateAccountAction(const Device& device,
                                    AssociateAccountAction action);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_UI_MOCK_UI_BROKER_H_
