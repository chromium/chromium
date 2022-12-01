// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_onc_util.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace rollback_network_config {

namespace {

static const char kDeviceUserHash[] = "";

static const char kDomain[] = "domain";
static const char kDomainId[] = "domain_id";

static const char kOpenWiFi[] = R"({
  "GUID": "open-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "None"
  }
})";
static const char kWpaPskWiFi[] = R"({
  "GUID": "wpa-psk-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "WPA-PSK",
    "Passphrase": "wpa-psk-network-passphrase"
  }
})";
static const char kWpaPskWiFiNoPass[] = R"({
  "GUID": "wpa-psk-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "WPA-PSK",
    "Passphrase": ""
  }
})";
static const char kWepPskWiFi[] = R"({
  "GUID": "wep-psk-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "WEP-PSK",
    "Passphrase": "wep-psk-network-passphrase"
  }
})";
static const char kPeapWiFi[] = R"({
  "GUID": "peap-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "WPA-EAP",
    "EAP" : {
      "ClientCertType": "None",
      "Identity" : "peap-network-identity",
      "Inner" : "Automatic",
      "Outer" : "PEAP",
      "Password" : "peap-network-password",
      "SaveCredentials" : true
    }
  }
})";
static const char kOpenEthernet[] = R"({
  "GUID": "ethernet-guid",
  "Type": "Ethernet",
  "Name": "Ethernet",
  "Ethernet": {
    "Authentication": "None"
  }
})";
static const char kPeapEthernet[] = R"({
  "GUID": "peap-ethernet-guid",
  "Type": "Ethernet",
  "Name": "Ethernet",
  "Ethernet": {
    "Authentication": "8021X",
    "EAP" : {
        "ClientCertType": "None",
        "Identity" : "peap-ethernet-identity",
        "Inner" : "MSCHAPv2",
        "Outer" : "PEAP",
        "Password" : "peap-ethernet-password",
        "SaveCredentials" : true
    }
  }
})";
static const char kPeapWiFiRecommendedPolicyPart[] = R"({
  "GUID": "peap-network-guid",
  "Type": "WiFi",
  "Name": "WiFi",
  "WiFi": {
    "Security": "WPA-EAP",
    "EAP" : {
      "ClientCertType": "None",
      "Identity" : "peap-network-identity",
      "Inner" : "Automatic",
      "Outer" : "PEAP",
      "Password" : "peap-network-password",
      "Recommended" : ["Identity"],
      "SaveCredentials" : true
    }
  }
})";
static const char kPeapWiFiRecommendedUserPart[] = R"({
  "WiFi": {
    "EAP" : {
      "Identity" : "peap-network-recommended-identity"
    }
  }
})";

TestingPrefServiceSimple* RegisterPrefs(TestingPrefServiceSimple* local_state) {
  device_settings_cache::RegisterPrefs(local_state->registry());
  return local_state;
}

void PrintErrorAndFail(const std::string& error_name) {
  LOG(ERROR) << error_name;
  FAIL();
}

void PrintErrorAndMessageAndFail(const std::string& error_name,
                                 const std::string& error_message) {
  LOG(ERROR) << error_name << " " << error_message;
  FAIL();
}

NetworkStateHandler* network_state_handler() {
  return NetworkHandler::Get()->network_state_handler();
}

ash::ManagedNetworkConfigurationHandler*
managed_network_configuration_handler() {
  return NetworkHandler::Get()->managed_network_configuration_handler();
}

ShillServiceClient* shill_service_client() {
  return ShillServiceClient::Get();
}

const NetworkState* GetNetworkState(const std::string& guid) {
  return network_state_handler()->GetNetworkStateFromGuid(guid);
}

std::string GetServicePath(const std::string& guid) {
  return GetNetworkState(guid)->path();
}

bool NetworkExists(const std::string& guid) {
  const ash::NetworkState* network_state =
      network_state_handler()->GetNetworkStateFromGuid(guid);
  return network_state && network_state->IsInProfile();
}

void SetUpDeviceWideNetworkConfig(const base::Value& config) {
  base::RunLoop run_loop;
  managed_network_configuration_handler()->CreateConfiguration(
      kDeviceUserHash, config,
      base::BindLambdaForTesting(
          [&](const std::string& service_path, const std::string& guid) {
            run_loop.Quit();
          }),
      base::BindOnce(&PrintErrorAndFail));
  run_loop.Run();
}

