// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/network_health_provider.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_certificate_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace diagnostics {
namespace {

// TODO(https://crbug.com/1164001): remove when network_config is moved to ash.
namespace network_config = ::chromeos::network_config;

void ValidateManagedPropertiesSet(
    const NetworkPropertiesMap& network_properties_map,
    const std::string& guid) {
  EXPECT_TRUE(base::Contains(network_properties_map, guid));
  auto network_props_iter_wifi = network_properties_map.find(guid);
  auto managed_properties_guid =
      network_props_iter_wifi->second.managed_properties->guid;
  EXPECT_EQ(managed_properties_guid, guid);
}

struct FakeNetworkListObserver : public mojom::NetworkListObserver {
  void OnNetworkListChanged(const std::vector<std::string>& network_guids,
                            const std::string& active_guid) override {
    fake_network_guids = std::move(network_guids);
    fake_active_guid = active_guid;
    network_list_changed_event_received_ = true;
  }

  mojo::PendingRemote<mojom::NetworkListObserver> pending_remote() {
    return receiver.BindNewPipeAndPassRemote();
  }

  bool network_list_changed_event_received() {
    return network_list_changed_event_received_;
  }

  std::vector<std::string> fake_network_guids;
  std::string fake_active_guid;
  bool network_list_changed_event_received_ = false;
  mojo::Receiver<mojom::NetworkListObserver> receiver{this};
};

struct FakeNetworkStateObserver : public mojom::NetworkStateObserver {
  void OnNetworkStateChanged(mojom::NetworkPtr network_ptr) override {
    fake_network_state_updates.push_back(std::move(network_ptr));
    network_state_changed_event_received_ = true;
  }

  mojo::PendingRemote<mojom::NetworkStateObserver> pending_remote() {
    return receiver.BindNewPipeAndPassRemote();
  }

  bool network_state_changed_event_received() {
    return network_state_changed_event_received_;
  }

  // Tracks calls to OnNetworkStateChanged. Each call adds an element to
  // the vector.
  std::vector<mojom::NetworkPtr> fake_network_state_updates;

  mojo::Receiver<mojom::NetworkStateObserver> receiver{this};
  bool network_state_changed_event_received_ = false;
};

}  // namespace

class NetworkHealthProviderTest : public testing::Test {
 public:
  NetworkHealthProviderTest() {
    // Initialize the ManagedNetworkConfigurationHandler and any associated
    // properties.
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();

    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
    network_health_provider_ = std::make_unique<NetworkHealthProvider>();
  }

  ~NetworkHealthProviderTest() override {
    managed_network_configuration_handler_.reset();
    ui_proxy_config_service_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_health_provider_.reset();
    LoginState::Shutdown();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
  }

  void InitializeManagedNetworkConfigurationHandler() {
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_configuration_handler_ =
        base::WrapUnique<NetworkConfigurationHandler>(
            NetworkConfigurationHandler::InitializeForTest(
                network_state_helper().network_state_handler(),
                cros_network_config_test_helper().network_device_handler()));

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<chromeos::UIProxyConfigService>(
        &user_prefs_, &local_state_,
        network_state_helper().network_state_handler(),
        network_profile_handler_.get());

    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_helper().network_state_handler(),
            network_profile_handler_.get(),
            cros_network_config_test_helper().network_device_handler(),
            network_configuration_handler_.get(),
            ui_proxy_config_service_.get());

    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    // Wait until the |managed_network_configuration_handler_| is initialized
    // and set up.
    base::RunLoop().RunUntilIdle();
  }

  void SetupWiFiNetwork() {
    network_state_helper().ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID": false})");

    base::RunLoop().RunUntilIdle();
  }

  void SetupEthernetNetwork() {
    network_state_helper().device_test()->AddDevice(
        "/device/stub_eth_device", shill::kTypeEthernet, "stub_eth_device");
    network_state_helper().ConfigureService(
        R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");

    base::RunLoop().RunUntilIdle();
  }

  void SetupVPNNetwork() {
    network_state_helper().ConfigureService(
        R"({"GUID": "vpn_guid", "Type": "vpn", "State": "association",
            "Provider": {"Type": "l2tpipsec"}})");
    base::RunLoop().RunUntilIdle();
  }

 protected:
  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }

  void ResetDevicesAndServices() {
    // Clear test devices and services and setup the default wifi device.
    network_state_helper().ResetDevicesAndServices();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
  std::unique_ptr<NetworkHealthProvider> network_health_provider_;
};

