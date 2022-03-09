// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_

#include <ostream>

#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/webui/eche_app_ui/apps_access_manager.h"
#include "ash/webui/eche_app_ui/eche_connector.h"
#include "ash/webui/eche_app_ui/eche_message_receiver.h"
#include "ash/webui/eche_app_ui/feature_status.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;

// Implements AppsAccessManager by persisting the last-known
// apps access value to user prefs.
class AppsAccessManagerImpl : public AppsAccessManager,
                              public EcheMessageReceiver::Observer,
                              public FeatureStatusProvider::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit AppsAccessManagerImpl(
      EcheConnector* eche_connector,
      EcheMessageReceiver* message_receiver,
      FeatureStatusProvider* feature_status_provider,
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  ~AppsAccessManagerImpl() override;

 private:
  friend class AppsAccessManagerImplTest;
  // AppsAccessManager:
  AccessStatus GetAccessStatus() const override;
  void SetAccessStatusInternal(AccessStatus access_status) override;
  void OnSetupRequested() override;

  // EcheMessageReceiver::Observer:
  void OnGetAppsAccessStateResponseReceived(
      proto::GetAppsAccessStateResponse apps_access_state_response) override;
  void OnSendAppsSetupResponseReceived(
      proto::SendAppsSetupResponse apps_setup_response) override;
  void OnStatusChange(proto::StatusChangeType status_change_type) override {}

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void AttemptAppsAccessStateRequest();
  void GetAppsAccessStateRequest();
  void SendShowAppsAccessSetupRequest();
  void UpdateFeatureEnabledState(AccessStatus access_status);
  bool IsWaitingForAccessToInitiallyEnableApps() const;

  AccessStatus ComputeAppsAccessState(proto::AppsAccessState apps_access_state);

  FeatureStatus current_feature_status_;
  EcheConnector* eche_connector_;
  EcheMessageReceiver* message_receiver_;
  FeatureStatusProvider* feature_status_provider_;
  PrefService* pref_service_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  bool initialized_ = false;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_
