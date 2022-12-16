// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/fake_cros_network_config.h"
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
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/session_manager/session_manager_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
using ::chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::GlobalPolicyPtr;
using ::chromeos::network_config::mojom::InhibitReason;
using ::chromeos::network_config::mojom::ManagedPropertiesPtr;
using ::chromeos::network_config::mojom::ManagedString;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PolicySource;
using ::chromeos::network_config::mojom::SIMInfoPtr;
using ::chromeos::network_config::mojom::VpnProviderPtr;
using network_config::CrosNetworkConfigTestHelper;

using ::testing::_;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

const std::string kCellularName = "cellular";
const std::string kCellularName2 = "cellular_2";
const char kCellularTestIccid[] = "1234567890";

const char kTetherName[] = "tether";

const std::string kEthernetName = "ethernet";
const std::string kEthernetName2 = "ethernet_2";

const char kVpnName[] = "vpn";

const char kWifiName[] = "wifi";
const char kWifiName2[] = "wifi_2";

const char kTestBaseEid[] = "12345678901234567890123456789012";

constexpr char kUser1Email[] = "user1@quicksettings.com";

constexpr char kNetworkListNetworkItemView[] = "NetworkListNetworkItemView";

// Delay used to simulate running process when setting device technology state.
constexpr base::TimeDelta kInteractiveDelay = base::Milliseconds(3000);

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

std::vector<ash::SIMInfoPtr> CellularSIMInfos(const std::string& iccid,
                                              const std::string& eid) {
  auto sim_info_mojo = chromeos::network_config::mojom::SIMInfo::New();
  sim_info_mojo->iccid = iccid;
  sim_info_mojo->eid = eid;
  sim_info_mojo->is_primary = true;
  std::vector<ash::SIMInfoPtr> sim_info_mojos;

  sim_info_mojos.push_back(std::move(sim_info_mojo));

  return sim_info_mojos;
}

ManagedPropertiesPtr CreateManagedPropertiesWithVPN(bool is_managed) {
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
  managed_properties->ip_address_config_type = ManagedString::New();
  managed_properties->name_servers_config_type = ManagedString::New();
  managed_properties->traffic_counter_properties =
      chromeos::network_config::mojom::TrafficCounterProperties::New();
  return managed_properties;
}

