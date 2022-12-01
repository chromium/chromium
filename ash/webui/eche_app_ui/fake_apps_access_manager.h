// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_

#include "ash/webui/eche_app_ui/apps_access_manager.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"

namespace ash::settings {
class MultideviceHandlerTest;
}

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;

class FakeAppsAccessManager : public AppsAccessManager {
 public:
  explicit FakeAppsAccessManager(
      AccessStatus access_status = AccessStatus::kAvailableButNotGranted);
  ~FakeAppsAccessManager() override;

  void SetAppsSetupOperationStatus(AppsAccessSetupOperation::Status new_status);

  using AppsAccessManager::IsSetupOperationInProgress;

  // AppsAccessManager:
  AccessStatus GetAccessStatus() const override;
  void OnSetupRequested() override;

 private:
  friend class ::ash::settings::MultideviceHandlerTest;
  // AppsAccessManager:
  void SetAccessStatusInternal(AccessStatus access_status) override;
  void NotifyAppsAccessCanceled() override;

  AccessStatus access_status_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_APPS_ACCESS_MANAGER_H_
