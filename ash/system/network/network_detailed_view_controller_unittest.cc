// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/network/network_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothSystemState;
using ::chromeos::network_config::mojom::ActivationStateType;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PortalState;

const std::string kCellular = "cellular";
constexpr char kCellularDevicePath[] = "/device/cellular_device";

constexpr char kWifi[] = "Wifi";
constexpr char kServicePatternWiFi[] = R"({
    "GUID": "%s", "Type": "wifi", "State": "%s", "Strength": 100,
            "Connectable": true})";

constexpr char kTetherName[] = "tether";
constexpr char kTetherGuid[] = "tetherNetworkGuid";
constexpr char kTetherCarrier[] = "TetherNetworkCarrier";
constexpr char kWifiServiceGuid[] = "wifiServiceGuid";
constexpr char kServicePatternTetherWiFi[] = R"({
    "GUID": "%s", "Type": "wifi", "State": "ready"})";

const int kSignalStrength = 50;
constexpr char kUser1Email[] = "user1@quicksettings.com";

constexpr char kNetworkConnectConfigured[] =
    "StatusArea_Network_ConnectConfigured";
constexpr char kNetworkConnectionDetails[] =
    "StatusArea_Network_ConnectionDetails";

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() {}

  NetworkConnectTestDelegate(const NetworkConnectTestDelegate&) = delete;
  NetworkConnectTestDelegate& operator=(const NetworkConnectTestDelegate&) =
      delete;

  ~NetworkConnectTestDelegate() override {}

  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSetupDialog(const std::string& network_id) override {}
  void ShowCarrierUnlockNotification() override {}
  void ShowCarrierAccountDetail(const std::string& network_id) override {}
  void ShowPortalSignin(const std::string& network_id,
                        NetworkConnect::Source source) override {
    portal_signin_guid_ = network_id;
  }
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {}
  void ShowMobileActivationError(const std::string& network_id) override {}

  const std::string& portal_signin_guid() const { return portal_signin_guid_; }

 private:
  std::string portal_signin_guid_;
};

}  // namespace

class NetworkDetailedViewControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    // Initialize CrosNetworkConfigTestHelper here, so we can initialize
    // a unique network handler and also use NetworkConnectTestDelegate to
    // initialize NetworkConnect.
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();

    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    network_connect_delegate_ = std::make_unique<NetworkConnectTestDelegate>();
    NetworkConnect::Initialize(network_connect_delegate_.get());
    AshTestBase::SetUp();

    network_detailed_view_controller_ =
        std::make_unique<NetworkDetailedViewController>(
            /*tray_controller=*/nullptr);
  }

  void TearDown() override {
    network_detailed_view_controller_.reset();
    AshTestBase::TearDown();
    NetworkConnect::Shutdown();
    NetworkHandler::Shutdown();
    network_connect_delegate_.reset();
  }

  void SelectNetworkListItem(const NetworkStatePropertiesPtr& network) {
    (static_cast<NetworkDetailedView::Delegate*>(
         network_detailed_view_controller_.get()))
        ->OnNetworkListItemSelected(mojo::Clone(network));
  }

  NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      NetworkType type,
      ConnectionStateType connection_state) {
    return network_config_helper_->CreateStandaloneNetworkProperties(
        id, type, connection_state, kSignalStrength);
  }

  std::string GetWifiNetworkState() {
    return network_state_helper()->GetServiceStringProperty(
        wifi_service_path_, shill::kStateProperty);
  }

  void DisconnectWifiNetwork() {
    network_state_helper()->SetServiceProperty(
        wifi_service_path_, std::string(shill::kStateProperty),
        base::Value(shill::kStateIdle));
    base::RunLoop().RunUntilIdle();
  }

  void ToggleWifiState(bool new_state) {
    (static_cast<NetworkDetailedNetworkView::Delegate*>(
         network_detailed_view_controller_.get()))
        ->OnWifiToggleClicked(new_state);
    base::RunLoop().RunUntilIdle();
  }

  void ToggleMobileState(bool new_state) {
    (static_cast<NetworkDetailedNetworkView::Delegate*>(
         network_detailed_view_controller_.get()))
        ->OnMobileToggleClicked(new_state);
    base::RunLoop().RunUntilIdle();
  }

  NetworkStateHandler::TechnologyState GetTechnologyState(
      const NetworkTypePattern& network) {
    return network_state_handler()->GetTechnologyState(network);
  }

  void SetTetherTechnologyState(NetworkStateHandler::TechnologyState state) {
    network_state_handler()->SetTetherTechnologyState(state);
    base::RunLoop().RunUntilIdle();
  }

  void AddCellularDevice() {
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);
    network_state_helper()->device_test()->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, kCellular);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void ClearDevices() {
    network_state_helper()->ClearDevices();
    base::RunLoop().RunUntilIdle();
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks.
  void AddTetherDevice() {
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    network_state_handler()->AddTetherNetworkState(
        kTetherGuid, kTetherName, kTetherCarrier, /*battery_percentage=*/100,
        kSignalStrength, /*has_connected_to_host=*/false);
    network_state_helper()->ConfigureService(
        base::StringPrintf(kServicePatternTetherWiFi, kWifiServiceGuid));
    network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherGuid, kWifiServiceGuid);
  }

  void AddWifiService(std::string state) {
    wifi_service_path_ = network_state_helper()->ConfigureService(
        base::StringPrintf(kServicePatternWiFi, kWifi, state.c_str()));
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  BluetoothSystemState GetBluetoothAdapterState() {
    return bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->GetAdapterState();
  }

  const std::string& portal_signin_guid() const {
    return network_connect_delegate_->portal_signin_guid();
  }

 private:
  NetworkStateHandler* network_state_handler() {
    return network_state_helper()->network_state_handler();
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_->network_state_helper();
  }

  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  std::unique_ptr<NetworkDetailedViewController>
      network_detailed_view_controller_;
  std::string wifi_service_path_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NetworkDetailedViewControllerTest,
       NetworkListItemSelectedWithLockedScreen) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellular, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);

  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());

  // Set login status to locked.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  SelectNetworkListItem(cellular_network);
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());

  // Show network details page for a connected cellular network.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SelectNetworkListItem(cellular_network);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
}