ManagedPropertiesPtr CreateManagedPropertiesWithProxy(bool is_managed) {
  auto managed_properties =
      chromeos::network_config::mojom::ManagedProperties::New();
  auto proxy_type = ManagedString::New();
  proxy_type->active_value = "test";
  proxy_type->policy_source =
      is_managed ? PolicySource::kUserPolicyEnforced : PolicySource::kNone;
  auto proxy_settings =
      chromeos::network_config::mojom::ManagedProxySettings::New();
  proxy_settings->type = std::move(proxy_type);
  auto wifi = chromeos::network_config::mojom::ManagedWiFiProperties::New();
  wifi->ssid = ManagedString::New();
  managed_properties->type_properties =
      chromeos::network_config::mojom::NetworkTypeManagedProperties::NewWifi(
          std::move(wifi));
  managed_properties->proxy_settings = std::move(proxy_settings);
  managed_properties->ip_address_config_type = ManagedString::New();
  managed_properties->name_servers_config_type = ManagedString::New();
  managed_properties->traffic_counter_properties =
      chromeos::network_config::mojom::TrafficCounterProperties::New();
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
    AshTestBase::SetUp();

    cros_network_ = std::make_unique<FakeCrosNetworkConfig>();
    Shell::Get()
        ->system_tray_model()
        ->network_state_model()
        ->ConfigureRemoteForTesting(cros_network()->GetPendingRemote());
    base::RunLoop().RunUntilIdle();
    cros_network_->SetGlobalPolicy(
        /*allow_only_policy_cellular_networks=*/false);

    fake_network_detailed_network_view_ =
        std::make_unique<FakeNetworkDetailedNetworkView>(
            /*delegate=*/nullptr);

    network_list_view_controller_impl_ =
        std::make_unique<NetworkListViewControllerImpl>(
            fake_network_detailed_network_view_.get());
  }

  bool IsQsRevampEnabled() { return GetParam(); }

  void TearDown() override {
    network_list_view_controller_impl_.reset();
    fake_network_detailed_network_view_.reset();

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

  views::ImageView* GetConnectionWarningIcon() {
    return FindViewById<views::ImageView*>(
        NetworkListViewControllerImpl::NetworkListViewControllerViewChildId::
            kConnectionWarningIcon);
  }

  views::View* GetViewInNetworkList(std::string id) {
    return network_list_view_controller_impl_->network_id_to_view_map_[id];
  }

  bool getScanningBarVisibility() {
    return fake_network_detailed_network_view_->last_scan_bar_visibility();
  }

  // Checks that network list items are in the right order. Wifi section
  // is always shown.
  void CheckNetworkListOrdering(int ethernet_network_count,
                                int mobile_network_count,
                                int wifi_network_count) {
    EXPECT_THAT(GetWifiSubHeader(), NotNull());

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
      ASSERT_THAT(GetMobileSubHeader(), NotNull());
      if (index > 0) {
        // Expect that the mobile network separator exists.
        ASSERT_THAT(GetMobileSeparator(), NotNull());
        EXPECT_EQ(network_list()->children().at(index++), GetMobileSeparator());
        EXPECT_EQ(network_list()->children().at(index++), GetMobileSubHeader());
      } else {
        EXPECT_THAT(GetMobileSeparator(), IsNull());
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
        ASSERT_THAT(GetMobileStatusMessage(), NotNull());
        index++;
      }
    }

    // Wifi section.
    if (index > 0) {
      // Expect that the wifi network separator exists.
      ASSERT_THAT(GetWifiSeparator(), NotNull());
      EXPECT_EQ(network_list()->children().at(index++), GetWifiSeparator());
      EXPECT_EQ(network_list()->children().at(index++), GetWifiSubHeader());
    } else {
      EXPECT_THAT(GetWifiSeparator(), IsNull());
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
      ASSERT_THAT(GetWifiStatusMessage(), NotNull());
      index++;
    } else {
      // Status message is not shown when WiFi networks are available.
      EXPECT_THAT(GetWifiStatusMessage(), IsNull());
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

  views::View* network_list() {
    return static_cast<NetworkDetailedNetworkView*>(
               fake_network_detailed_network_view_.get())
        ->network_list();
  }

  FakeCrosNetworkConfig* cros_network() { return cros_network_.get(); }

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
  std::unique_ptr<FakeCrosNetworkConfig> cros_network_;
  std::unique_ptr<FakeNetworkDetailedNetworkView>
      fake_network_detailed_network_view_;
  std::unique_ptr<NetworkListViewControllerImpl>
      network_list_view_controller_impl_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         NetworkListViewControllerTest,
                         testing::Bool() /* IsQsRevampEnabled() */);

TEST_P(NetworkListViewControllerTest, MobileDataSectionIsShown) {
  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  EXPECT_THAT(GetMobileSeparator(), IsNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 0);

  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kCellular;
  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 1);

  // Mobile separator is still null because mobile data is at index 0.
  EXPECT_THAT(GetMobileSeparator(), IsNull());

  // Clear device list and check if Mobile subheader is shown with just
  // tether device.
  cros_network()->ClearNetworksAndDevices();

  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 1);

  // Add tether networks
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kTetherName, NetworkType::kTether, ConnectionStateType::kConnected));

  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 2);

  // Tether device is prohibited.
  properties->type = NetworkType::kTether;
  properties->device_state = DeviceStateType::kProhibited;
  cros_network()->SetDeviceProperties(properties.Clone());
  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 2);

  // Tether device is uninitialized but is primary user.
  properties->device_state = DeviceStateType::kUninitialized;
  cros_network()->SetDeviceProperties(properties.Clone());
  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 3);

  // Simulate login as secondary user.
  LoginAsSecondaryUser();
  cros_network()->ClearNetworksAndDevices();

  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 3);

  // Add tether networks
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kTetherName, NetworkType::kTether, ConnectionStateType::kConnected));
  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kMobileSection, 4);
}

TEST_P(NetworkListViewControllerTest, WifiSectionHeader) {
  EXPECT_THAT(GetWifiSubHeader(), IsNull());
  EXPECT_THAT(GetWifiSeparator(), IsNull());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 0);

  // Add an enabled wifi device.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected));

  ASSERT_THAT(GetWifiSubHeader(), NotNull());
  EXPECT_THAT(GetWifiSeparator(), IsNull());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetWifiSubHeader()->is_join_wifi_enabled());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 1);

  // Disable wifi device.
  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kWiFi;
  properties->device_state = DeviceStateType::kDisabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetWifiSubHeader(), NotNull());
  EXPECT_TRUE(GetWifiToggleButton()->GetVisible());
  EXPECT_TRUE(GetWifiSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetWifiSubHeader()->is_toggle_on());
  EXPECT_FALSE(GetWifiSubHeader()->is_join_wifi_enabled());
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kWifiSection, 1);
}

