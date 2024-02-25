// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_IMPL_H_
#define ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_IMPL_H_

#include "ash/quick_pair/companion_app/companion_app_broker.h"

#include "base/observer_list.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

class CompanionAppBrokerImpl : public CompanionAppBroker {
 public:
  CompanionAppBrokerImpl();
  CompanionAppBrokerImpl(const CompanionAppBrokerImpl&) = delete;
  CompanionAppBrokerImpl& operator=(const CompanionAppBrokerImpl&) = delete;
  ~CompanionAppBrokerImpl() override;

  // CompanionAppBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool MaybeShowCompanionAppActions(scoped_refptr<Device> device) override;
  void InstallCompanionApp(scoped_refptr<Device> device) override;
  void LaunchCompanionApp(scoped_refptr<Device> device) override;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_IMPL_H_
