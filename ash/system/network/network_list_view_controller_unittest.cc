// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/fake_network_detailed_network_view.h"
#include "ash/system/network/fake_network_list_mobile_header_view.h"
#include "ash/system/network/fake_network_list_wifi_header_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"

namespace ash {

namespace {

using testing::_;
using testing::Return;

using chromeos::bluetooth_config::ScopedBluetoothConfigTestHelper;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;

using chromeos::network_config::CrosNetworkConfigTestHelper;
using chromeos::network_config::NetworkTypeMatchesType;

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;
using chromeos::network_config::mojom::SecurityType;

const std::string kCellularName = "cellular";
const std::string kCellularName2 = "cellular_2";
const char kCellularDeviceName[] = "cellular_device";
const char kCellularDevicePath[] = "/device/cellular_device";
const char kCellularTestIccid[] = "1234567890";

const char kTetherName[] = "tether";
const char kTetherGuid[] = "tetherNetworkGuid";
const char kTetherCarrier[] = "TetherNetworkCarrier";
const char kWifiServiceGuid[] = "wifiServiceGuid";

const std::string kEthernet = "ethernet";
const std::string kEthernet2 = "ethernet_2";

const char kVpnName[] = "vpn";
const char kVpnDevicePath[] = "device/vpn";

const char kWifiName[] = "wifi";
const char kWifiName2[] = "wifi_2";
const char kWifiDevicePath[] = "device/wifi";

const char kTestEuiccBasePath[] = "/org/chromium/Hermes/Euicc/";
const char kTestBaseEid[] = "12345678901234567890123456789012";

const int kSignalStrength = 50;
constexpr char kUser1Email[] = "user1@quicksettings.com";

constexpr char kNetworkListNetworkItemView[] = "NetworkListNetworkItemView";

// Delay used to simulate running process when setting device technology state.
constexpr base::TimeDelta kInteractiveDelay = base::Milliseconds(3000);

std::string CreateConfigurationJsonString(const std::string& guid,
                                          const std::string& type,
                                          const std::string& state) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << type << "\","
     << "  \"State\": \"" << state << "\""
     << "}";
  return ss.str();
}

std::string CreateTestEuiccPath(int euicc_num) {
  return base::StringPrintf("%s%d", kTestEuiccBasePath, euicc_num);
}

std::string CreateTestEid(int euicc_num) {
  return base::StringPrintf("%s%d", kTestBaseEid, euicc_num);
}

}  // namespace

class NetworkListViewControllerTest : public AshTestBase {
 public:
  NetworkListViewControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  NetworkListViewControllerTest(const NetworkListViewControllerTest&) = delete;
  NetworkListViewControllerTest& operator=(
      const NetworkListViewControllerTest&) = delete;
  ~NetworkListViewControllerTest() override = default;

  void SetUp() override {
    // Initialize CrosNetworkConfigTestHelper here, so we can use
    // MockManagedNetworkConfigurationHandler.
    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>(
            /*initialize=*/false);

    mock_managed_network_configuration_manager_ = base::WrapUnique(
        new testing::NiceMock<MockManagedNetworkConfigurationHandler>);

    SetGlobalPolicyConfig(/*allow_only_policy=*/false);

    ON_CALL(*mock_managed_network_configuration_manager_,
            GetGlobalConfigFromPolicy(_))
        .WillByDefault(Return(&global_config_));

    cros_network_config_test_helper_->Initialize(
        mock_managed_network_configuration_manager_.get());
    base::RunLoop().RunUntilIdle();

    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    fake_network_detailed_network_view_ =
        std::make_unique<FakeNetworkDetailedNetworkView>(
            /*delegate=*/nullptr);

    network_list_view_controller_impl_ =
        std::make_unique<NetworkListViewControllerImpl>(
            fake_network_detailed_network_view_.get());
  }

  void SetGlobalPolicyConfig(bool allow_only_policy) {
    base::Value::Dict global_config_dict;
    global_config_dict.Set(
        ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
        allow_only_policy);

    global_config_ = base::Value(std::move(global_config_dict));

    // This function can be called before AshTestBase::SetUp(), Shell is not
    // initialized, make sure to only call FlushGlobalPolicyForTesting
    // after initialization.
    if (Shell::HasInstance()) {
      Shell::Get()
          ->system_tray_model()
          ->network_state_model()
          ->FlushGlobalPolicyForTesting();
      base::RunLoop().RunUntilIdle();
    }
  }