TEST_P(NetworkListViewControllerTest, MobileSectionHeaderAddEsimButtonStates) {
  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());

  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kCellular;
  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_enabled());

  // Since no Euicc was added, this means device is not eSIM capable, do not
  // show add eSIM button.
  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_visible());

  cros_network()->ClearNetworksAndDevices();

  properties->sim_infos = CellularSIMInfos(kCellularTestIccid, kTestBaseEid);
  cros_network()->SetDeviceProperties(properties.Clone());

  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());
  EXPECT_THAT(GetMobileSeparator(), IsNull());
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());

  // Add eSIM button is not enabled when inhibited.
  properties->inhibit_reason = InhibitReason::kResettingEuiccMemory;
  cros_network()->SetDeviceProperties(properties.Clone());

  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());

  // Uninhibit the device.
  properties->inhibit_reason = InhibitReason::kNotInhibited;
  cros_network()->SetDeviceProperties(properties.Clone());

  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_add_esim_visible());

  // When no Mobile networks are available and eSIM policy is set to allow only
  // cellular devices which means adding a new eSIM is disallowed by enterprise
  // policy, add eSIM button is not displayed.
  cros_network()->SetGlobalPolicy(/*allow_only_policy_cellular_networks=*/true);

  EXPECT_FALSE(GetMobileSubHeader()->is_add_esim_visible());
}

TEST_P(NetworkListViewControllerTest, HasCorrectMobileNetworkList) {
  EXPECT_EQ(0u, network_list()->children().size());
  EXPECT_THAT(GetMobileSubHeader(), IsNull());
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());

  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kCellular;
  properties->device_state = DeviceStateType::kEnabled;
  properties->sim_infos = CellularSIMInfos(kCellularTestIccid, kTestBaseEid);
  cros_network()->SetDeviceProperties(properties.Clone());

  properties = chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kWiFi;
  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/0,
                           /*wifi_network_count=*/0);

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kCellularName, NetworkType::kCellular,
          ConnectionStateType::kConnected));

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/1u,
                       /*guid=*/kCellularName);

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kCellularName2, NetworkType::kCellular,
          ConnectionStateType::kConnected));

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/2,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/2u,
                       /*guid=*/kCellularName2);

  // Update a network and make sure it is still in network list.
  cros_network()->SetNetworkState(kCellularName,
                                  ConnectionStateType::kNotConnected);

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/2,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/1u,
                       /*guid=*/kCellularName);
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/2u,
                       /*guid=*/kCellularName2);

  // Remove all networks and add Tether networks. Only one network should be in
  // list.
  cros_network()->ClearNetworksAndDevices();

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kTetherName, NetworkType::kTether, ConnectionStateType::kConnected));

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

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kEthernetName, NetworkType::kEthernet,
          ConnectionStateType::kNotConnected));

  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 1);

  CheckNetworkListOrdering(/*ethernet_network_count=*/1,
                           /*mobile_network_count=*/-1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/0u,
                       /*guid=*/kEthernetName);

  // Add mobile network.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kCellularName, NetworkType::kCellular,
          ConnectionStateType::kConnected));

  CheckNetworkListOrdering(/*ethernet_network_count=*/1,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);

  // Mobile list item will be at index 3 after ethernet, separator and header.
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/3u,
                       /*guid=*/kCellularName);

  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kEthernetName2, NetworkType::kEthernet,
          ConnectionStateType::kNotConnected));

  // Metrics is only recorded the first time ethernet section is shown. Here a
  // new ethernet network was added but the section was already being shown, so
  // no new metric would be recorded.
  histogram_tester.ExpectBucketCount("ChromeOS.SystemTray.Network.SectionShown",
                                     DetailedViewSection::kEthernetSection, 1);

  CheckNetworkListOrdering(/*ethernet_network_count=*/2,
                           /*mobile_network_count=*/1,
                           /*wifi_network_count=*/0);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/0u,
                       /*guid=*/kEthernetName);
  CheckNetworkListItem(NetworkType::kEthernet, /*index=*/1u,
                       /*guid=*/kEthernetName2);

  // Mobile list item will be at index 4 after ethernet, separator and header.
  CheckNetworkListItem(NetworkType::kCellular, /*index=*/4u,
                       /*guid=*/kCellularName);
}

