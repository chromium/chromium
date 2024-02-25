// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMPANION_APP_MOCK_COMPANION_APP_BROKER_H_
#define ASH_QUICK_PAIR_COMPANION_APP_MOCK_COMPANION_APP_BROKER_H_

#include "ash/quick_pair/companion_app/companion_app_broker.h"

#include "base/observer_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockCompanionAppBroker : public CompanionAppBroker {
 public:
  MockCompanionAppBroker();
  MockCompanionAppBroker(const MockCompanionAppBroker&) = delete;
  MockCompanionAppBroker& operator=(const MockCompanionAppBroker&) = delete;
  ~MockCompanionAppBroker() override;

  // CompanionAppBroker:
  MOCK_METHOD(bool,
              MaybeShowCompanionAppActions,
              (scoped_refptr<Device>),
              (override));
  MOCK_METHOD(void, InstallCompanionApp, (scoped_refptr<Device>), (override));
  MOCK_METHOD(void, LaunchCompanionApp, (scoped_refptr<Device>), (override));

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMPANION_APP_MOCK_COMPANION_APP_BROKER_H_
