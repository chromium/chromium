// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
#define ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_

#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// This class waits until a multi-device host phone is verified before enabling
// the Phone Hub feature. This intent to enable the feature is persisted across
// restarts. This class also disables the PhoneHubNotification Multidevice
// feature state when Notification access has been revoked by the phone,
// provided via MultideviceFeatureAccessManager.
class MultideviceSetupStateUpdater
    : public multidevice_setup::MultiDeviceSetupClient::Observer,
      public MultideviceFeatureAccessManager::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  MultideviceSetupStateUpdater(
      PrefService* pref_service,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      MultideviceFeatureAccessManager* multidevice_feature_access_manager);
  ~MultideviceSetupStateUpdater() override;

 private:
  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_state_map) override;

  // MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnCameraRollAccessChanged() override;

  bool IsWaitingForAccessToInitiallyEnableNotifications() const;
  bool IsWaitingForAccessToInitiallyEnableCameraRoll() const;
  void EnablePhoneHubIfAwaitingVerifiedHost();
  void UpdateIsAwaitingVerifiedHost();

  PrefService* pref_service_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  MultideviceFeatureAccessManager* multidevice_feature_access_manager_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_SETUP_STATE_UPDATER_H_
