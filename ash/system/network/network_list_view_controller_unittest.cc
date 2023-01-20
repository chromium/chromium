// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include <cstddef>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/fake_network_detailed_network_view.h"
#include "ash/system/network/fake_network_list_mobile_header_view.h"
#include "ash/system/network/fake_network_list_wifi_header_view.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using bluetooth_config::ScopedBluetoothConfigTestHelper;
using bluetooth_config::mojom::BluetoothSystemState;
using ::chromeos::network_config::NetworkTypeMatchesType;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::ManagedPropertiesPtr;
using ::chromeos::network_config::mojom::ManagedString;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PolicySource;
using ::testing::_;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

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

class TestNetworkStateHandlerObserver : public NetworkStateHandlerObserver {
 public:
  TestNetworkStateHandlerObserver() = default;

  TestNetworkStateHandlerObserver(const TestNetworkStateHandlerObserver&) =
      delete;
  TestNetworkStateHandlerObserver& operator=(
      const TestNetworkStateHandlerObserver&) = delete;

  // NetworkStateHandlerObserver:
  void ScanRequested(const NetworkTypePattern& type) override {
    scan_request_count_++;

    if (type.MatchesPattern(NetworkTypePattern::WiFi())) {
      wifi_scan_request_count_++;
    }

    if (type.MatchesPattern(NetworkTypePattern::Tether())) {
      tether_scan_request_count_++;
    }
  }

  // Returns the number of ScanRequested() call.
  size_t scan_request_count() { return scan_request_count_; }

  size_t wifi_scan_request_count() { return wifi_scan_request_count_; }

  size_t tether_scan_request_count() { return tether_scan_request_count_; }

 private:
  size_t scan_request_count_ = 0;
  size_t wifi_scan_request_count_ = 0;
  size_t tether_scan_request_count_ = 0;
};