TEST_F(NetworkDetailedViewControllerTest, EmptyNetworkListItemSelected) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());

  SelectNetworkListItem(/*network=*/nullptr);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
}

TEST_F(NetworkDetailedViewControllerTest, CellularNetworkListItemSelected) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellular, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);

  // When cellular eSIM network is not activated open network details page.
  cellular_network->connection_state = ConnectionStateType::kNotConnected;
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  cellular_network->type_state->get_cellular()->eid = "eid";
  SelectNetworkListItem(cellular_network);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));

  // When cellular network is SIM locked, we show the SIM unlock settings page.
  cellular_network->type_state->get_cellular()->sim_locked = true;
  SelectNetworkListItem(cellular_network);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(1, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
}

TEST_F(NetworkDetailedViewControllerTest,
       CarrierLockedNetworkListItemSelected) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellular, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);

  // When cellular network is carrier locked, verify that SIM unlock
  // settings page is NOT displayed. Device will be unlocked only through
  // carrier lock manager.
  cellular_network->type_state->get_cellular()->sim_locked = true;
  cellular_network->type_state->get_cellular()->sim_lock_type = "network-pin";
  SelectNetworkListItem(cellular_network);
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
}

TEST_F(NetworkDetailedViewControllerTest, WifiNetworkListItemSelected) {
  base::UserActionTester user_action_tester;

  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());

  AddWifiService(shill::kStateIdle);
  // Clicking on an already connected network opens settings page.
  // Since this network is already connected, selecting this network
  // in network list vew should result in no change in NetworkState of
  // the network service.
  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWifi, NetworkType::kWiFi, ConnectionStateType::kOnline);

  SelectNetworkListItem(wifi_network);
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(shill::kStateIdle, GetWifiNetworkState());

  // Set to be connectable and make sure network is connected to.
  wifi_network->connection_state = ConnectionStateType::kNotConnected;
  wifi_network->connectable = true;
  SelectNetworkListItem(wifi_network);

  // Wait for Network to be connected to.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(shill::kStateOnline, GetWifiNetworkState());

  // Reset network state to idle.
  DisconnectWifiNetwork();
  EXPECT_EQ(shill::kStateIdle, GetWifiNetworkState());

  // Network can be connected to since active user is primary and the
  // network is configurable.
  wifi_network->connection_state = ConnectionStateType::kNotConnected;
  wifi_network->connectable = false;

  SelectNetworkListItem(wifi_network);

  // Wait for network to be connected to.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(2, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(shill::kStateOnline, GetWifiNetworkState());

  // Reset network to idle.
  DisconnectWifiNetwork();
  EXPECT_EQ(shill::kStateIdle, GetWifiNetworkState());

  // Login as secondary user, and make sure network is not connected to,
  // but settings page is opened.
  GetSessionControllerClient()->AddUserSession(kUser1Email);
  SimulateUserLogin(kUser1Email);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  base::RunLoop().RunUntilIdle();

  SelectNetworkListItem(wifi_network);
  EXPECT_EQ(2, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kNetworkConnectionDetails));
  EXPECT_EQ(2, user_action_tester.GetActionCount(kNetworkConnectConfigured));
  EXPECT_EQ(shill::kStateIdle, GetWifiNetworkState());
}