TEST_P(NetworkListViewControllerTest, HasCorrectWifiNetworkList) {
  // Add an enabled wifi device.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected));

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
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kCellularName, NetworkType::kCellular,
          ConnectionStateType::kConnected));

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
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName2, NetworkType::kWiFi, ConnectionStateType::kNotConnected));

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
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());

  // Update cellular device state to be Uninitialized.
  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kCellular;
  properties->sim_infos = CellularSIMInfos(kCellularTestIccid, kTestBaseEid);
  properties->device_state = DeviceStateType::kUninitialized;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR),
      GetMobileStatusMessage()->label()->GetText());

  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // No message is shown when there are available networks.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kCellularName, NetworkType::kCellular,
          ConnectionStateType::kConnected));

  EXPECT_THAT(GetMobileStatusMessage(), IsNull());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // Message shown again when list is empty.
  cros_network()->ClearNetworksAndDevices();
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // No message is shown when inhibited.
  properties->inhibit_reason = InhibitReason::kResettingEuiccMemory;
  cros_network()->SetDeviceProperties(properties.Clone());
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // Uninhibit the device.
  properties->inhibit_reason = InhibitReason::kNotInhibited;
  cros_network()->SetDeviceProperties(properties.Clone());

  // Message is shown when uninhibited.
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS),
            GetMobileStatusMessage()->label()->GetText());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  // When device is in disabling message is shown.
  properties->device_state = DeviceStateType::kDisabling;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING),
      GetMobileStatusMessage()->label()->GetText());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_TRUE(GetMobileToggleButton()->GetVisible());

  properties->device_state = DeviceStateType::kDisabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  // Message is shown when device is disabled.
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
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
  properties->sim_lock_status =
      chromeos::network_config::mojom::SIMLockStatus::New();
  properties->sim_lock_status->lock_type = "lock";
  cros_network()->SetDeviceProperties(properties.Clone());

  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
}

TEST_P(NetworkListViewControllerTest, HasCorrectTetherStatusMessage) {
  // Mobile section is not shown if Tether network is unavailable.
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());

  // Tether is enabled but no devices are added.
  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kTether;
  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  ASSERT_THAT(GetMobileSubHeader(), NotNull());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND),
      GetMobileStatusMessage()->label()->GetText());

  // Tether network is uninitialized and Bluetooth state enabling.
  properties->device_state = DeviceStateType::kUninitialized;
  cros_network()->SetDeviceProperties(properties.Clone());
  SetBluetoothAdapterState(BluetoothSystemState::kEnabling);
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_on());
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR),
      GetMobileStatusMessage()->label()->GetText());

  // Set Bluetooth device to disabling.
  SetBluetoothAdapterState(BluetoothSystemState::kDisabling);
  EXPECT_TRUE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH),
            GetMobileStatusMessage()->label()->GetText());

  // Simulate login as secondary user and disable Bluetooth device.
  LoginAsSecondaryUser();
  SetBluetoothAdapterState(BluetoothSystemState::kDisabled);
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_enabled());
  EXPECT_FALSE(GetMobileSubHeader()->is_toggle_on());
  ASSERT_THAT(GetMobileStatusMessage(), NotNull());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH),
            GetMobileStatusMessage()->label()->GetText());

  // No message shown when Tether devices are added.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kTetherName, NetworkType::kTether, ConnectionStateType::kConnected));
  EXPECT_THAT(GetMobileStatusMessage(), IsNull());
}

TEST_P(NetworkListViewControllerTest, HasCorrectWifiStatusMessage) {
  EXPECT_THAT(GetWifiStatusMessage(), IsNull());

  // Add an enabled wifi device.
  auto properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  properties->type = NetworkType::kWiFi;
  properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  // Wifi is enabled but not networks are added.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED),
            GetWifiStatusMessage()->label()->GetText());

  // Disable wifi device.
  properties->device_state = DeviceStateType::kDisabled;
  cros_network()->SetDeviceProperties(properties.Clone());

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED),
      GetWifiStatusMessage()->label()->GetText());

  // Enable and add wifi network.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected));

  CheckNetworkListOrdering(/*ethernet_network_count=*/0,
                           /*mobile_network_count=*/-1,
                           /*wifi_network_count=*/1);
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningSystemIconVpn) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());
  cros_network()->AddManagedProperties(
      kVpnName, CreateManagedPropertiesWithVPN(/*is_managed=*/false));
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kVpnName, NetworkType::kVPN, ConnectionStateType::kConnected));

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());
  EXPECT_EQ(network_list()->children().at(0), GetConnectionWarning());
  views::ImageView* icon = GetConnectionWarningIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsSystemIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  cros_network()->ClearNetworksAndDevices();
  EXPECT_THAT(GetConnectionWarning(), IsNull());
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningManagedIconVpn) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  cros_network()->AddManagedProperties(
      kVpnName, CreateManagedPropertiesWithVPN(/*is_managed=*/true));
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kVpnName, NetworkType::kVPN, ConnectionStateType::kConnected));

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());
  EXPECT_EQ(network_list()->children().at(0), GetConnectionWarning());
  views::ImageView* icon = GetConnectionWarningIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  cros_network()->ClearNetworksAndDevices();
  EXPECT_THAT(GetConnectionWarning(), IsNull());
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningSystemIconProxy) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  cros_network()->AddManagedProperties(
      kWifiName, CreateManagedPropertiesWithProxy(/*is_managed=*/false));
  auto network = CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
      kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected);
  network->proxy_mode = chromeos::network_config::mojom::ProxyMode::kAutoDetect;
  cros_network()->AddNetworkAndDevice(std::move(network));

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING),
      GetConnectionLabelView()->GetText());

  views::ImageView* icon = GetConnectionWarningIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsSystemIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  cros_network()->ClearNetworksAndDevices();
  EXPECT_THAT(GetConnectionWarning(), IsNull());
}