bool IsManagedIcon(views::ImageView* icon) {
  const gfx::ImageSkia managed_icon = gfx::CreateVectorIcon(
      kSystemTrayManagedIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
  return gfx::BitmapsAreEqual(*icon->GetImage().bitmap(),
                              *managed_icon.bitmap());
}

bool IsSystemIcon(views::ImageView* icon) {
  const gfx::ImageSkia system_icon = gfx::CreateVectorIcon(
      kSystemMenuInfoIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
  return gfx::BitmapsAreEqual(*icon->GetImage().bitmap(),
                              *system_icon.bitmap());
}

NetworkStatePropertiesPtr GetDefaultNetworkWithProxy(const std::string& guid) {
  auto default_network =
      chromeos::network_config::mojom::NetworkStateProperties::New();
  default_network->guid = guid;
  default_network->proxy_mode =
      ::chromeos::network_config::mojom::ProxyMode::kAutoDetect;

  return default_network;
}

ManagedPropertiesPtr GetManagedNetworkPropertiesWithVPN(bool is_managed) {
  auto managed_properties =
      chromeos::network_config::mojom::ManagedProperties::New();
  auto host = ManagedString::New();
  host->active_value = "test";
  host->policy_source =
      is_managed ? PolicySource::kUserPolicyEnforced : PolicySource::kNone;
  auto vpn = chromeos::network_config::mojom::ManagedVPNProperties::New();
  vpn->host = std::move(host);
  managed_properties->type_properties =
      chromeos::network_config::mojom::NetworkTypeManagedProperties::NewVpn(
          std::move(vpn));
  return managed_properties;
}

ManagedPropertiesPtr GetManagedNetworkPropertiesWithProxy(bool is_managed) {
  auto managed_properties =
      chromeos::network_config::mojom::ManagedProperties::New();
  auto proxy_type = ManagedString::New();
  proxy_type->active_value = "test";
  proxy_type->policy_source =
      is_managed ? PolicySource::kUserPolicyEnforced : PolicySource::kNone;
  auto proxy_settings =
      chromeos::network_config::mojom::ManagedProxySettings::New();
  proxy_settings->type = std::move(proxy_type);
  managed_properties->proxy_settings = std::move(proxy_settings);
  return managed_properties;
}

}  // namespace

class NetworkListViewControllerTest : public AshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  NetworkListViewControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  NetworkListViewControllerTest(const NetworkListViewControllerTest&) = delete;
  NetworkListViewControllerTest& operator=(
      const NetworkListViewControllerTest&) = delete;
  ~NetworkListViewControllerTest() override = default;

  void SetUp() override {
    if (IsQsRevampEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kQsRevamp, features::kQsRevampWip,
           features::kQuickSettingsNetworkRevamp},
          {});
    } else {
      feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);
    }

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

    fake_network_detailed_network_view_ =
        std::make_unique<FakeNetworkDetailedNetworkView>(
            /*delegate=*/nullptr);

    network_list_view_controller_impl_ =
        std::make_unique<NetworkListViewControllerImpl>(
            fake_network_detailed_network_view_.get());

    network_state_handler_observer_ =
        std::make_unique<TestNetworkStateHandlerObserver>();
    network_state_handler()->AddObserver(network_state_handler_observer_.get());
  }

  bool IsQsRevampEnabled() { return GetParam(); }

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

    NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        base::ListValue(), global_config_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_handler()->RemoveObserver(
        network_state_handler_observer_.get());
    network_state_handler_observer_.reset();
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
            kMobileSeparator);
  }

  FakeNetworkListWifiHeaderView* GetWifiSubHeader() {
    return FindViewById<FakeNetworkListWifiHeaderView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kWifiSectionHeader);
  }

  views::Separator* GetWifiSeparator() {
    return FindViewById<views::Separator*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kWifiSeparator);
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

  views::ImageView* GetConnectionWarningSystemIcon() {
    return FindViewById<views::ImageView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kConnectionWarningSystemIcon);
  }
  views::ImageView* GetConnectionWarningManagedIcon() {
    return FindViewById<views::ImageView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kConnectionWarningManagedIcon);
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
      if (IsQsRevampEnabled()) {
        // There's a wifi group label above the item view.
        CheckNetworkListItem(NetworkType::kWiFi, index + 1,
                             /*guid=*/absl::nullopt);
        EXPECT_STREQ(network_list()->children().at(index + 1)->GetClassName(),
                     kNetworkListNetworkItemView);
        index++;
      } else {
        CheckNetworkListItem(NetworkType::kWiFi, index, /*guid=*/absl::nullopt);
        EXPECT_STREQ(network_list()->children().at(index++)->GetClassName(),
                     kNetworkListNetworkItemView);
      }
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

    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoICCID, kCellularTestIccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, true);
    slot_info_item.Set(shill::kSIMSlotInfoEID, kTestBaseEid);
    sim_slot_infos.Append(std::move(slot_info_item));
    network_state_helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMSlotInfoProperty,
        base::Value(std::move(sim_slot_infos)), /*notify_changed=*/true);

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

  void SetCellularSimLockStatus(const std::string& lock_type, bool sim_locked) {
    base::Value sim_lock_status(base::Value::Type::DICTIONARY);
    sim_lock_status.SetKey(shill::kSIMLockEnabledProperty,
                           base::Value(sim_locked));
    sim_lock_status.SetKey(shill::kSIMLockTypeProperty, base::Value(lock_type));
    sim_lock_status.SetKey(shill::kSIMLockRetriesLeftProperty, base::Value(3));
    network_state_helper()->device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty,
        std::move(sim_lock_status),
        /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks.
  void AddTetherNetworkState() {
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
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

    network_state_helper()->device_test()->SetDeviceProperty(
        kWifiDevicePath, shill::kScanningProperty, base::Value(true),
        /*notify_changed=*/true);

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  bool getScanningBarVisibility() {
    return fake_network_detailed_network_view_->last_scan_bar_visibility();
  }

  size_t GetScanCount() {
    return network_state_handler_observer_->scan_request_count();
  }

  size_t GetWifiScanCount() {
    return network_state_handler_observer_->wifi_scan_request_count();
  }

  size_t GetTetherScanCount() {
    return network_state_handler_observer_->tether_scan_request_count();
  }

  std::unique_ptr<CellularInhibitor::InhibitLock> InhibitCellularScanning() {
    base::RunLoop inhibit_loop;
    CellularInhibitor::InhibitReason inhibit_reason =
        CellularInhibitor::InhibitReason::kInstallingProfile;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    NetworkHandler::Get()->cellular_inhibitor()->InhibitCellularScanning(
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

  bool GetNetworkListItemIsEnabled(NetworkType type, size_t index) {
    EXPECT_STREQ(network_list()->children().at(index)->GetClassName(),
                 kNetworkListNetworkItemView);

    NetworkListNetworkItemView* network =
        static_cast<NetworkListNetworkItemView*>(
            network_list()->children().at(index));

    return network->GetEnabled();
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

  bool HasScanTimerStarted() {
    return network_list_view_controller_impl_->network_scan_repeating_timer_
        .IsRunning();
  }

  void SetDefaultNetworkForTesting(NetworkStatePropertiesPtr default_network) {
    network_list_view_controller_impl_->SetDefaultNetworkForTesting(
        std::move(default_network));
  }

  void SetManagedNetworkPropertiesForTesting(
      ManagedPropertiesPtr managed_properties) {
    network_list_view_controller_impl_->SetManagedNetworkPropertiesForTesting(
        std::move(managed_properties));
  }

  NetworkStateHandler* network_state_handler() {
    return NetworkHandler::Get()->network_state_handler();
  }

  NetworkHandlerTestHelper* network_state_helper() {
    return &network_handler_test_helper_;
  }

  views::View* network_list() {
    return static_cast<NetworkDetailedNetworkView*>(
               fake_network_detailed_network_view_.get())
        ->network_list();
  }

 protected:
  const std::vector<NetworkStatePropertiesPtr> empty_list_;

  base::HistogramTester histogram_tester;

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

  std::unique_ptr<TestNetworkStateHandlerObserver>
      network_state_handler_observer_;

  NetworkHandlerTestHelper network_handler_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         NetworkListViewControllerTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

TEST_P(NetworkListViewControllerTest, MobileDataSectionIsShown) {
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  EXPECT_EQ(nullptr, GetMobileSeparator());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 0);

  AddEuicc();
  SetupCellular();
  EXPECT_NE(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 1);

  // Mobile separator is still null because mobile data is at index 0.
  EXPECT_EQ(nullptr, GetMobileSeparator());

  // Clear device list and check if Mobile subheader is shown with just
  // tether device.
  network_state_helper()->ClearDevices();
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 1);

  // Add tether networks
  AddTetherNetworkState();
  EXPECT_NE(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 2);

  // Tether device is prohibited.
  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 2);

  // Tether device is uninitialized but is primary user.
  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 3);

  // Simulate login as secondary user.
  LoginAsSecondaryUser();
  UpdateNetworkList(empty_list_);
  EXPECT_EQ(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 3);

  // Add tether networks
  AddTetherNetworkState();
  EXPECT_NE(nullptr, GetMobileSubHeader());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 4);
}