TEST_F(NetworkDetailedViewControllerTest, WifiStateChange) {
  // By default ash test instantiates WiFi networks and enables them.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::WiFi()));

  // Disable wifi.
  ToggleWifiState(/*new_state=*/false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            GetTechnologyState(NetworkTypePattern::WiFi()));

  // Renable wifi.
  ToggleWifiState(/*new_state=*/true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::WiFi()));
}

TEST_F(NetworkDetailedViewControllerTest, MobileToggleClicked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kInstantHotspotRebrand);

  AddCellularDevice();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Cellular()));

  ToggleMobileState(/*new_state=*/false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            GetTechnologyState(NetworkTypePattern::Cellular()));

  // When Cellular and Tether are both available toggle should control cellular.
  AddTetherDevice();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Tether()));

  // Set Tether to available and check toggle updates Cellular.
  SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE);

  ToggleMobileState(/*new_state=*/true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            GetTechnologyState(NetworkTypePattern::Tether()));
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Cellular()));

  ClearDevices();
  AddTetherDevice();

  // Toggle now controls Tether since there are no Cellular devices.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Tether()));

  ToggleMobileState(/*new_state=*/false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            GetTechnologyState(NetworkTypePattern::Tether()));

  // When Tether is uninitialized and Bluetooth is disabled, toggling Mobile on
  // should enable Bluetooth.
  SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);

  ToggleMobileState(/*new_state=*/true);
  EXPECT_EQ(BluetoothSystemState::kEnabling, GetBluetoothAdapterState());
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            GetTechnologyState(NetworkTypePattern::Tether()));

  // Simulate Bluetooth adapter being enabled. Note that when testing Bluetooth
  // will be set to kEnabling and needs to be manually changed to kEnabled using
  // adapter state. Enabling Bluetooth will also change Tether state to
  // available.
  SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE);
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);

  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Tether()));
}

TEST_F(NetworkDetailedViewControllerTest, MobileToggleDoesntAffectTether) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kInstantHotspotRebrand);

  AddCellularDevice();
  AddTetherDevice();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Cellular()));
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Tether()));

  // Toggle should only control Cellular device, not Tether device.
  ToggleMobileState(/*new_state=*/false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            GetTechnologyState(NetworkTypePattern::Cellular()));
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            GetTechnologyState(NetworkTypePattern::Tether()));
}

TEST_F(NetworkDetailedViewControllerTest, MobileToggleDoesntAffectBluetooth) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kInstantHotspotRebrand);

  AddCellularDevice();
  AddTetherDevice();

  // When Tether is uninitialized and Bluetooth is disabled, toggling Mobile on
  // should NOT enable Bluetooth with the Instant Hotspot Rebrand flag enabled.
  SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);

  ToggleMobileState(/*new_state=*/true);
  EXPECT_EQ(BluetoothSystemState::kDisabled, GetBluetoothAdapterState());
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            GetTechnologyState(NetworkTypePattern::Tether()));

  // Simulate Bluetooth adapter being enabled. Note that when testing Bluetooth
  // will be set to kEnabling and needs to be manually changed to kEnabled using
  // adapter state. Disabling cellular will NOT change the Bluetooth or Tether
  // state to available.
  SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  SetBluetoothAdapterState(BluetoothSystemState::kEnabled);

  ToggleMobileState(/*new_state=*/false);
  EXPECT_EQ(BluetoothSystemState::kEnabled, GetBluetoothAdapterState());
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            GetTechnologyState(NetworkTypePattern::Tether()));
}

TEST_F(NetworkDetailedViewControllerTest,
       PortalNetworkListItemSelectedWithFlagEnabled) {
  AddWifiService(shill::kStateRedirectFound);

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWifi, NetworkType::kWiFi, ConnectionStateType::kPortal);
  wifi_network->portal_state = PortalState::kPortal;

  SelectNetworkListItem(wifi_network);

  // Wait for Network to be connected to.
  base::RunLoop().RunUntilIdle();

  // Verify that guid is set from ShowPortalSignin.
  EXPECT_EQ(portal_signin_guid(), kWifi);
  EXPECT_EQ(0, GetSystemTrayClient()->show_network_settings_count());
  EXPECT_EQ(0, GetSystemTrayClient()->show_sim_unlock_settings_count());
  EXPECT_EQ(shill::kStateRedirectFound, GetWifiNetworkState());
}

}  // namespace ash
