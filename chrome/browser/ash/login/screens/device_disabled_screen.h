// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

namespace ash {

class DeviceDisabledScreenView;

// Screen informing the user that the device has been disabled by its owner.
class DeviceDisabledScreen
    : public BaseScreen,
      public system::DeviceDisablingManager::Observer,
      public policy::DeviceRestrictionScheduleController::Observer {
 public:
  // `device_restriction_schedule_controller` must be non-null and must outlive
  // `this`.
  DeviceDisabledScreen(policy::DeviceRestrictionScheduleController*
                           device_restriction_schedule_controller,
                       base::WeakPtr<DeviceDisabledScreenView> view);

  DeviceDisabledScreen(const DeviceDisabledScreen&) = delete;
  DeviceDisabledScreen& operator=(const DeviceDisabledScreen&) = delete;

  ~DeviceDisabledScreen() override;

  // system::DeviceDisablingManager::Observer:
  void OnDisabledMessageChanged(const std::string& disabled_message) override;
  void OnLocationTrackingEnabledChanged(
      bool location_tracking_enabled) override;

  // policy::DeviceRestrictionScheduleController::Observer:
  void OnRestrictionScheduleMessageChanged() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  const raw_ref<policy::DeviceRestrictionScheduleController>
      device_restriction_schedule_controller_;
  base::WeakPtr<DeviceDisabledScreenView> view_;
  base::ScopedObservation<system::DeviceDisablingManager, DeviceDisabledScreen>
      device_disabling_manager_observation_{this};
  base::ScopedObservation<policy::DeviceRestrictionScheduleController,
                          policy::DeviceRestrictionScheduleController::Observer>
      restriction_schedule_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEVICE_DISABLED_SCREEN_H_
