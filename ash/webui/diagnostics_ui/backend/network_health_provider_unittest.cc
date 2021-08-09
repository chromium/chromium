// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/network_health_provider.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
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
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace diagnostics {
namespace {

constexpr char kEth0DevicePath[] = "/device/eth0";
constexpr char kEth0Name[] = "eth0_name";
constexpr char kEth0NetworkGuid[] = "eth0_network_guid";
constexpr char kWlan0DevicePath[] = "/device/wlan0";
constexpr char kWlan0Name[] = "wlan0_name";
constexpr char kWlan0NetworkGuid[] = "wlan0_network_guid";
constexpr char kFormattedMacAddress[] = "01:23:45:67:89:AB";
constexpr char kTestIPConfigPath[] = "test_ip_config_path";

// TODO(https://crbug.com/1164001): remove when network_config is moved to ash.
namespace network_config = ::chromeos::network_config;

class FakeNetworkListObserver : public mojom::NetworkListObserver {
 public:
  void OnNetworkListChanged(const std::vector<std::string>& observer_guids,
                            const std::string& active_guid) override {
    observer_guids_ = observer_guids;
    active_guid_ = active_guid;
    call_count_++;
  }

  mojo::PendingRemote<mojom::NetworkListObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<std::string>& observer_guids() const {
    return observer_guids_;
  }

  const std::string& active_guid() const { return active_guid_; }

  size_t call_count() const { return call_count_; }

 private:
  std::vector<std::string> observer_guids_;
  std::string active_guid_;
  size_t call_count_ = 0;
  mojo::Receiver<mojom::NetworkListObserver> receiver_{this};
};

class FakeNetworkStateObserver : public mojom::NetworkStateObserver {
 public:
  void OnNetworkStateChanged(mojom::NetworkPtr network_ptr) override {
    network_state_updates_.push_back(std::move(network_ptr));
  }

  mojo::PendingRemote<mojom::NetworkStateObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const mojom::NetworkPtr& GetLatestState() const {
    DCHECK(HasFired());
    return network_state_updates_.back();
  }

  size_t GetCallCount() const { return network_state_updates_.size(); }

  bool HasFired() const { return GetCallCount() > 0; }

 private:
  // Tracks calls to OnNetworkStateChanged. Each call adds an element to
  // the vector.
  std::vector<mojom::NetworkPtr> network_state_updates_;
  mojo::Receiver<mojom::NetworkStateObserver> receiver_{this};
};

// Expects that the call count increases and returns the new call count.
void ExpectListObserverFired(const FakeNetworkListObserver& observer,
                             size_t* prior_call_count) {
  DCHECK(prior_call_count);
  const size_t current_call_count = observer.call_count();
  EXPECT_GT(current_call_count, *prior_call_count);
  *prior_call_count = current_call_count;
}

// Expects that the call count increases and returns the new call count.
void ExpectStateObserverFired(const FakeNetworkStateObserver& observer,
                              size_t* prior_call_count) {
  DCHECK(prior_call_count);
  const size_t current_call_count = observer.GetCallCount();
  EXPECT_GT(current_call_count, *prior_call_count);
  *prior_call_count = current_call_count;
}

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
    ClearDevicesAndServices();
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

 protected:
  void CreateEthernetDevice() {
    network_state_helper().manager_test()->AddTechnology(shill::kTypeEthernet,
                                                         true);
    network_state_helper().device_test()->AddDevice(
        kEth0DevicePath, shill::kTypeEthernet, kEth0Name);

    base::RunLoop().RunUntilIdle();
  }

  void CreateWifiDevice() {
    network_state_helper().manager_test()->AddTechnology(shill::kTypeWifi,
                                                         true);
    network_state_helper().device_test()->AddDevice(
        kWlan0DevicePath, shill::kTypeWifi, kWlan0Name);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateIPConfigWithWifiDevice() {
    network_state_helper().device_test()->SetDeviceProperty(
        kWlan0DevicePath, shill::kSavedIPConfigProperty,
        base::Value(kTestIPConfigPath),
        /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void CreateVpnDevice() {
    network_state_helper().manager_test()->AddTechnology(shill::kTypeVPN, true);
    network_state_helper().device_test()->AddDevice(
        "/device/vpn", shill::kTypeVPN, "vpn_name");

    base::RunLoop().RunUntilIdle();
  }

  // The device must have been created with CreateEthernetDevice().
  void AssociateEthernet() {
    network_state_helper().service_test()->AddService(
        kEth0DevicePath, kEth0NetworkGuid, kEth0Name, shill::kTypeEthernet,
        shill::kStateAssociation, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateWifi() {
    network_state_helper().service_test()->AddService(
        kWlan0DevicePath, kWlan0NetworkGuid, kWlan0Name, shill::kTypeWifi,
        shill::kStateAssociation, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateWifiWithIPConfig() {
    network_state_helper().service_test()->AddServiceWithIPConfig(
        kWlan0DevicePath, kWlan0NetworkGuid, kWlan0Name, shill::kTypeWifi,
        shill::kStateAssociation, kTestIPConfigPath, true);

    base::RunLoop().RunUntilIdle();
  }

  void AssociateAndConnectVpn() {
    network_state_helper().service_test()->AddService(
        "/device/vpn", "vpn guid", "vpn_name", shill::kTypeVPN,
        shill::kStateAssociation, true);

    SetNetworkState("/device/vpn", shill::kStateOnline);
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkState(const std::string& device_path,
                       const std::string& state) {
    network_state_helper().SetServiceProperty(
        device_path, shill::kStateProperty, base::Value(state));
    base::RunLoop().RunUntilIdle();
  }

  void SetEthernetConnected() {
    // NOTE: `kStateReady` is connected but not "online".
    SetNetworkState(kEth0DevicePath, shill::kStateReady);
  }

  void SetEthernetOnline() {
    SetNetworkState(kEth0DevicePath, shill::kStateOnline);
  }

  void SetEthernetDisconnected() {
    SetNetworkState(kEth0DevicePath, shill::kStateOffline);
  }

  void SetWifiConnected() {
    // NOTE: `kStateReady` is connected but not "online".
    SetNetworkState(kWlan0DevicePath, shill::kStateReady);
  }

  void SetWifiOnline() {
    SetNetworkState(kWlan0DevicePath, shill::kStateOnline);
  }

  void SetWifiDisconnected() {
    SetNetworkState(kWlan0DevicePath, shill::kStateOffline);
  }

  void SetWifiProperty(std::string property, base::Value value) {
    network_state_helper().SetServiceProperty(kWlan0DevicePath, property,
                                              value);
    base::RunLoop().RunUntilIdle();
  }

  void SetWifiSignalStrength(int signal_strength) {
    SetWifiProperty(shill::kSignalStrengthProperty,
                    base::Value(signal_strength));
  }

  void SetWifiFrequency(int frequency) {
    SetWifiProperty(shill::kWifiFrequency, base::Value(frequency));
  }

  void SetWifiBssid(std::string bssid) {
    SetWifiProperty(shill::kWifiBSsid, base::Value(bssid));
  }

  void SetEthernetMacAddress(const std::string& mac_address) {
    network_state_helper().device_test()->SetDeviceProperty(
        kEth0DevicePath, shill::kAddressProperty, base::Value(mac_address),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetWifiMacAddress(const std::string& mac_address) {
    network_state_helper().device_test()->SetDeviceProperty(
        kWlan0DevicePath, shill::kAddressProperty, base::Value(mac_address),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetGatewayForIPConfig(const std::string& gateway) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kGatewayProperty,
        base::Value(gateway), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetIPAddressForIPConfig(const std::string& ip_address) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kAddressProperty,
        base::Value(ip_address), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetNameServersForIPConfig(const base::ListValue& dns_servers) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kNameServersProperty,
        dns_servers, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetRoutingPrefixForIPConfig(int routing_prefix) {
    chromeos::ShillIPConfigClient::Get()->SetProperty(
        dbus::ObjectPath(kTestIPConfigPath), shill::kPrefixlenProperty,
        base::Value(routing_prefix), base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetupObserver(FakeNetworkListObserver* observer) {
    network_health_provider_->ObserveNetworkList(observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  void SetupObserver(FakeNetworkStateObserver* observer,
                     const std::string& observer_guid) {
    network_health_provider_->ObserveNetwork(observer->pending_remote(),
                                             observer_guid);
    base::RunLoop().RunUntilIdle();
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }

  void ClearDevicesAndServices() {
    // Clear test devices and services.
    network_state_helper().ClearDevices();
    network_state_helper().ClearServices();
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

TEST_F(NetworkHealthProviderTest, ZeroNetworksAvailable) {
  FakeNetworkListObserver observer;
  SetupObserver(&observer);
  size_t prior_call_count = 0;
  ExpectListObserverFired(observer, &prior_call_count);
  EXPECT_TRUE(observer.observer_guids().empty());
  EXPECT_TRUE(observer.active_guid().empty());
}

TEST_F(NetworkHealthProviderTest, ObserveNonExistantNetwork) {
  // Observe a guid that doesn't exist and nothing happens.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, "unknown_guid");
  EXPECT_FALSE(observer.HasFired());
}

TEST_F(NetworkHealthProviderTest, UnsupportedNetworkTypeIgnored) {
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // Create a VPN device, and it should not be visible to the observer.
  CreateVpnDevice();
  EXPECT_TRUE(list_observer.observer_guids().empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Associate and connect the VPN but it should still not be visible.
  AssociateAndConnectVpn();
  EXPECT_TRUE(list_observer.observer_guids().empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create an ethernet device and verify that the observer list added the
  // network.
  CreateEthernetDevice();
  ExpectListObserverFired(list_observer, &list_call_count);
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  ASSERT_FALSE(guid.empty());

  // Observe the guid and verify that it's the ethernet.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, guid);
  EXPECT_EQ(eth_observer.GetLatestState()->type, mojom::NetworkType::kEthernet);
}

// Test the setup and all intermediate states for ethernet network.
TEST_F(NetworkHealthProviderTest, SetupEthernetNetwork) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create an ethernet device and verify `list_observer` fired.
  CreateEthernetDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  size_t state_call_count = 0;
  ExpectStateObserverFired(observer, &state_call_count);

  // Get latest state and verify ethernet in not connected state.
  EXPECT_EQ(observer.GetLatestState()->guid, guid);
  EXPECT_EQ(observer.GetLatestState()->type, mojom::NetworkType::kEthernet);
  // TODO(michaelcheco): Support disabled state.
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(observer.GetLatestState()->type_properties.get(), nullptr);

  // Put the ethernet device into the connecting/associating state and verify
  // the new state and there is still no active guid.
  AssociateEthernet();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnecting);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Put ethernet into connected (but not online) state. It's guid should now
  // be the active one.
  SetEthernetConnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnected);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());
  // TODO(michaelcheco): Verify ethernet authentication properties once added
  // to the API.

  // Put ethernet into online state. It's guid should remain active.
  SetEthernetOnline();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // Simulate unplug and network goes back to kNotConnected, and the active
  // guid should be cleared.
  SetEthernetDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Simulate plug in and back to online state. The active guid should be set.
  SetEthernetOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());
}

// Test the setup and all intermediate states for ethernet network.
TEST_F(NetworkHealthProviderTest, SetupWifiNetwork) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create a wifi device and verify `list_observer` fired.
  CreateWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  size_t state_call_count = 0;
  ExpectStateObserverFired(observer, &state_call_count);

  // Get latest state and verify wifi in not connected state.
  EXPECT_EQ(observer.GetLatestState()->guid, guid);
  EXPECT_EQ(observer.GetLatestState()->type, mojom::NetworkType::kWiFi);
  // TODO(michaelcheco): Support disabled state.
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(observer.GetLatestState()->type_properties.get(), nullptr);

  // Put the wifi device into the connecting/associating state and verify
  // the new state and there is still no active guid.
  AssociateWifi();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnecting);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Put wifi into connected (but not online) state. It's guid should now
  // be the active one.
  SetWifiConnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kConnected);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());
  // TODO(michaelcheco): Verify encryption properties once added to the API.

  // Put wifi into online state. It's guid should remain active.
  SetWifiOnline();
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // Simulate disconnect and network goes back to kNotConnected, and the
  // active guid should be cleared.
  SetWifiDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Simulate reconnect and back to online state. The active guid should be
  // set.
  SetWifiOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());
}

// Test modifying wifi properties
TEST_F(NetworkHealthProviderTest, ChangingWifiProperties) {
  // Create a wifi device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateWifiDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];

  // Put wifi online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateWifi();
  SetWifiOnline();
  size_t state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  // Set signal strength.
  const int signal_strength_1 = 40;
  SetWifiSignalStrength(signal_strength_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_1);

  // Change the signal strength.
  const int signal_strength_2 = 55;
  SetWifiSignalStrength(signal_strength_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_2);

  // Set BSSID.
  const std::string bssid_1("wifi_bssid_1");
  SetWifiBssid(bssid_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_1);

  // Change BSSID.
  const std::string bssid_2("wifi_bssid_2");
  SetWifiBssid(bssid_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_2);

  // Set frequency.
  const int frequency_1 = 2400;
  SetWifiFrequency(frequency_1);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_1);

  // Change frequency.
  const int frequency_2 = 2450;
  SetWifiFrequency(frequency_2);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_2);

  // Verify all properties are still set.
  EXPECT_EQ(
      observer.GetLatestState()->type_properties->get_wifi()->signal_strength,
      signal_strength_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->bssid,
            bssid_2);
  EXPECT_EQ(observer.GetLatestState()->type_properties->get_wifi()->frequency,
            frequency_2);
}

// Start with an online ethernet connection and validate the interaction
// with a newly added wifi network.
TEST_F(NetworkHealthProviderTest, EthernetOnlineThenConnectWifi) {
  // Create an ethernet device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateEthernetDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string eth_guid = list_observer.observer_guids()[0];

  // Put ethernet online and validate it is active.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, eth_guid);
  AssociateEthernet();
  SetEthernetOnline();
  size_t state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Create Wifi device and verify it was added to the network list. The
  // ethernet network should remain active.
  CreateWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);
  ASSERT_EQ(2u, list_observer.observer_guids().size());
  const std::string wifi_guid = (list_observer.observer_guids()[0] == eth_guid)
                                    ? list_observer.observer_guids()[1]
                                    : list_observer.observer_guids()[0];
  ASSERT_NE(eth_guid, wifi_guid);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(eth_guid, list_observer.active_guid());

  // Observe and associate the Wifi network. The wifi network should be in
  // the connecting state, and the active guid should still be ethernet.
  FakeNetworkStateObserver wifi_observer;
  SetupObserver(&wifi_observer, wifi_guid);
  AssociateWifi();
  state_call_count = 0;
  ExpectStateObserverFired(wifi_observer, &state_call_count);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kConnecting);
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Put wifi network online. With both networks online, the ethernet should
  // still remain the active network.
  SetWifiOnline();
  ExpectStateObserverFired(wifi_observer, &state_call_count);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  // Disconnect ethernet and wifi should become the active network.
  SetEthernetDisconnected();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state,
            mojom::NetworkState::kNotConnected);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(wifi_guid, list_observer.active_guid());

  // Reconnect ethernet and it becomes the active network again.
  SetEthernetOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_EQ(wifi_observer.GetLatestState()->state,
            mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  ASSERT_EQ(eth_guid, list_observer.active_guid());
}

TEST_F(NetworkHealthProviderTest, SetupEthernetNetworkWithMacAddress) {
  // Create an ethernet device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateEthernetDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string eth_guid = list_observer.observer_guids()[0];

  // Put ethernet online and validate it is active.
  FakeNetworkStateObserver eth_observer;
  SetupObserver(&eth_observer, eth_guid);
  AssociateEthernet();
  SetEthernetOnline();
  size_t eth_state_call_count = 0;
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(eth_observer, &eth_state_call_count);
  EXPECT_EQ(eth_observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(eth_guid, list_observer.active_guid());

  SetEthernetMacAddress(kFormattedMacAddress);
  ExpectStateObserverFired(eth_observer, &eth_state_call_count);

  EXPECT_EQ(eth_observer.GetLatestState()->mac_address, kFormattedMacAddress);
}

TEST_F(NetworkHealthProviderTest, SetupWifiNetworkWithMacAddress) {
  // Create a wifi device.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  CreateWifiDevice();
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  size_t state_call_count = 0;
  size_t list_call_count = 0;

  // Put wifi online and validate it is active.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);
  AssociateWifi();
  SetWifiOnline();
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  SetWifiMacAddress(kFormattedMacAddress);
  ExpectStateObserverFired(observer, &state_call_count);

  EXPECT_EQ(observer.GetLatestState()->mac_address, kFormattedMacAddress);
}

TEST_F(NetworkHealthProviderTest, IPConfig) {
  // Observe the network list.
  FakeNetworkListObserver list_observer;
  SetupObserver(&list_observer);
  size_t list_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);

  // No networks are present and no active network.
  ASSERT_EQ(0u, list_observer.observer_guids().size());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Create a wifi device and verify `list_observer` fired.
  CreateWifiDevice();
  AssociateIPConfigWithWifiDevice();
  ExpectListObserverFired(list_observer, &list_call_count);

  // Verify a new network is created, but there is no active guid because
  // the network isn't connected.
  ASSERT_EQ(1u, list_observer.observer_guids().size());
  const std::string guid = list_observer.observer_guids()[0];
  EXPECT_FALSE(guid.empty());
  EXPECT_TRUE(list_observer.active_guid().empty());

  // Observe the network and verify the observer fired.
  FakeNetworkStateObserver observer;
  SetupObserver(&observer, guid);

  // Set IP Config properties.
  const std::string gateway("192.0.0.1");
  SetGatewayForIPConfig(gateway);
  const std::string ip_address("192.168.1.1");
  SetIPAddressForIPConfig(ip_address);
  const int routing_prefix = 1;
  SetRoutingPrefixForIPConfig(routing_prefix);
  base::ListValue dns_servers;
  const std::string dns_server_1 = "192.168.1.100";
  const std::string dns_server_2 = "192.168.1.101";
  dns_servers.AppendString(dns_server_1);
  dns_servers.AppendString(dns_server_2);
  SetNameServersForIPConfig(dns_servers);

  AssociateWifiWithIPConfig();
  SetWifiOnline();

  list_call_count = 0;
  size_t state_call_count = 0;
  ExpectListObserverFired(list_observer, &list_call_count);
  ExpectStateObserverFired(observer, &state_call_count);
  EXPECT_EQ(observer.GetLatestState()->state, mojom::NetworkState::kOnline);
  EXPECT_FALSE(list_observer.active_guid().empty());
  EXPECT_EQ(guid, list_observer.active_guid());

  auto ip_config = observer.GetLatestState()->ip_config.Clone();
  EXPECT_EQ(ip_config->gateway.value(), gateway);
  EXPECT_EQ(ip_config->routing_prefix, routing_prefix);
  EXPECT_EQ(ip_config->ip_address.value(), ip_address);

  auto name_servers = ip_config->name_servers.value();
  EXPECT_EQ(name_servers.size(), 2U);
  EXPECT_EQ(name_servers[0], dns_server_1);
  EXPECT_EQ(name_servers[1], dns_server_2);
}

}  // namespace diagnostics
}  // namespace ash