void SetPropertiesForExistingNetwork(const std::string& guid,
                                     const base::Value& config) {
  base::RunLoop run_loop;
  ASSERT_TRUE(NetworkExists(guid));
  const ash::NetworkState* network_state =
      network_state_handler()->GetNetworkStateFromGuid(guid);
  managed_network_configuration_handler()->SetProperties(
      network_state->path(), config,
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }),
      base::BindOnce(&PrintErrorAndFail));
  run_loop.Run();
}

base::Value GetProperties(const std::string userhash, const std::string& guid) {
  base::RunLoop run_loop;
  base::Value result;
  managed_network_configuration_handler()->GetProperties(
      userhash, GetServicePath(guid),
      base::BindLambdaForTesting([&](const std::string& service_path,
                                     absl::optional<base::Value> properties,
                                     absl::optional<std::string> error) {
        ASSERT_TRUE(properties.has_value());
        result = std::move(*properties);
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

base::Value GetManagedProperties(const std::string userhash,
                                 const std::string& guid) {
  base::RunLoop run_loop;
  base::Value result;
  managed_network_configuration_handler()->GetManagedProperties(
      userhash, GetServicePath(guid),
      base::BindLambdaForTesting([&](const std::string& service_path,
                                     absl::optional<base::Value> properties,
                                     absl::optional<std::string> error) {
        ASSERT_TRUE(properties.has_value());
        result = std::move(*properties);
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

std::string GetPskPassphrase(const std::string& guid) {
  base::RunLoop run_loop;
  std::string result;
  shill_service_client()->GetWiFiPassphrase(
      dbus::ObjectPath(GetServicePath(guid)),
      base::BindLambdaForTesting([&](const std::string& password) {
        result = password;
        run_loop.Quit();
      }),
      base::BindOnce(&PrintErrorAndMessageAndFail));
  run_loop.Run();
  return result;
}

std::string GetEapPassphrase(const std::string& guid) {
  base::RunLoop run_loop;
  std::string result;
  shill_service_client()->GetEapPassphrase(
      dbus::ObjectPath(GetServicePath(guid)),
      base::BindLambdaForTesting([&](const std::string& password) {
        result = password;
        run_loop.Quit();
      }),
      base::BindOnce(&PrintErrorAndMessageAndFail));
  run_loop.Run();
  return result;
}

void RemoveNetwork(const std::string& guid) {
  const ash::NetworkState* network_state =
      network_state_handler()->GetNetworkStateFromGuid(guid);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::Value result;
  managed_network_configuration_handler()->RemoveConfiguration(
      network_state->path(),
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }),
      base::BindOnce(&PrintErrorAndFail));
  run_loop.Run();
}

}  // namespace

class RollbackNetworkConfigTest : public testing::Test {
 public:
  RollbackNetworkConfigTest() {
    network_handler_test_helper_.AddDefaultProfiles();
    network_handler_test_helper_.ResetDevicesAndServices();

    RegisterAndSetUpPrefs();

    rollback_network_config_ = std::make_unique<RollbackNetworkConfig>();
  }

  ~RollbackNetworkConfigTest() override { rollback_network_config_.reset(); }

  void RegisterAndSetUpPrefs() {
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    network_handler_test_helper_.RegisterPrefs(user_prefs_.registry(),
                                               local_state_.registry());

    network_handler_test_helper_.InitializePrefs(&user_prefs_, &local_state_);
  }

  void SetUp() override { SetEmptyDevicePolicy(); }

  void SetEmptyDevicePolicy() {
    managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, kDeviceUserHash,
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());
    task_environment_.RunUntilIdle();
  }

  void SetUpDevicePolicyNetworkConfig(const base::Value& network_config) {
    base::ListValue network_configs_onc;
    base::DictionaryValue global_network_config;
    network_configs_onc.Append(network_config.Clone());
    managed_network_configuration_handler()->SetPolicy(
        onc::ONC_SOURCE_DEVICE_POLICY, kDeviceUserHash, network_configs_onc,
        global_network_config);
    task_environment_.RunUntilIdle();
  }

  std::string Export() {
    base::RunLoop run_loop;
    std::string result;
    rollback_network_config_->RollbackConfigExport(
        base::BindLambdaForTesting([&](const std::string& config) {
          result = config;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void Import(const std::string& config) {
    base::RunLoop run_loop;
    rollback_network_config_->RollbackConfigImport(
        config, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Exports network data, resets all network configuration including policies
  // and imports network data.
  void SimulateRollback() {
    const std::string config = Export();

    SetEmptyDevicePolicy();
    network_handler_test_helper_.ResetDevicesAndServices();

    Import(config);
  }

  void TakeOwnershipAsConsumer() {
    rollback_network_config_->fake_ownership_taken_for_testing();
    task_environment_.RunUntilIdle();
  }

  void TakeOwnershipEnrolled() {
    scoped_stub_install_attributes_.Get()->SetCloudManaged(kDomain, kDomainId);
    rollback_network_config_->fake_ownership_taken_for_testing();
    task_environment_.RunUntilIdle();
  }

 private:
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_keys_{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};

  TestingPrefServiceSimple local_state_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  NetworkHandlerTestHelper network_handler_test_helper_;
  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ScopedTestDeviceSettingsService scoped_device_settings_;
  ScopedTestCrosSettings scoped_cros_settings_{RegisterPrefs(&local_state_)};
  policy::DevicePolicyBuilder device_policy_;

  std::unique_ptr<RollbackNetworkConfig> rollback_network_config_;
};

TEST_F(RollbackNetworkConfigTest, OpenWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));
  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kSecurityNone);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevice);
}

TEST_F(RollbackNetworkConfigTest, PolicyOpenWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();
  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));
  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kSecurityNone);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, WpaPskWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kWpaPskWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetPskPassphrase(guid), OncWiFiGetPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWPA_PSK);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevice);
}

TEST_F(RollbackNetworkConfigTest, WpaPskWiFiWithoutPasswordIsPreserved) {
  base::Value network = *base::JSONReader::Read(kWpaPskWiFiNoPass);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetPskPassphrase(guid), OncWiFiGetPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWPA_PSK);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevice);
}