TEST_P(NetworkListViewControllerTest, WifiSectionHeader) {
  EXPECT_EQ(nullptr, GetWifiSubHeader());
  EXPECT_EQ(nullptr, GetWifiSeparator());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 0);

  // Add an enabled wifi device.
  AddWifiDevice();

  EXPECT_NE(nullptr, GetWifiSubHeader());
  EXPECT_EQ(nullptr, GetWifiSeparator());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetWifiSubHeader()->is_join_wifi_enabled());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 1);

  // Disable wifi device.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetWifiSubHeader());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_FALSE(GetWifiSubHeader()->is_join_wifi_enabled());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 1);
}

TEST_P(NetworkListViewControllerTest, MobileSectionHeaderAddEsimButtonStates) {
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
  UpdateNetworkList(empty_list_);
  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_visible());
}

TEST_P(NetworkListViewControllerTest, HasCorrectMobileNetworkList) {
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

TEST_P(NetworkListViewControllerTest, HasCorrectEthernetNetworkList) {
  std::vector<NetworkStatePropertiesPtr> networks;
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 0);

  NetworkStatePropertiesPtr ethernet_network =
      CreateStandaloneNetworkProperties(kEthernet, NetworkType::kEthernet,
                                        ConnectionStateType::kNotConnected);
  networks.push_back(std::move(ethernet_network));
  UpdateNetworkList(networks);

  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 1);

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

  // Metrics is recorded here because when AddEuicc() and SetupCellular() are
  // called, model()->cros_network_config()->GetNetworkStateList returns an
  // empty list of networks, this resets the present network list map.
  // The next call to UpdateNetworkList(networks), the views are re-added and
  // a metric is recorded.
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 2);

  // Mobile list item will be at index 3 after ethernet, separator and header.
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/3u,
                       /*guid=*/kCellularName);
  ethernet_network = CreateStandaloneNetworkProperties(
      kEthernet2, NetworkType::kEthernet, ConnectionStateType::kNotConnected);
  networks.push_back(std::move(ethernet_network));
  UpdateNetworkList(networks);

  // Metrics is only recorded the first time ethernet section is shown. Here a
  // new ethernet network was added but the section was already being shown, so
  // no new metric would be recorded.
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 2);

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

