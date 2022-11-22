// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"

namespace ash {

class DeviceDisabledScreenView;

// Screen informing the user that the device has been disabled by its owner.
class DeviceDisabledScreen : public BaseScreen,
                             public system::DeviceDisablingManager::Observer {
 public:
  explicit DeviceDisabledScreen(base::WeakPtr<DeviceDisabledScreenView> view);

  DeviceDisabledScreen(const DeviceDisabledScreen&) = delete;
  DeviceDisabledScreen& operator=(const DeviceDisabledScreen&) = delete;

  ~DeviceDisabledScreen() override;

  // system::DeviceDisablingManager::Observer:
  void OnDisabledMessageChanged(const std::string& disabled_message) override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  base::WeakPtr<DeviceDisabledScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