TEST_F(NetworkHealthProviderTest, MultipleConnectedNetworksStoredInActiveList) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();
  SetupEthernetNetwork();

  const std::vector<std::string>& network_guid_list =
      network_health_provider_->GetNetworkGuidList();
  ASSERT_EQ(2u, network_guid_list.size());
  ASSERT_TRUE(base::Contains(network_guid_list, "wifi1_guid"));
  ASSERT_TRUE(base::Contains(network_guid_list, "eth_guid"));
}

TEST_F(NetworkHealthProviderTest, UnsupportedNetworkTypeIgnored) {
  ResetDevicesAndServices();
  SetupVPNNetwork();

  const std::vector<std::string>& network_guid_list =
      network_health_provider_->GetNetworkGuidList();
  ASSERT_TRUE(network_guid_list.empty());
}

TEST_F(NetworkHealthProviderTest, SingleSupportedDeviceStoredInDeviceTypeMap) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();

  const DeviceMap& device_type_map =
      network_health_provider_->GetDeviceTypeMapForTesting();

  EXPECT_EQ(1U, device_type_map.size());
  EXPECT_TRUE(base::Contains(device_type_map,
                             network_config::mojom::NetworkType::kWiFi));
}

TEST_F(NetworkHealthProviderTest,
       MultipleSupportedDevicesStoredInDeviceTypeMap) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();
  SetupEthernetNetwork();

  const DeviceMap& device_type_map =
      network_health_provider_->GetDeviceTypeMapForTesting();

  EXPECT_EQ(2U, device_type_map.size());
  EXPECT_TRUE(base::Contains(device_type_map,
                             network_config::mojom::NetworkType::kWiFi));
  EXPECT_TRUE(base::Contains(device_type_map,
                             network_config::mojom::NetworkType::kEthernet));
}

TEST_F(NetworkHealthProviderTest, DeviceTypeMapEmptyWithNoDevices) {
  ResetDevicesAndServices();
  // Remove the default WiFi device created by network_state_helper.
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->RemoveTechnology(shill::kTypeWifi);
  task_environment_.RunUntilIdle();

  const DeviceMap& device_type_map =
      network_health_provider_->GetDeviceTypeMapForTesting();
  EXPECT_EQ(0U, device_type_map.size());
}

TEST_F(NetworkHealthProviderTest, ManagedPropertiesSetForNetwork) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();

  const NetworkPropertiesMap& network_properties_map =
      network_health_provider_->GetNetworkPropertiesMapForTesting();

  EXPECT_EQ(1U, network_properties_map.size());
  ValidateManagedPropertiesSet(network_properties_map, "wifi1_guid");
}

TEST_F(NetworkHealthProviderTest, ManagedPropertiesSetForMultipleNetwork) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();
  SetupEthernetNetwork();

  const NetworkPropertiesMap& network_properties_map =
      network_health_provider_->GetNetworkPropertiesMapForTesting();

  EXPECT_EQ(2U, network_properties_map.size());
  ValidateManagedPropertiesSet(network_properties_map, "wifi1_guid");
  ValidateManagedPropertiesSet(network_properties_map, "eth_guid");
}

TEST_F(NetworkHealthProviderTest, NetworkListObserverSingleNetwork) {
  ResetDevicesAndServices();
  FakeNetworkListObserver fake_network_list_observer;
  network_health_provider_->ObserveNetworkList(
      fake_network_list_observer.pending_remote());

  SetupEthernetNetwork();

  std::vector<std::string> expected = {"eth_guid"};
  EXPECT_EQ(1U, fake_network_list_observer.fake_network_guids.size());
  EXPECT_EQ(fake_network_list_observer.fake_network_guids, expected);
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "eth_guid");
  EXPECT_EQ(fake_network_list_observer.network_list_changed_event_received(),
            true);
}