  void TearDown() override {
    network_list_view_controller_impl_.reset();
    fake_network_detailed_network_view_.reset();
    cros_network_config_test_helper_.reset();

    AshTestBase::TearDown();
  }

  views::ToggleButton* GetMobileToggleButton() {
    return static_cast<views::ToggleButton*>(GetMobileSubHeader()->GetViewByID(
        static_cast<int>(NetworkListNetworkHeaderView::kToggleButtonId)));
  }

  views::ToggleButton* GetWifiToggleButton() {
    return static_cast<views::ToggleButton*>(GetWifiSubHeader()->GetViewByID(
        static_cast<int>(NetworkListNetworkHeaderView::kToggleButtonId)));
  }

  FakeNetworkListMobileHeaderView* GetMobileSubHeader() {
    return FindViewById<FakeNetworkListMobileHeaderView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kMobileSectionHeader);
  }

  views::Separator* GetMobileSeparator() {
    return FindViewById<views::Separator*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kMobileSeperator);
  }

  FakeNetworkListWifiHeaderView* GetWifiSubHeader() {
    return FindViewById<FakeNetworkListWifiHeaderView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kWifiSectionHeader);
  }

  views::Separator* GetWifiSeparator() {
    return FindViewById<views::Separator*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kWifiSeperator);
  }

  TrayInfoLabel* GetMobileStatusMessage() {
    return FindViewById<TrayInfoLabel*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kMobileStatusMessage);
  }

  TrayInfoLabel* GetWifiStatusMessage() {
    return FindViewById<TrayInfoLabel*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kWifiStatusMessage);
  }

  TriView* GetConnectionWarning() {
    return FindViewById<TriView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kConnectionWarning);
  }

  views::Label* GetConnectionLabelView() {
    return FindViewById<views::Label*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kConnectionWarningLabel);
  }

  views::View* GetViewInNetworkList(std::string id) {
    return network_list_view_controller_impl_->network_id_to_view_map_[id];
  }

  void UpdateNetworkList(
      const std::vector<NetworkStatePropertiesPtr>& networks) {
    network_list_view_controller_impl_->OnGetNetworkStateList(
        mojo::Clone(networks));
  }

  // Checks that network list items are in the right order. Wifi section
  // is always shown.
  void CheckNetworkListOrdering(int ethernet_network_count,
                                int mobile_network_count,
                                int wifi_network_count) {
    EXPECT_NE(nullptr, GetWifiSubHeader());

    size_t index = 0;

    // Expect that the view at |index| is a network item, and that it is an
    // ethernet network.
    for (int i = 0; i < ethernet_network_count; i++) {
      CheckNetworkListItem(NetworkType::kEthernet, index++,
                           /*guid=*/absl::nullopt);
    }

    // Mobile data section. If |mobile_network_count| is equal to -1
    // Mobile device is not available.
    if (mobile_network_count != -1) {
      EXPECT_NE(nullptr, GetMobileSubHeader());
      if (index > 0) {
        // Expect that the mobile network separator exists.
        EXPECT_NE(nullptr, GetMobileSeparator());
        EXPECT_EQ(network_list()->children().at(index++), GetMobileSeparator());
        EXPECT_EQ(network_list()->children().at(index++), GetMobileSubHeader());
      } else {
        EXPECT_EQ(nullptr, GetMobileSeparator());
        EXPECT_EQ(network_list()->children().at(index++), GetMobileSubHeader());
      }

      for (int i = 0; i < mobile_network_count; i++) {
        CheckNetworkListItem(NetworkType::kMobile, index,
                             /*guid=*/absl::nullopt);
        EXPECT_STREQ(network_list()->children().at(index++)->GetClassName(),
                     kNetworkListNetworkItemView);
      }

      if (!mobile_network_count) {
        // No mobile networks message is shown.
        EXPECT_NE(nullptr, GetMobileStatusMessage());
        index++;
      }
    }

    // Wifi section.
    if (index > 0) {
      // Expect that the wifi network separator exists.
      EXPECT_NE(nullptr, GetWifiSeparator());
      EXPECT_EQ(network_list()->children().at(index++), GetWifiSeparator());
      EXPECT_EQ(network_list()->children().at(index++), GetWifiSubHeader());
    } else {
      EXPECT_EQ(nullptr, GetWifiSeparator());
      EXPECT_EQ(network_list()->children().at(index++), GetWifiSubHeader());
    }

    for (int i = 0; i < wifi_network_count; i++) {
      CheckNetworkListItem(NetworkType::kWiFi, index, /*guid=*/absl::nullopt);
      EXPECT_STREQ(network_list()->children().at(index++)->GetClassName(),
                   kNetworkListNetworkItemView);
    }

    if (!wifi_network_count) {
      // When no WiFi networks are available, status message is shown.
      EXPECT_NE(nullptr, GetWifiStatusMessage());
      index++;
    } else {
      // Status message is not shown when WiFi networks are available.
      EXPECT_EQ(nullptr, GetWifiStatusMessage());
    }
  }

  void CheckNetworkListItem(NetworkType type,
                            size_t index,
                            const absl::optional<std::string>& guid) {
    ASSERT_GT(network_list()->children().size(), index);
    EXPECT_STREQ(network_list()->children().at(index)->GetClassName(),
                 kNetworkListNetworkItemView);

    const NetworkStatePropertiesPtr& network =
        static_cast<NetworkListNetworkItemView*>(
            network_list()->children().at(index))
            ->network_properties();
    EXPECT_TRUE(NetworkTypeMatchesType(network->type, type));

    if (guid.has_value()) {
      EXPECT_EQ(network->guid, guid);
    }
  }

  void SetupCellular() {
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);
    network_state_helper()->device_test()->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, kCellularDeviceName);

    base::Value::ListStorage sim_slot_infos;
    base::Value slot_info_item(base::Value::Type::DICTIONARY);
    slot_info_item.SetKey(shill::kSIMSlotInfoICCID,
                          base::Value(kCellularTestIccid));
    slot_info_item.SetBoolKey(shill::kSIMSlotInfoPrimary, true);
    slot_info_item.SetStringKey(shill::kSIMSlotInfoEID, kTestBaseEid);
    sim_slot_infos.push_back(std::move(slot_info_item));
    network_state_helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(sim_slot_infos), /*notify_changed=*/true);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void AddEuicc() {
    network_state_helper()->hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(CreateTestEuiccPath(/*euicc_num=*/1)),
        CreateTestEid(/*euicc_num=*/1), /*is_active=*/true,
        /*physical_slot=*/0);

    // Wait for network state change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks.
  void AddTetherNetworkState() {
    network_state_handler()->SetTetherTechnologyState(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
    network_state_handler()->AddTetherNetworkState(
        kTetherGuid, kTetherName, kTetherCarrier, /*battery_percentage=*/100,
        kSignalStrength, /*has_connected_to_host=*/false);
    network_state_helper()->ConfigureService(CreateConfigurationJsonString(
        kWifiServiceGuid, shill::kTypeWifi, shill::kStateReady));
    network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherGuid, kWifiServiceGuid);
  }

  void AddVpnDevice() {
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeVPN,
                                                          /*enabled=*/true);
    network_state_helper()->device_test()->AddDevice(kVpnDevicePath,
                                                     shill::kTypeVPN, kVpnName);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void AddWifiDevice() {
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeWifi,
                                                          /*enabled=*/true);
    network_state_helper()->device_test()->AddDevice(
        kWifiDevicePath, shill::kTypeWifi, kWifiName);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellularScanning() {
    base::RunLoop inhibit_loop;
    CellularInhibitor::InhibitReason inhibit_reason =
        CellularInhibitor::InhibitReason::kInstallingProfile;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    cros_network_config_test_helper_->cellular_inhibitor()
        ->InhibitCellularScanning(
            inhibit_reason,
            base::BindLambdaForTesting(
                [&](std::unique_ptr<CellularInhibitor::InhibitLock> result) {
                  inhibit_lock = std::move(result);
                  inhibit_loop.Quit();
                }));
    inhibit_loop.Run();
    return inhibit_lock;
  }

  NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      NetworkType type,
      ConnectionStateType connection_state) {
    return cros_network_config_test_helper_->CreateStandaloneNetworkProperties(
        id, type, connection_state, kSignalStrength);
  }

  void SetBluetoothAdapterState(BluetoothSystemState system_state) {
    bluetooth_config_test_helper()
        ->fake_adapter_state_controller()
        ->SetSystemState(system_state);
    base::RunLoop().RunUntilIdle();
  }

  void LoginAsSecondaryUser() {
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    SimulateUserLogin(kUser1Email);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOGIN_SECONDARY);
    base::RunLoop().RunUntilIdle();
  }

  chromeos::NetworkStateHandler* network_state_handler() {
    return network_state_helper()->network_state_handler();
  }

  chromeos::NetworkStateTestHelper* network_state_helper() {
    return &cros_network_config_test_helper_->network_state_helper();
  }

  views::View* network_list() {
    return static_cast<NetworkDetailedNetworkView*>(
               fake_network_detailed_network_view_.get())
        ->network_list();
  }

 protected:
  const std::vector<NetworkStatePropertiesPtr> empty_list_;

 private:
  template <class T>
  T FindViewById(
      NetworkListViewControllerImpl::NetworkListViewControllerViewChildId id) {
    return static_cast<T>(network_list()->GetViewByID(static_cast<int>(id)));
  }

  ScopedBluetoothConfigTestHelper* bluetooth_config_test_helper() {
    return ash_test_helper()->bluetooth_config_test_helper();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeNetworkDetailedNetworkView>
      fake_network_detailed_network_view_;
  std::unique_ptr<NetworkListViewControllerImpl>
      network_list_view_controller_impl_;

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;

  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      mock_managed_network_configuration_manager_;

  base::Value global_config_;
};