TEST_P(NetworkListViewControllerTest, ConnectionWarningManagedIconProxy) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());

  cros_network()->AddManagedProperties(
      kWifiName, CreateManagedPropertiesWithProxy(/*is_managed=*/true));
  auto network = CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
      kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected);
  network->proxy_mode = chromeos::network_config::mojom::ProxyMode::kAutoDetect;
  cros_network()->AddNetworkAndDevice(std::move(network));

  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());

  views::ImageView* icon = GetConnectionWarningIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));

  // Clear all devices and make sure warning is no longer being shown.
  cros_network()->ClearNetworksAndDevices();
  EXPECT_THAT(GetConnectionWarning(), IsNull());
}

TEST_P(NetworkListViewControllerTest,
       ConnectionWarningDnsTemplateUriWithIdentifier) {
  EXPECT_THAT(GetConnectionWarning(), IsNull());
  auto network = CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
      kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected);
  network->dns_queries_monitored = true;
  cros_network()->AddNetworkAndDevice(std::move(network));

  views::ImageView* icon = GetConnectionWarningIcon();
  ASSERT_THAT(icon, NotNull());
  EXPECT_TRUE(IsManagedIcon(icon));
  ASSERT_THAT(GetConnectionWarning(), NotNull());
  ASSERT_THAT(GetConnectionLabelView(), NotNull());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING),
      GetConnectionLabelView()->GetText());
}

TEST_P(NetworkListViewControllerTest, NetworkScanning) {
  cros_network()->ClearNetworksAndDevices();
  int initial_wifi_count = 0;
  int initial_tether_count = 0;

  // Scanning bar is not visible if WiFi is not enabled.
  EXPECT_FALSE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_wifi_count + 0,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_wifi_count + 0,
            cros_network()->GetScanCount(NetworkType::kTether));

  auto wifi_properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  wifi_properties->type = NetworkType::kWiFi;
  wifi_properties->scanning = true;
  wifi_properties->device_state = DeviceStateType::kEnabled;
  cros_network()->SetDeviceProperties(wifi_properties.Clone());

  auto tether_properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  tether_properties->type = NetworkType::kTether;
  tether_properties->scanning = true;
  cros_network()->SetDeviceProperties(tether_properties.Clone());

  // Add an enabled WiFi device.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName, NetworkType::kWiFi, ConnectionStateType::kConnected));
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_TRUE(getScanningBarVisibility());
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kTether));

  // Simulate scanning finishing.
  wifi_properties->scanning = false;
  cros_network()->SetDeviceProperties(wifi_properties.Clone());
  tether_properties->scanning = false;
  cros_network()->SetDeviceProperties(tether_properties.Clone());

  // Simulate scanning finishing.
  task_environment()->FastForwardBy(kInteractiveDelay);

  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_tether_count + 1,
            cros_network()->GetScanCount(NetworkType::kTether));

  // Make sure scan timer is still running.
  task_environment()->FastForwardBy(kInteractiveDelay);
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_tether_count + 1,
            cros_network()->GetScanCount(NetworkType::kTether));

  task_environment()->FastForwardBy(kInteractiveDelay);
  EXPECT_TRUE(HasScanTimerStarted());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_tether_count + 1,
            cros_network()->GetScanCount(NetworkType::kTether));

  // Disabling WiFi device ends scan timer.

  wifi_properties->device_state = DeviceStateType::kDisabled;
  cros_network()->SetDeviceProperties(wifi_properties.Clone());
  EXPECT_FALSE(getScanningBarVisibility());
  EXPECT_FALSE(HasScanTimerStarted());
  EXPECT_EQ(initial_wifi_count + 1,
            cros_network()->GetScanCount(NetworkType::kWiFi));
  EXPECT_EQ(initial_tether_count + 1,
            cros_network()->GetScanCount(NetworkType::kTether));
}

}  // namespace ash