TEST_F(NetworkHealthProviderTest, NetworkListObserverNoActiveNetwork) {
  ResetDevicesAndServices();
  FakeNetworkListObserver fake_network_list_observer;
  network_health_provider_->ObserveNetworkList(
      fake_network_list_observer.pending_remote());

  SetupWiFiNetwork();

  std::vector<std::string> expected = {"wifi1_guid"};
  EXPECT_EQ(1U, fake_network_list_observer.fake_network_guids.size());
  EXPECT_EQ(fake_network_list_observer.fake_network_guids, expected);
  EXPECT_EQ(0U, fake_network_list_observer.fake_active_guid.size());
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "");
  EXPECT_EQ(fake_network_list_observer.network_list_changed_event_received(),
            true);
}

TEST_F(NetworkHealthProviderTest, NetworkListObserverMultipleNetworks) {
  ResetDevicesAndServices();
  FakeNetworkListObserver fake_network_list_observer;
  network_health_provider_->ObserveNetworkList(
      fake_network_list_observer.pending_remote());

  SetupEthernetNetwork();
  SetupWiFiNetwork();

  std::vector<std::string> expected = {"eth_guid", "wifi1_guid"};
  EXPECT_EQ(2U, fake_network_list_observer.fake_network_guids.size());
  EXPECT_EQ(fake_network_list_observer.fake_network_guids, expected);
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "eth_guid");
  EXPECT_EQ(fake_network_list_observer.network_list_changed_event_received(),
            true);
}

TEST_F(NetworkHealthProviderTest, NetworkListObserverNoNetworks) {
  ResetDevicesAndServices();
  FakeNetworkListObserver fake_network_list_observer;
  network_health_provider_->ObserveNetworkList(
      fake_network_list_observer.pending_remote());

  std::vector<std::string> expected;
  EXPECT_EQ(0U, fake_network_list_observer.fake_network_guids.size());
  EXPECT_EQ(fake_network_list_observer.fake_network_guids, expected);
  EXPECT_EQ(0U, fake_network_list_observer.fake_active_guid.size());
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "");
}

TEST_F(NetworkHealthProviderTest, ActiveGuidResetsWhenConnectionStateChanges) {
  ResetDevicesAndServices();
  FakeNetworkListObserver fake_network_list_observer;
  network_health_provider_->ObserveNetworkList(
      fake_network_list_observer.pending_remote());

  SetupEthernetNetwork();

  std::vector<std::string> expected = {"eth_guid"};
  EXPECT_EQ(1U, fake_network_list_observer.fake_network_guids.size());
  EXPECT_EQ(fake_network_list_observer.fake_network_guids, expected);
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "eth_guid");
  EXPECT_EQ(fake_network_list_observer.network_list_changed_event_received(),
            true);

  ResetDevicesAndServices();

  EXPECT_EQ(0U, fake_network_list_observer.fake_active_guid.size());
  EXPECT_EQ(fake_network_list_observer.fake_active_guid, "");
}

TEST_F(NetworkHealthProviderTest, NetworkStateObserver) {
  ResetDevicesAndServices();
  SetupWiFiNetwork();
  FakeNetworkStateObserver fake_network_state_observer;
  network_health_provider_->ObserveNetwork(
      fake_network_state_observer.pending_remote(), "wifi1_guid");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fake_network_state_observer.fake_network_state_updates.size());
  auto network =
      std::move(fake_network_state_observer.fake_network_state_updates[0]);

  // Correct NetworkTypeProperties struct set.
  EXPECT_EQ(network->type_properties->which(),
            mojom::NetworkTypeProperties::Tag::kWifi);
  EXPECT_EQ(network->type_properties->get_wifi()->signal_strength, 50);
  // Network state correctly mapped to corresponding mojom::NetworkState enum.
  EXPECT_EQ(network->state, mojom::NetworkState::kConnected);
  // Network state correctly mapped to corresponding mojom::NetworkType enum.
  EXPECT_EQ(network->type, mojom::NetworkType::kWiFi);
  EXPECT_EQ(network->guid, "wifi1_guid");
  LOG(INFO) << network->mac_address.value() << "$";
  EXPECT_EQ(fake_network_state_observer.network_state_changed_event_received(),
            true);
}

}  // namespace diagnostics
}  // namespace ash