TEST_F(NetworkListViewControllerTest, MobileDataSectionIsShown) {
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  EXPECT_EQ(nullptr, GetMobileSeparator());

  AddEuicc();
  SetupCellular();
  EXPECT_NE(nullptr, GetMobileSubHeader());

  // Mobile separator is still null because mobile data is at index 0.
  EXPECT_EQ(nullptr, GetMobileSeparator());

  // Clear device list and check if Mobile subheader is shown with just
  // tether device.
  network_state_helper()->ClearDevices();
  EXPECT_EQ(nullptr, GetMobileSubHeader());

  // Add tether networks
  AddTetherNetworkState();
  EXPECT_NE(nullptr, GetMobileSubHeader());

  // Tether device is prohibited.
  network_state_handler()->SetTetherTechnologyState(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetMobileSubHeader());

  // Tether device is uninitialized but is primary user.
  network_state_handler()->SetTetherTechnologyState(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(nullptr, GetMobileSubHeader());

  // Simulate login as secondary user.
  LoginAsSecondaryUser();
  UpdateNetworkList(empty_list_);
  EXPECT_EQ(nullptr, GetMobileSubHeader());

  // Add tether networks
  AddTetherNetworkState();
  EXPECT_NE(nullptr, GetMobileSubHeader());
}

TEST_F(NetworkListViewControllerTest, WifiSectionHeader) {
  EXPECT_EQ(nullptr, GetWifiSubHeader());
  EXPECT_EQ(nullptr, GetWifiSeparator());

  // Add an enabled wifi device.
  AddWifiDevice();

  EXPECT_NE(nullptr, GetWifiSubHeader());
  EXPECT_EQ(nullptr, GetWifiSeparator());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetWifiSubHeader()->is_join_wifi_enabled());

  // Disable wifi device.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetWifiSubHeader());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_FALSE(GetWifiSubHeader()->is_join_wifi_enabled());
}

