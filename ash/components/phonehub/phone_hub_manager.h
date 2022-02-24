// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_

namespace ash {
namespace phonehub {

class BrowserTabsModelProvider;
class CameraRollManager;
class ConnectionScheduler;
class DoNotDisturbController;
class FeatureStatusProvider;
class FindMyDeviceController;
class MultideviceFeatureAccessManager;
class NotificationInteractionHandler;
class NotificationManager;
class OnboardingUiTracker;
class PhoneModel;
class RecentAppsInteractionHandler;
class ScreenLockManager;
class TetherController;
class UserActionRecorder;

// Responsible for the core logic of the Phone Hub feature and exposes
// interfaces via its public API. This class is intended to be a singleton.
class PhoneHubManager {
 public:
  virtual ~PhoneHubManager() = default;

  PhoneHubManager(const PhoneHubManager&) = delete;
  PhoneHubManager& operator=(const PhoneHubManager&) = delete;

  // Getters for sub-elements.
  virtual BrowserTabsModelProvider* GetBrowserTabsModelProvider() = 0;
  virtual CameraRollManager* GetCameraRollManager() = 0;
  virtual ConnectionScheduler* GetConnectionScheduler() = 0;
  virtual DoNotDisturbController* GetDoNotDisturbController() = 0;
  virtual FeatureStatusProvider* GetFeatureStatusProvider() = 0;
  virtual FindMyDeviceController* GetFindMyDeviceController() = 0;
  virtual MultideviceFeatureAccessManager*
  GetMultideviceFeatureAccessManager() = 0;
  virtual NotificationInteractionHandler*
  GetNotificationInteractionHandler() = 0;
  virtual NotificationManager* GetNotificationManager() = 0;
  virtual OnboardingUiTracker* GetOnboardingUiTracker() = 0;
  virtual PhoneModel* GetPhoneModel() = 0;
  virtual RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() = 0;
  virtual ScreenLockManager* GetScreenLockManager() = 0;
  virtual TetherController* GetTetherController() = 0;
  virtual UserActionRecorder* GetUserActionRecorder() = 0;

 protected:
  PhoneHubManager() = default;
};

}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
using ::ash::phonehub::PhoneHubManager;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