TEST_F(RollbackNetworkConfigTest, PolicyWpaPskWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kWpaPskWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  EXPECT_EQ(GetPskPassphrase(guid), OncWiFiGetPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWPA_PSK);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, WepPskWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kWepPskWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));
  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWEP_PSK);
  EXPECT_EQ(GetPskPassphrase(guid), OncWiFiGetPassword(network));
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevice);
}

TEST_F(RollbackNetworkConfigTest, PolicyWepPskWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kWepPskWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));
  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWEP_PSK);
  EXPECT_EQ(GetPskPassphrase(guid), OncWiFiGetPassword(network));
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, PeapWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kPeapWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetEapPassphrase(guid), OncGetEapPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWPA_EAP);
  EXPECT_EQ(OncGetEapIdentity(properties), OncGetEapIdentity(network));
  EXPECT_EQ(OncGetEapInner(properties), OncGetEapInner(network));
  EXPECT_EQ(OncGetEapOuter(properties), OncGetEapOuter(network));
  EXPECT_EQ(OncGetEapSaveCredentials(properties),
            OncGetEapSaveCredentials(network));
  EXPECT_TRUE(OncIsEapWithoutClientCertificate(properties));
}

TEST_F(RollbackNetworkConfigTest, PolicyPeapWiFiIsPreserved) {
  base::Value network = *base::JSONReader::Read(kPeapWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetEapPassphrase(guid), OncGetEapPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kWPA_EAP);
  EXPECT_EQ(OncGetEapIdentity(properties), OncGetEapIdentity(network));
  EXPECT_EQ(OncGetEapInner(properties), OncGetEapInner(network));
  EXPECT_EQ(OncGetEapOuter(properties), OncGetEapOuter(network));
  EXPECT_EQ(OncGetEapSaveCredentials(properties),
            OncGetEapSaveCredentials(network));
  EXPECT_TRUE(OncIsEapWithoutClientCertificate(properties));
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, OpenEthernetIsPreserved) {
  base::Value network = *base::JSONReader::Read(kOpenEthernet);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kEthernet);
  EXPECT_TRUE(OncHasNoSecurity(properties));
}

