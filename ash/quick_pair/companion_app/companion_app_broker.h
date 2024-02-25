// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_H_
#define ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_H_

#include "ash/quick_pair/common/device.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

class CompanionAppBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void ShowInstallCompanionApp(scoped_refptr<Device> device) = 0;
    virtual void ShowLaunchCompanionApp(scoped_refptr<Device> device) = 0;
    virtual void OnCompanionAppInstalled(scoped_refptr<Device> device) = 0;
  };

  virtual ~CompanionAppBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Depending on whether the companion app is (a) available and
  // (b) installed, call observers to ShowInstall/ShowLaunchCompanionApp.
  // Returns true if a companion app is available OR installed, causing a
  // notification to be shown; false if no notification was shown.
  virtual bool MaybeShowCompanionAppActions(scoped_refptr<Device> device) = 0;

  // Install the companion app for this device if not already installed.
  virtual void InstallCompanionApp(scoped_refptr<Device> device) = 0;

  // Launch an already-installed companion app.
  virtual void LaunchCompanionApp(scoped_refptr<Device> device) = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_BROKER_H_
