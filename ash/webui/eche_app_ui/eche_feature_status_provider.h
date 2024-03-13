// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {

namespace phonehub {
class PhoneHubManager;
}

namespace eche_app {

class EcheConnectionStatusHandler;

// FeatureStatusProvider implementation which observes PhoneHub's state, then
// layers in Eche's state.
class EcheFeatureStatusProvider
    : public FeatureStatusProvider,
      public phonehub::FeatureStatusProvider::Observer,
      public secure_channel::ConnectionManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  EcheFeatureStatusProvider(
      phonehub::PhoneHubManager* phone_hub_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager,
      EcheConnectionStatusHandler* eche_connection_status_handler);
  ~EcheFeatureStatusProvider() override;

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  void UpdateStatus();
  FeatureStatus ComputeStatus();

  // phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  raw_ptr<phonehub::FeatureStatusProvider> phone_hub_feature_status_provider_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<EcheConnectionStatusHandler, DanglingUntriaged>
      eche_connection_status_handler_;
  phonehub::FeatureStatus current_phone_hub_feature_status_;
  std::optional<FeatureStatus> status_;
  base::WeakPtrFactory<EcheFeatureStatusProvider> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_