TEST_P(NetworkListViewControllerTest, HasCorrectWifiNetworkList) {
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
  if (IsQsRevampEnabled()) {
    EXPECT_EQ(
        u"Unknown networks",
        static_cast<views::Label*>(network_list()->children()[1])->GetText());

    // Wifi list item will be at index 2 after Wifi group label.
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/2u, /*guid=*/kWifiName);
  } else {
    // Wifi list item will be at index 1 after Wifi header.
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/1u, /*guid=*/kWifiName);
  }

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

  if (IsQsRevampEnabled()) {
    EXPECT_EQ(
        u"Unknown networks",
        static_cast<views::Label*>(network_list()->children()[4])->GetText());
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/5u, /*guid=*/kWifiName);
  } else {
    // Wifi list item be at index 4 after Mobile header, Mobile network
    // item, Wifi separator and header.
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/4u, /*guid=*/kWifiName);
  }

  // Add a second Wifi network.
  wifi_network = CreateStandaloneNetworkProperties(
      kWifiName2, NetworkType::kWiFi, ConnectionStateType::kNotConnected);
  networks.push_back(std::move(wifi_network));
  UpdateNetworkList(networks);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/2);
  if (IsQsRevampEnabled()) {
    EXPECT_EQ(
        u"Unknown networks",
        static_cast<views::Label*>(network_list()->children()[4])->GetText());
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/5u, /*guid=*/kWifiName);
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/6u, /*guid=*/kWifiName2);
  } else {
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/4u, /*guid=*/kWifiName);
    CheckNetworkListItem(NetworkType::kWiFi, /*index=*/5u, /*guid=*/kWifiName2);
  }
}

TEST_P(NetworkListViewControllerTest,
       CellularStatusMessageAndToggleButtonState) {
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  AddEuicc();
  SetupCellular();

  // Update cellular device state to be Uninitialized.
  network_state_helper()->manager_test()->SetTechnologyInitializing(
      shill::kTypeCellular, /*initializing=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetMobileStatusMessage());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
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
      NetworkTypePattern::Cellular(), /*enabled=*/false, base::DoNothing());

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

  // The toggle is not enabled, the cellular device SIM is locked, and user
  // cannot open the settings page.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  SetCellularSimLockStatus(shill::kSIMLockPin, /*sim_locked=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
}

TEST_P(NetworkListViewControllerTest, HasCorrectTetherStatusMessage) {
  // Mobile section is not shown if Tether network is unavailable.
  EXPECT_EQ(nullptr, GetMobileStatusMessage());

  // Tether is enabled but no devices are added.
  network_state_handler()->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
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
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED);
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

TEST_P(NetworkListViewControllerTest, HasCorrectWifiStatusMessage) {
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

TEST_P(NetworkListViewControllerTest, ConnectionWarningSystemIconVpn) {
  EXPECT_EQ(nullptr, GetConnectionWarning());

  SetManagedNetworkPropertiesForTesting(GetManagedNetworkPropertiesWithVPN(
      /*is_managed=*/false));
  AddVpnDevice();
  std::vector<NetworkStatePropertiesPtr> networks;
  networks.push_back(CreateStandaloneNetworkProperties(
      kVpnName, NetworkType::kVPN, ConnectionStateType::kConnected));
  UpdateNetworkList(networks);
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());
  EXPECT_EQ(network_list()->children().at(0), GetConnectionWarning());
  views::ImageView* icon = GetConnectionWarningSystemIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsSystemIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  network_state_helper()->ClearDevices();
  EXPECT_EQ(nullptr, GetConnectionWarning());
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningManagedIconVpn) {
  EXPECT_EQ(nullptr, GetConnectionWarning());

  SetManagedNetworkPropertiesForTesting(GetManagedNetworkPropertiesWithVPN(
      /*is_managed=*/true));
  AddVpnDevice();
  std::vector<NetworkStatePropertiesPtr> networks;
  networks.push_back(CreateStandaloneNetworkProperties(
      kVpnName, NetworkType::kVPN, ConnectionStateType::kConnected));
  UpdateNetworkList(networks);
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());
  EXPECT_EQ(network_list()->children().at(0), GetConnectionWarning());
  views::ImageView* icon = GetConnectionWarningManagedIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  network_state_helper()->ClearDevices();
  EXPECT_EQ(nullptr, GetConnectionWarning());
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningSystemIconProxy) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  SetDefaultNetworkForTesting(GetDefaultNetworkWithProxy(kWifiName));
  SetManagedNetworkPropertiesForTesting(
      GetManagedNetworkPropertiesWithProxy(/*is_managed*/ false));
  AddWifiDevice();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());

  views::ImageView* icon = GetConnectionWarningSystemIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsSystemIcon(icon));
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningManagedIconProxy) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  SetDefaultNetworkForTesting(GetDefaultNetworkWithProxy(kWifiName));
  SetManagedNetworkPropertiesForTesting(GetManagedNetworkPropertiesWithProxy(
      /*is_managed=*/true));
  AddWifiDevice();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());

  views::ImageView* icon = GetConnectionWarningManagedIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));
}