TEST_F(NetworkListViewControllerTest, MobileSectionHeaderAddEsimButtonStates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ash::features::kESimPolicy);
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  SetupCellular();
  EXPECT_NE(nullptr, GetMobileSubHeader());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_enabled());

  // Since no Euicc was added, this means device is not eSIM capable, do not
  // show add eSIM button.
  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_visible());

  AddEuicc();
  UpdateNetworkList(empty_list_);

  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());
  EXPECT_EQ(nullptr, GetMobileSeparator());
  EXPECT_NE(nullptr, GetMobileStatusMessage());

  // Add eSIM button is not enabled when inhibited.
  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock =
      InhibitCellularScanning();
  EXPECT_TRUE(inhibit_lock);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());

  // Uninhibit the device.
  inhibit_lock.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());

  // When no Mobile networks are available and eSIM policy is set to allow only
  // cellular devices which means adding a new eSIM is disallowed by enterprise
  // policy, add eSIM button is not displayed.
  SetGlobalPolicyConfig(/*allow_only_policy=*/true);
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeature(ash::features::kESimPolicy);
  UpdateNetworkList(empty_list_);
  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_visible());
}

TEST_F(NetworkListViewControllerTest, HasCorrectMobileNetworkList) {
  EXPECT_EQ(0u, network_list()->children().size());
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  AddEuicc();
  SetupCellular();
  AddWifiDevice();

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/0,
                           /*wifi_network_count=*/0);

  std::vector<NetworkStatePropertiesPtr> networks;

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  networks.push_back(std::move(cellular_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/1u,
                       /*guid=*/kCellularName);

  cellular_network = CreateStandaloneNetworkProperties(
      kCellularName2, NetworkType::kCellular, ConnectionStateType::kConnected);
  networks.push_back(std::move(cellular_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/2,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/2u,
                       /*guid=*/kCellularName2);

  // Update a network and make sure it is still in network list.
  networks.front()->connection_state = ConnectionStateType::kNotConnected;
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/2,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/1u,
                       /*guid=*/kCellularName);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/2u,
                       /*guid=*/kCellularName2);

  // Remove all networks and add Tether networks. Only one network should be in
  // list.
  networks.clear();
  NetworkStatePropertiesPtr tether_network = CreateStandaloneNetworkProperties(
      kTetherName, NetworkType::kTether, ConnectionStateType::kConnected);
  networks.push_back(std::move(tether_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kTether, /*index=*/1u,
                       /*guid=*/kTetherName);
}