TEST_F(RollbackNetworkConfigTest, PolicyOpenEthernetIsPreserved) {
  base::Value network = *base::JSONReader::Read(kOpenEthernet);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kEthernet);
  EXPECT_TRUE(OncHasNoSecurity(properties));
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, PeapEthernetIsPreserved) {
  base::Value network = *base::JSONReader::Read(kPeapEthernet);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetEapPassphrase(guid), OncGetEapPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kEthernet);
  EXPECT_EQ(OncEthernetGetAuthentication(properties), onc::ethernet::k8021X);
  EXPECT_EQ(OncGetEapIdentity(properties), OncGetEapIdentity(network));
  EXPECT_EQ(OncGetEapInner(properties), OncGetEapInner(network));
  EXPECT_EQ(OncGetEapOuter(properties), OncGetEapOuter(network));
  EXPECT_EQ(OncGetEapSaveCredentials(properties),
            OncGetEapSaveCredentials(network));
  EXPECT_TRUE(OncIsEapWithoutClientCertificate(properties));
}

TEST_F(RollbackNetworkConfigTest, PolicyPeapEthernetIsPreserved) {
  base::Value network = *base::JSONReader::Read(kPeapEthernet);
  SetUpDevicePolicyNetworkConfig(network);
  SimulateRollback();
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  ASSERT_TRUE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));

  EXPECT_EQ(GetEapPassphrase(guid), OncGetEapPassword(network));

  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kEthernet);
  EXPECT_EQ(OncEthernetGetAuthentication(properties), onc::ethernet::k8021X);
  EXPECT_EQ(OncGetEapIdentity(properties), OncGetEapIdentity(network));
  EXPECT_EQ(OncGetEapInner(properties), OncGetEapInner(network));
  EXPECT_EQ(OncGetEapOuter(properties), OncGetEapOuter(network));
  EXPECT_EQ(OncGetEapSaveCredentials(properties),
            OncGetEapSaveCredentials(network));
  EXPECT_TRUE(OncIsEapWithoutClientCertificate(properties));
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevicePolicy);
}

TEST_F(RollbackNetworkConfigTest, ConsumerOwnershipKeepsDeviceNetworks) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();
  TakeOwnershipAsConsumer();

  ASSERT_TRUE(NetworkExists(guid));
  base::Value properties = GetProperties(kDeviceUserHash, guid);
  ASSERT_EQ(GetStringValue(properties, onc::network_config::kType),
            onc::network_type::kWiFi);
  EXPECT_EQ(OncWiFiGetSecurity(properties), onc::wifi::kSecurityNone);
  EXPECT_EQ(GetStringValue(properties, onc::network_config::kSource),
            onc::network_config::kSourceDevice);
}

TEST_F(RollbackNetworkConfigTest, ConsumerOwnershipDeletesPolicyNetworksWiFi) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  EXPECT_TRUE(NetworkExists(guid));
  SimulateRollback();
  EXPECT_TRUE(NetworkExists(guid));
  TakeOwnershipAsConsumer();
  EXPECT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest,
       ConsumerOwnershipDeletesPolicyNetworksEthernet) {
  base::Value network = *base::JSONReader::Read(kPeapEthernet);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  EXPECT_TRUE(NetworkExists(guid));
  SimulateRollback();
  EXPECT_TRUE(NetworkExists(guid));
  TakeOwnershipAsConsumer();

  // Essential properties of the configuration may be kept, but at least
  // identity and password should be deleted.
  if (NetworkExists(guid)) {
    base::Value properties = GetProperties(kDeviceUserHash, guid);
    // Shill may only delete the eap part and keep the authentication type, that
    // is okay as well.
    if (OncIsEap(properties) && OncHasEapConfiguration(properties)) {
      EXPECT_EQ(GetEapPassphrase(guid), "");
      EXPECT_EQ(OncGetEapIdentity(properties), "");
    }
  }
}

