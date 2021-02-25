// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"

namespace chromeos {

class DeviceDisabledScreenView;

// Screen informing the user that the device has been disabled by its owner.
class DeviceDisabledScreen : public BaseScreen,
                             public system::DeviceDisablingManager::Observer {
 public:
  explicit DeviceDisabledScreen(DeviceDisabledScreenView* view);
  ~DeviceDisabledScreen() override;

  // Called when the view is being destroyed. Note that if the Delegate is
  // destroyed first, it must call SetDelegate(nullptr).
  void OnViewDestroyed(DeviceDisabledScreenView* view);

  // system::DeviceDisablingManager::Observer:
  void OnDisabledMessageChanged(const std::string& disabled_message) override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  DeviceDisabledScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisabledScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