TEST_F(NetworkListViewControllerTest, HasCorrectEthernetNetworkList) {
  std::vector<NetworkStatePropertiesPtr> networks;

  NetworkStatePropertiesPtr ethernet_network =
      CreateStandaloneNetworkProperties(kEthernet, NetworkType::kEthernet,
                                        ConnectionStateType::kNotConnected);
  networks.push_back(std::move(ethernet_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/1,
                           /*mobile_network_count=*/-1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/0u,
                       /*guid=*/kEthernet);

  // Add mobile network.
  AddEuicc();
  SetupCellular();
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  networks.push_back(std::move(cellular_network));
  UpdateNetworkList(networks);
  CheckNetworkListOrdering(/*ethernet_network_count=*/1,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);

  // Mobile list item will be at index 3 after ethernet, separator and header.
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/3u,
                       /*guid=*/kCellularName);

  ethernet_network = CreateStandaloneNetworkProperties(
      kEthernet2, NetworkType::kEthernet, ConnectionStateType::kNotConnected);
  networks.push_back(std::move(ethernet_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/2,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/0u,
                       /*guid=*/kEthernet);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/1u,
                       /*guid=*/kEthernet2);

  // Mobile list item will be at index 4 after ethernet, separator and header.
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/4u,
                       /*guid=*/kCellularName);
}

