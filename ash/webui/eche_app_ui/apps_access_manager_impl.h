// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_

#include <ostream>

#include "ash/webui/eche_app_ui/apps_access_manager.h"
#include "ash/webui/eche_app_ui/eche_connector.h"
#include "ash/webui/eche_app_ui/eche_message_receiver.h"
#include "ash/webui/eche_app_ui/feature_status.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;
using ConnectionStatus = secure_channel::ConnectionManager::Status;

// Implements AppsAccessManager by persisting the last-known
// apps access value to user prefs.
class AppsAccessManagerImpl
    : public AppsAccessManager,
      public EcheMessageReceiver::Observer,
      public FeatureStatusProvider::Observer,
      public secure_channel::ConnectionManager::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit AppsAccessManagerImpl(
      EcheConnector* eche_connector,
      EcheMessageReceiver* message_receiver,
      FeatureStatusProvider* feature_status_provider,
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager);

  ~AppsAccessManagerImpl() override;

 private:
  friend class AppsAccessManagerImplTest;
  // AppsAccessManager:
  AccessStatus GetAccessStatus() const override;
  void SetAccessStatusInternal(AccessStatus access_status) override;
  void OnSetupRequested() override;
  void NotifyAppsAccessCanceled() override;

  // EcheMessageReceiver::Observer:
  void OnGetAppsAccessStateResponseReceived(
      proto::GetAppsAccessStateResponse apps_access_state_response) override;
  void OnSendAppsSetupResponseReceived(
      proto::SendAppsSetupResponse apps_setup_response) override;
  void OnStatusChange(proto::StatusChangeType status_change_type) override {}
  void OnAppPolicyStateChange(
      proto::AppStreamingPolicy app_policy_state) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void AttemptAppsAccessStateRequest();
  void GetAppsAccessStateRequest();
  void SendShowAppsAccessSetupRequest();
  void UpdateFeatureEnabledState(AccessStatus previous_access_status,
                                 AccessStatus current_access_status);
  bool IsWaitingForAccessToInitiallyEnableApps() const;
  bool IsPhoneHubEnabled() const;
  bool IsEligibleForOnboarding(FeatureStatus feature_status) const;
  void UpdateSetupOperationState();
  void LogAppsSetupResponse(proto::Result apps_setup_result);

  AccessStatus ComputeAppsAccessState();

  FeatureStatus current_feature_status_;
  ConnectionStatus current_connection_status_;
  raw_ptr<EcheConnector> eche_connector_;
  raw_ptr<EcheMessageReceiver> message_receiver_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  bool initialized_ = false;
  proto::AppStreamingPolicy current_app_policy_state_ =
      proto::AppStreamingPolicy::APP_POLICY_UNKNOWN;
  proto::AppsAccessState current_apps_access_state_ =
      proto::AppsAccessState::ACCESS_UNKNOWN;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_IMPL_H_