TEST_F(RollbackNetworkConfigTest, EnrollmentToSameKeepsPolicyNetworks) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  EXPECT_TRUE(NetworkExists(guid));
  SimulateRollback();
  EXPECT_TRUE(NetworkExists(guid));
  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  base::Value managed_properties = GetManagedProperties(kDeviceUserHash, guid);
  ManagedOncCollapseToUiData(&managed_properties);

  EXPECT_TRUE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest, EnrollmentToDifferentDeletesPolicyNetworks) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  EXPECT_TRUE(NetworkExists(guid));
  SimulateRollback();
  EXPECT_TRUE(NetworkExists(guid));
  TakeOwnershipEnrolled();
  SetEmptyDevicePolicy();

  EXPECT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest, ExactlyRecommendedValuesPreserved) {
  base::Value policy_config =
      *base::JSONReader::Read(kPeapWiFiRecommendedPolicyPart);
  base::Value user_config =
      *base::JSONReader::Read(kPeapWiFiRecommendedUserPart);
  const std::string& guid =
      GetStringValue(policy_config, onc::network_config::kGUID);

  SetUpDevicePolicyNetworkConfig(policy_config);
  SetPropertiesForExistingNetwork(guid, user_config);

  SimulateRollback();
  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(policy_config);

  base::Value managed_properties = GetManagedProperties(kDeviceUserHash, guid);
  ManagedOncCollapseToUiData(&managed_properties);
  EXPECT_EQ(managed_properties, user_config);
}

TEST_F(RollbackNetworkConfigTest,
       ConsumerOwnershipDeletesPolicyNetworkWithRecommendFields) {
  base::Value policy_config =
      *base::JSONReader::Read(kPeapWiFiRecommendedPolicyPart);
  base::Value user_config =
      *base::JSONReader::Read(kPeapWiFiRecommendedUserPart);
  const std::string& guid =
      GetStringValue(policy_config, onc::network_config::kGUID);

  SetUpDevicePolicyNetworkConfig(policy_config);
  SetPropertiesForExistingNetwork(guid, user_config);

  SimulateRollback();
  TakeOwnershipAsConsumer();
  EXPECT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest,
       DeleteDeviceNetworkBetweenImportAndConsumerOwnership) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();
  RemoveNetwork(guid);

  ASSERT_FALSE(NetworkExists(guid));
  TakeOwnershipAsConsumer();
  ASSERT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest,
       DeleteDeviceNetworkBetweenImportAndEnrollment) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDeviceWideNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();
  RemoveNetwork(guid);

  ASSERT_FALSE(NetworkExists(guid));
  TakeOwnershipEnrolled();
  ASSERT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest,
       DeletePolicyNetworkBetweenImportAndConsumerOwnership) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  RemoveNetwork(guid);

  ASSERT_FALSE(NetworkExists(guid));

  TakeOwnershipAsConsumer();

  ASSERT_FALSE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest,
       DeletePolicyNetworkBetweenImportAndEnrollment) {
  base::Value network = *base::JSONReader::Read(kOpenWiFi);
  SetUpDevicePolicyNetworkConfig(network);
  const std::string& guid = GetStringValue(network, onc::network_config::kGUID);

  SimulateRollback();

  RemoveNetwork(guid);

  ASSERT_FALSE(NetworkExists(guid));

  TakeOwnershipEnrolled();
  SetUpDevicePolicyNetworkConfig(network);

  ASSERT_TRUE(NetworkExists(guid));
}

TEST_F(RollbackNetworkConfigTest, EmptyImport) {
  const std::string empty_config = "{\"NetworkConfigurations\":[]}";
  Import(empty_config);
}

TEST_F(RollbackNetworkConfigTest, EmptyExport) {
  EXPECT_EQ(Export(), "{\"NetworkConfigurations\":[]}");
}

TEST_F(RollbackNetworkConfigTest, MultipleNetworks) {
  base::Value peap_wifi = *base::JSONReader::Read(kPeapWiFi);
  SetUpDeviceWideNetworkConfig(peap_wifi);
  const std::string& peap_wifi_guid =
      GetStringValue(peap_wifi, onc::network_config::kGUID);

  base::Value open_wifi = *base::JSONReader::Read(kOpenWiFi);
  SetUpDeviceWideNetworkConfig(open_wifi);
  const std::string& open_wifi_guid =
      GetStringValue(open_wifi, onc::network_config::kGUID);

  base::Value eap_ethernet = *base::JSONReader::Read(kPeapEthernet);
  SetUpDeviceWideNetworkConfig(eap_ethernet);
  const std::string& eap_ethernet_guid =
      GetStringValue(eap_ethernet, onc::network_config::kGUID);

  SimulateRollback();
  TakeOwnershipEnrolled();
  SetEmptyDevicePolicy();

  EXPECT_TRUE(NetworkExists(peap_wifi_guid));
  EXPECT_TRUE(NetworkExists(open_wifi_guid));
  EXPECT_TRUE(NetworkExists(eap_ethernet_guid));
}

}  // namespace rollback_network_config
}  // namespace ash