// Disconnect and re-connect a network that shows a warning.
// Regression test for b/263803248.
TEST_P(NetworkListViewControllerTest, ConnectionWarningDisconnectReconnect) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  SetDefaultNetworkForTesting(GetDefaultNetworkWithProxy(kWifiName));
  SetManagedNetworkPropertiesForTesting(GetManagedNetworkPropertiesWithProxy(
      /*is_managed=*/true));
  AddWifiDevice();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());

  {
    views::ImageView* icon = GetConnectionWarningManagedIcon();
    ASSERT_THAT(icon, NotNull());
    EXPECT_TRUE(IsManagedIcon(icon));
  }

  // Disconnect the network and check that no warning is shown.
  SetDefaultNetworkForTesting(nullptr);
  SetManagedNetworkPropertiesForTesting(nullptr);
  network_state_helper()->ClearDevices();
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  // Reconnect the network. This should not crash (regression test for
  // b/263803248). Afterwards, the warning should be shown again.
  SetDefaultNetworkForTesting(GetDefaultNetworkWithProxy(kWifiName));
  SetManagedNetworkPropertiesForTesting(GetManagedNetworkPropertiesWithProxy(
      /*is_managed=*/true));
  AddWifiDevice();

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());
  {
    views::ImageView* icon = GetConnectionWarningManagedIcon();
    ASSERT_THAT(icon, NotNull());
    EXPECT_TRUE(IsManagedIcon(icon));
  }
}

TEST_P(NetworkListViewControllerTest,
       ConnectionWarningDnsTemplateUriWithIdentifier) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());
  auto default_network =
      chromeos::network_config::mojom::NetworkStateProperties::New();
  default_network->guid = kWifiName;
  default_network->dns_queries_monitored = true;
  SetDefaultNetworkForTesting(std::move(default_network));

  AddWifiDevice();
  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());

  views::ImageView* icon = GetConnectionWarningManagedIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));
}

TEST_P(NetworkListViewControllerTest, NetworkScanning) {
  network_state_helper()->ClearDevices();
  network_state_helper()->manager_test()->SetInteractiveDelay(
      kInteractiveDelay);

  // ClearDevices() calls RunUntilIdle which performs some initial scans.
  size_t initial_wifi_count = 1u;
  size_t initial_tether_count = 1u;
  size_t initial_scan_count = 2u;

  // Scanning bar is not visible if WiFi is not enabled.
  EXPECT_FALSE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_scan_count + 0u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 0u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 0u, GetTetherScanCount());

  // Add an enabled WiFi device.
  AddWifiDevice();
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_TRUE(getScanningBarVisibility());
  EXPECT_EQ(initial_scan_count + 2u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 1u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 1u, GetTetherScanCount());

  // Simulate scanning finishing.
  task_environment()->FastForwardBy(kInteractiveDelay);

  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_EQ(initial_scan_count + 2u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 1u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 1u, GetTetherScanCount());

  // Make sure scan timer is still running.
  task_environment()->FastForwardBy(kInteractiveDelay);
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_scan_count + 2u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 1u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 1u, GetTetherScanCount());

  task_environment()->FastForwardBy(kInteractiveDelay);
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_scan_count + 2u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 1u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 1u, GetTetherScanCount());

  // Disabling WiFi device ends scan timer.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_FALSE(HasScanTimerStarted());
  EXPECT_EQ(initial_scan_count + 2u, GetScanCount());
  EXPECT_EQ(initial_wifi_count + 1u, GetWifiScanCount());
  EXPECT_EQ(initial_tether_count + 1u, GetTetherScanCount());
}

TEST_P(NetworkListViewControllerTest, NetworkItemIsEnabled) {
  AddEuicc();
  SetupCellular();
  ASSERT_THAT(GetMobileSubHeader(), NotNull());

  std::vector<NetworkStatePropertiesPtr> networks;

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  cellular_network->prohibited_by_policy = false;
  networks.push_back(std::move(cellular_network));
  UpdateNetworkList(networks);

  CheckNetworkListItem(NetworkType::kCellular, /*index=*/1u, kCellularName);
  EXPECT_TRUE(GetNetworkListItemIsEnabled(NetworkType::kCellular, 1u));

  networks.front()->prohibited_by_policy = true;
  UpdateNetworkList(networks);
  EXPECT_FALSE(GetNetworkListItemIsEnabled(NetworkType::kCellular, 1u));

  networks.front()->prohibited_by_policy = false;
  UpdateNetworkList(networks);
  EXPECT_TRUE(GetNetworkListItemIsEnabled(NetworkType::kCellular, 1u));
}

}  // namespace ash
