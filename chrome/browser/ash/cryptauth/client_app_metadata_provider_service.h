// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_H_

#include <list>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/client_app_metadata_provider.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace instance_id {
class InstanceID;
class InstanceIDProfileService;
}  // namespace instance_id

namespace ash {

class NetworkStateHandler;

// Concrete ClientAppMetadataProvider implementation, which lazily computes the
// ClientAppMetadata when GetClientAppMetadata() is called. Once the
// ClientAppMetadata has been successfully computed once, the same instance is
// returned to clients on subsequent calls.
class ClientAppMetadataProviderService
    : public device_sync::ClientAppMetadataProvider,
      public KeyedService {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ClientAppMetadataProviderService(
      PrefService* pref_service,
      NetworkStateHandler* network_state_handler,
      instance_id::InstanceIDProfileService* instance_id_profile_service);

  ClientAppMetadataProviderService(const ClientAppMetadataProviderService&) =
      delete;
  ClientAppMetadataProviderService& operator=(
      const ClientAppMetadataProviderService&) = delete;

  ~ClientAppMetadataProviderService() override;

  // device_sync::ClientAppMetadataProvider:
  void GetClientAppMetadata(const std::string& gcm_registration_id,
                            GetMetadataCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClientAppMetadataProviderServiceTest,
                           VersionCodeToInt64);

  // Converts a version code string to an integer. The version code is expected
  // to be in the format "XXX.X.XXXX.XXX". It is possible that the final set of
  // numbers has fewer than three digits, so this function adds extra 0's if
  // necessary.
  //   Example: "74.0.3690.1" ==> 7403690001
  //   Example: "100.0.1234.567" ==> 10001234567
  static int64_t ConvertVersionCodeToInt64(const std::string& version_code_str);

  // KeyedService:
  void Shutdown() override;

  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void OnHardwareInfoFetched(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      base::SysInfo::HardwareInfo hardware_info);
  void OnInstanceIdFetched(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      const base::SysInfo::HardwareInfo& hardware_info,
      const std::string& instance_id);
  void OnInstanceIdTokenFetched(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      const base::SysInfo::HardwareInfo& hardware_info,
      const std::string& instance_id,
      const std::string& token,
      instance_id::InstanceID::Result result);
  void OnInstanceIdDeleted(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      const base::SysInfo::HardwareInfo& hardware_info,
      instance_id::InstanceID::Result result);

  instance_id::InstanceID* GetInstanceId();
  int64_t SoftwareVersionCodeAsInt64();
  void InvokePendingCallbacks();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<instance_id::InstanceIDProfileService> instance_id_profile_service_;

  bool instance_id_recreated_ = false;
  std::optional<std::string> pending_gcm_registration_id_;
  std::optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
  std::list<GetMetadataCallback> pending_callbacks_;
  base::WeakPtrFactory<ClientAppMetadataProviderService> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_H_