TEST_F(NetworkListViewControllerTest, HasCorrectWifiNetworkList) {
  std::vector<NetworkStatePropertiesPtr> networks;

  // Add an enabled wifi device.
  AddWifiDevice();

  // Add Wifi network.
  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWifiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected);
  networks.push_back(std::move(wifi_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/-1,
                           /*wifi_network_count=*/1);

  // Wifi list item will be at index 1 after Wifi header.
  CheckNetworkListItem(NetworkType::kWiFi, /*index=*/1u, /*guid=*/kWifiName);

  // Add mobile network.
  AddEuicc();
  SetupCellular();
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  networks.push_back(std::move(cellular_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/1);

  // Wifi list item be at index 4 after Mobile header, Mobile network
  // item, Wifi separator and header.
  CheckNetworkListItem(NetworkType::kWiFi, /*index=*/4u, /*guid=*/kWifiName);

  // Add a second Wifi network.
  wifi_network = CreateStandaloneNetworkProperties(
      kWifiName2, NetworkType::kWiFi, ConnectionStateType::kNotConnected);
  networks.push_back(std::move(wifi_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/2);
  CheckNetworkListItem(NetworkType::kWiFi, /*index=*/4u, /*guid=*/kWifiName);
  CheckNetworkListItem(NetworkType::kWiFi, /*index=*/5u, /*guid=*/kWifiName2);
}

TEST_F(NetworkListViewControllerTest,
       CellularStatusMessageAndToggleButtonState) {
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  AddEuicc();
  SetupCellular();

  // Update cellular device state to be Uninitialized.
  network_state_helper()->manager_test()->SetTechnologyInitializing(
      shill::kTypeCellular, /*initializing=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_FALSE(GetMobileToggleButton()->GetVisible());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR),
      GetMobileStatusMessage()->label()->GetText());

  network_state_helper()->manager_test()->SetTechnologyInitializing(
      shill::kTypeCellular, /*initializing=*/false);
  base::RunLoop().RunUntilIdle();

  SetupCellular();
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_NE(nullptr, GetMobileSubHeader());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // No message is shown when there are available networks.
  std::vector<NetworkStatePropertiesPtr> networks;
  networks.push_back(CreateStandaloneNetworkProperties(
      kCellularName, NetworkType::kCellular, ConnectionStateType::kConnected));
  UpdateNetworkList(networks);
  EXPECT_EQ(nullptr, GetMobileStatusMessage());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // Message shown again when list is empty.
  UpdateNetworkList(empty_list_);
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // No message is shown when inhibited.
  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock =
      InhibitCellularScanning();
  EXPECT_TRUE(inhibit_lock);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetMobileStatusMessage());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // Uninhibit the device.
  inhibit_lock.reset();
  base::RunLoop().RunUntilIdle();

  // Message is shown when uninhibited.
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // When device is in disabling message is shown.
  network_state_helper()->manager_test()->SetInteractiveDelay(
      kInteractiveDelay);
  network_state_handler()->SetTechnologyEnabled(
      chromeos::NetworkTypePattern::Cellular(), /*enabled=*/false,
      base::DoNothing());

  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING),
      GetMobileStatusMessage()->label()->GetText());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());
  task_environment()->FastForwardBy(kInteractiveDelay);

  // Message is shown when device is disabled.
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED),
      GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());
}

TEST_F(NetworkListViewControllerTest, HasCorrectTetherStatusMessage) {
  // Mobile section is not shown if Tether network is unavailable.
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  // Tether is enabled but no devices are added.
  network_state_handler()->SetTetherTechnologyState(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_NE(nullptr, GetMobileSubHeader());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND),
      GetMobileStatusMessage()->label()->GetText());

  // Tether network is uninitialized and Bluetooth state enabling.
  network_state_handler()->SetTetherTechnologyState(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  base::RunLoop().RunUntilIdle();

  SetBluetoothAdapterState(BluetoothSystemState::kEnabling);
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR),
      GetMobileStatusMessage()->label()->GetText());

  // Set Bluetooth device to disabling.
  SetBluetoothAdapterState(BluetoothSystemState::kDisabling);
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH),
            GetMobileStatusMessage()->label()->GetText());

  // Simulate login as secondary user and disable Bluetooth device.
  LoginAsSecondaryUser();
  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH),
            GetMobileStatusMessage()->label()->GetText());

  // No message shown when Tether devices are added.
  AddTetherNetworkState();
  EXPECT_EQ(nullptr, GetMobileStatusMessage());
}

TEST_F(NetworkListViewControllerTest, HasCorrectWifiStatusMessage) {
  EXPECT_EQ(nullptr, GetWifiStatusMessage());

  // Add an enabled wifi device.
  AddWifiDevice();

  // Wifi is enabled but not networks are added.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED),
            GetWifiStatusMessage()->label()->GetText());

  // Disable wifi device.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED),
      GetWifiStatusMessage()->label()->GetText());

  // Enable and add wifi network.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  std::vector<NetworkStatePropertiesPtr> networks;
  networks.push_back(CreateStandaloneNetworkProperties(
      kWifiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/-1,
                           /*wifi_network_count=*/1);
}

TEST_F(NetworkListViewControllerTest, HasConnectionWarning) {
  EXPECT_EQ(nullptr, GetConnectionWarning());

  AddVpnDevice();
  std::vector<NetworkStatePropertiesPtr> networks;
  networks.push_back(CreateStandaloneNetworkProperties(
      kVpnName, NetworkType::kVPN, ConnectionStateType::kConnected));
  UpdateNetworkList(networks);

  EXPECT_NE(nullptr, GetConnectionWarning());
  EXPECT_NE(nullptr, GetConnectionLabelView());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());
  EXPECT_EQ(network_list()->children().at(0), GetConnectionWarning());

  // Clear all devices and make sure warning is no longer being shown.
  network_state_helper()->ClearDevices();
  EXPECT_EQ(nullptr, GetConnectionWarning());
}

}  // namespace ash
