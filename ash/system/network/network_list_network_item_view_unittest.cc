// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_item_view.h"

#include <memory>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/network/fake_network_detailed_network_view.h"
#include "ash/system/network/network_icon.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ActivationStateType;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::OncSource;
using ::chromeos::network_config::mojom::PortalState;
using ::chromeos::network_config::mojom::SecurityType;
using network_config::CrosNetworkConfigTestHelper;

const std::string kWiFiName = "WiFi";
const std::string kCellularName = "cellular";
const std::string kTetherName = "tether";
const std::string kEid = "sim_eid";
const std::string kEthernetName = "ethernet";

const char kEthernetDeviceName[] = "ethernet_device";
const char kEthernetDevicePath[] = "/device/ethernet_device";
const char kWiFiDeviceName[] = "wifi_device";
const char kWiFiDevicePath[] = "/device/wifi_device";
const char kCellularDeviceName[] = "cellular_device";
const char kCellularDevicePath[] = "/device/cellular_device";

int kSignalStrength = 50;

}  // namespace

class NetworkListNetworkItemViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    SetUpDefaultNetworkDevices();

    fake_network_detailed_network_view_ =
        std::make_unique<FakeNetworkDetailedNetworkView>(
            /*delegate=*/nullptr);

    std::unique_ptr<NetworkListNetworkItemView> network_list_network_item_view =
        std::make_unique<NetworkListNetworkItemView>(
            fake_network_detailed_network_view_.get());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
        kWiFiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected);

    network_list_network_item_view_ =
        widget_->SetContentsView(std::move(network_list_network_item_view));
    network_list_network_item_view_->UpdateViewForNetwork(wifi_network);
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  std::vector<ConnectionStateType> GetConnectionStateTypes() {
    return {ConnectionStateType::kConnected, ConnectionStateType::kConnecting,
            ConnectionStateType::kNotConnected, ConnectionStateType::kOnline,
            ConnectionStateType::kPortal};
  }

  std::vector<OncSource> GetPolicies() {
    return {OncSource::kDevicePolicy, OncSource::kNone};
  }

  std::vector<PortalState> GetPortalStates() {
    return {PortalState::kPortal, PortalState::kNoInternet};
  }

  const NetworkListItemView* LastClickedNetworkListItem() {
    return fake_network_detailed_network_view_
        ->last_clicked_network_list_item();
  }

  NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      NetworkType type,
      ConnectionStateType connection_state) {
    return network_config_helper_.CreateStandaloneNetworkProperties(
        id, type, connection_state, kSignalStrength);
  }

  void UpdateViewForNetwork(NetworkStatePropertiesPtr& network) {
    network_list_network_item_view()->UpdateViewForNetwork(network);
  }

  void AssertA11yDescription(NetworkStatePropertiesPtr& network_properties,
                             const std::u16string& expected_description) {
    ui::AXNodeData node_data;
    UpdateViewForNetwork(network_properties);
    network_list_network_item_view()
        ->GetViewAccessibility()
        .GetAccessibleNodeData(&node_data);
    std::string a11ydescription =
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    EXPECT_EQ(base::UTF8ToUTF16(a11ydescription), expected_description);
  }

  void NetworkIconChanged() {
    network_list_network_item_view()->NetworkIconChanged();
  }

  NetworkListNetworkItemView* network_list_network_item_view() {
    return network_list_network_item_view_;
  }

  const ui::ColorProvider* GetColorProvider() {
    return widget_->GetColorProvider();
  }

 protected:
  void SetUpDefaultNetworkDevices() {
    network_state_helper()->ClearDevices();
    network_state_helper()->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                                      kCellularDeviceName);
    network_state_helper()->AddDevice(kEthernetDevicePath, shill::kTypeEthernet,
                                      kEthernetDeviceName);
    network_state_helper()->AddDevice(kWiFiDevicePath, shill::kTypeWifi,
                                      kWiFiDeviceName);
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeNetworkDetailedNetworkView>
      fake_network_detailed_network_view_;
  CrosNetworkConfigTestHelper network_config_helper_;
  raw_ptr<NetworkListNetworkItemView, DanglingUntriaged>
      network_list_network_item_view_;
};

TEST_F(NetworkListNetworkItemViewTest, HasCorrectLabel) {
  ASSERT_TRUE(network_list_network_item_view()->text_label());

  EXPECT_EQ(base::UTF8ToUTF16(kWiFiName),
            network_list_network_item_view()->text_label()->GetText());

  const std::string kNewWifiName = "New wifi";

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kNewWifiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected);
  UpdateViewForNetwork(wifi_network);

  EXPECT_EQ(base::UTF8ToUTF16(kNewWifiName),
            network_list_network_item_view()->text_label()->GetText());

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kActivating;
  UpdateViewForNetwork(cellular_network);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING,
                                 base::UTF8ToUTF16(kCellularName)),
      network_list_network_item_view()->text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectNonCellularSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected);

  UpdateViewForNetwork(wifi_network);
  EXPECT_TRUE(network_list_network_item_view()->sub_text_label());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED),
      network_list_network_item_view()->sub_text_label()->GetText());

  wifi_network->connection_state = ConnectionStateType::kConnecting;
  UpdateViewForNetwork(wifi_network);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING),
      network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectCellularSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());
  // Label for pSIM networks that are connected but not yet activated.
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE),
            network_list_network_item_view()->sub_text_label()->GetText());

  // Simulate user logout and check label for pSIM networks that are
  // connected but not activated.
  GetSessionControllerClient()->Reset();
  base::RunLoop().RunUntilIdle();
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_ACTIVATE_AFTER_DEVICE_SETUP),
            network_list_network_item_view()->sub_text_label()->GetText());

  CreateUserSessions(/*session_count=*/1);
  base::RunLoop().RunUntilIdle();

  // Label for unactivated eSIM networks.
  cellular_network->type_state->get_cellular()->eid = kEid;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK),
            network_list_network_item_view()->sub_text_label()->GetText());

  // label for connected unlocked cellular network.
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kActivated;
  cellular_network->type_state->get_cellular()->sim_locked = false;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED),
      network_list_network_item_view()->sub_text_label()->GetText());

  // label for connecting unlocked cellular network.
  cellular_network->connection_state = ConnectionStateType::kConnecting;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING),
      network_list_network_item_view()->sub_text_label()->GetText());

  // label for unconnected unlocked cellular network.
  cellular_network->connection_state = ConnectionStateType::kNotConnected;
  UpdateViewForNetwork(cellular_network);
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  // label for locked cellular network.
  cellular_network->type_state->get_cellular()->sim_locked = true;
  cellular_network->connection_state = ConnectionStateType::kConnected;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK),
            network_list_network_item_view()->sub_text_label()->GetText());

  // label for locked cellular network when user is not logged in.
  GetSessionControllerClient()->Reset();
  base::RunLoop().RunUntilIdle();
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK),
            network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectCarrierLockSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  // Label for carrier locked cellular network.
  cellular_network->type_state->get_cellular()->sim_locked = true;
  cellular_network->type_state->get_cellular()->sim_lock_type = "network-pin";
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CARRIER_LOCKED),
            network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectPortalSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kPortal);
  wifi_network->portal_state = PortalState::kPortal;

  UpdateViewForNetwork(wifi_network);
  EXPECT_TRUE(network_list_network_item_view()->sub_text_label());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN),
      network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectPortalSuspectedSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kPortal);
  wifi_network->portal_state = PortalState::kPortalSuspected;

  UpdateViewForNetwork(wifi_network);
  EXPECT_TRUE(network_list_network_item_view()->sub_text_label());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN),
      network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectNoConnectivitySublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kPortal);
  wifi_network->portal_state = PortalState::kNoInternet;

  UpdateViewForNetwork(wifi_network);
  EXPECT_TRUE(network_list_network_item_view()->sub_text_label());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED_NO_INTERNET),
            network_list_network_item_view()->sub_text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, NotifiesListenerWhenClicked) {
  EXPECT_FALSE(LastClickedNetworkListItem());
  LeftClickOn(network_list_network_item_view());
  EXPECT_EQ(LastClickedNetworkListItem(), network_list_network_item_view());
}

TEST_F(NetworkListNetworkItemViewTest, HasEnterpriseIconWhenBlockedByPolicy) {
  EXPECT_FALSE(network_list_network_item_view()->right_view());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected);
  wifi_network->source = OncSource::kDevicePolicy;
  wifi_network->prohibited_by_policy = true;
  UpdateViewForNetwork(wifi_network);

  ASSERT_TRUE(network_list_network_item_view()->right_view());
  EXPECT_TRUE(network_list_network_item_view()->right_view()->GetVisible());
  ASSERT_TRUE(views::IsViewClass<views::ImageView>(
      network_list_network_item_view()->right_view()));

  const gfx::Image expected_image(CreateVectorIcon(
      kSystemMenuBusinessIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  const gfx::Image actual_image(
      static_cast<views::ImageView*>(
          network_list_network_item_view()->right_view())
          ->GetImage());

  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, actual_image));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED),
            network_list_network_item_view()->text_label()->GetTooltipText());

  wifi_network->source = OncSource::kNone;
  UpdateViewForNetwork(wifi_network);
  ASSERT_FALSE(network_list_network_item_view()->right_view());
}

TEST_F(NetworkListNetworkItemViewTest, HasPoweredIcon) {
  EXPECT_FALSE(network_list_network_item_view()->right_view());
  int battery_percentage = 50;

  NetworkStatePropertiesPtr tether_network = CreateStandaloneNetworkProperties(
      kTetherName, NetworkType::kTether, ConnectionStateType::kConnected);

  tether_network->type_state->get_tether()->battery_percentage =
      battery_percentage;
  UpdateViewForNetwork(tether_network);

  ASSERT_TRUE(network_list_network_item_view()->right_view());
  EXPECT_TRUE(network_list_network_item_view()->right_view()->GetVisible());
  ASSERT_TRUE(views::IsViewClass<views::ImageView>(
      network_list_network_item_view()->right_view()));

  EXPECT_EQ(base::FormatPercent(battery_percentage),
            static_cast<views::ImageView*>(
                network_list_network_item_view()->right_view())
                ->GetTooltipText());
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedA11yText) {
  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected);
  wifi_network->connection_state = ConnectionStateType::kConnected;

  UpdateViewForNetwork(wifi_network);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN,
                                 base::UTF8ToUTF16(kWiFiName)),
      network_list_network_item_view()->GetViewAccessibility().GetCachedName());

  // Network can be connected to.
  wifi_network->connectable = true;
  wifi_network->connection_state = ConnectionStateType::kNotConnected;
  UpdateViewForNetwork(wifi_network);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT,
                                 base::UTF8ToUTF16(kWiFiName)),
      network_list_network_item_view()->GetViewAccessibility().GetCachedName());

  // Activate cellular network A11Y label is shown when a pSIM network is
  // connected but not yet activated.
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);
  cellular_network->connectable = true;
  cellular_network->connection_state = ConnectionStateType::kConnected;
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  UpdateViewForNetwork(cellular_network);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE,
          base::UTF8ToUTF16(kCellularName)),
      network_list_network_item_view()->GetViewAccessibility().GetCachedName());

  // Simulate user logout and check label for pSIM networks that are
  // connected but not activated.
  GetSessionControllerClient()->Reset();
  base::RunLoop().RunUntilIdle();
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE_AFTER_SETUP,
          base::UTF8ToUTF16(kCellularName)),
      network_list_network_item_view()->GetViewAccessibility().GetCachedName());

  CreateUserSessions(/*session_count=*/1);
  base::RunLoop().RunUntilIdle();

  // Contact carrier A11Y label is shown when a eSIM network is connected but
  // not yet activated.
  cellular_network->type_state->get_cellular()->eid = kEid;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_A11Y_UNAVAILABLE_SIM_NETWORK,
          base::UTF8ToUTF16(kCellularName)),
      network_list_network_item_view()->GetViewAccessibility().GetCachedName());
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedDescriptionForEthernet) {
  NetworkStatePropertiesPtr ethernet_network =
      CreateStandaloneNetworkProperties(kEthernetName, NetworkType::kEthernet,
                                        ConnectionStateType::kConnected);
  std::u16string connection_status;
  for (const auto& connection : GetConnectionStateTypes()) {
    ethernet_network->connection_state = connection;

    for (const auto& policy : GetPolicies()) {
      ethernet_network->source = OncSource::kNone;
      switch (connection) {
        case ConnectionStateType::kConnected:
        case ConnectionStateType::kPortal:
        case ConnectionStateType::kOnline: {
          connection_status = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
          AssertA11yDescription(ethernet_network, connection_status);
          ethernet_network->source = policy;
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                ethernet_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
                    connection_status));
          }
          break;
        }
        case ConnectionStateType::kConnecting: {
          connection_status = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
          AssertA11yDescription(ethernet_network, connection_status);
          ethernet_network->source = policy;
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                ethernet_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
                    connection_status));
          }
          break;
        }
        case ConnectionStateType::kNotConnected:
          AssertA11yDescription(
              ethernet_network,
              l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET));
          ethernet_network->source = policy;
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                ethernet_network,
                l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED));
          }
      }
    }
  }
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedDescriptionForTether) {
  int battery_percentage = 50;
  std::u16string connection_status;
  NetworkStatePropertiesPtr tether_network = CreateStandaloneNetworkProperties(
      kTetherName, NetworkType::kTether, ConnectionStateType::kConnected);

  tether_network->type_state->get_tether()->battery_percentage =
      battery_percentage;
  for (const auto& connection : GetConnectionStateTypes()) {
    tether_network->connection_state = connection;
    switch (connection) {
      case ConnectionStateType::kConnected:
      case ConnectionStateType::kPortal:
      case ConnectionStateType::kOnline: {
        connection_status = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
        AssertA11yDescription(
            tether_network,
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
                connection_status, base::FormatPercent(kSignalStrength),
                base::FormatPercent(battery_percentage)));
        break;
      }
      case ConnectionStateType::kConnecting: {
        connection_status = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
        AssertA11yDescription(
            tether_network,
            l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
                connection_status, base::FormatPercent(kSignalStrength),
                base::FormatPercent(battery_percentage)));
        break;
      }
      case ConnectionStateType::kNotConnected:
        AssertA11yDescription(tether_network,
                              l10n_util::GetStringFUTF16(
                                  IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC,
                                  base::FormatPercent(kSignalStrength),
                                  base::FormatPercent(battery_percentage)));
    }
  }
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedDescriptionForCellular) {
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);

  // Cellular is not activated
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  AssertA11yDescription(
      cellular_network,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE));

  // Cellular is not activate and user is not logged in.
  GetSessionControllerClient()->Reset();
  base::RunLoop().RunUntilIdle();
  AssertA11yDescription(
      cellular_network,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_ACTIVATE_AFTER_DEVICE_SETUP));

  CreateUserSessions(/*session_count=*/1);
  base::RunLoop().RunUntilIdle();

  // Cellular is not activated and is an eSIM network.
  cellular_network->type_state->get_cellular()->eid = kEid;
  AssertA11yDescription(
      cellular_network,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK));

  // eSIM is locked.
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kActivated;
  cellular_network->type_state->get_cellular()->sim_locked = true;
  cellular_network->connection_state = ConnectionStateType::kConnected;
  AssertA11yDescription(
      cellular_network,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK));

  // User is not signed in.
  GetSessionControllerClient()->Reset();
  AssertA11yDescription(
      cellular_network,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK));

  cellular_network->type_state->get_cellular()->sim_locked = false;
  std::u16string connection_status;
  for (const auto& connection : GetConnectionStateTypes()) {
    cellular_network->connection_state = connection;
    for (const auto& policy : GetPolicies()) {
      cellular_network->source = policy;
      switch (connection) {
        case ConnectionStateType::kConnected:
        case ConnectionStateType::kPortal:
        case ConnectionStateType::kOnline: {
          connection_status = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
                    connection_status, base::FormatPercent(kSignalStrength)));
          } else {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
                    connection_status, base::FormatPercent(kSignalStrength)));
          }
          break;
        }
        case ConnectionStateType::kConnecting: {
          connection_status = l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
                    connection_status, base::FormatPercent(kSignalStrength)));
          } else {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
                    connection_status, base::FormatPercent(kSignalStrength)));
          }
          break;
        }
        case ConnectionStateType::kNotConnected:
          if (policy == OncSource::kDevicePolicy) {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED,
                    base::FormatPercent(kSignalStrength)));
          } else {
            AssertA11yDescription(
                cellular_network,
                l10n_util::GetStringFUTF16(
                    IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC,
                    base::FormatPercent(kSignalStrength)));
          }
      }
    }
  }
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedDescriptionForWiFi) {
  SecurityType security_types[2] = {SecurityType::kNone, SecurityType::kWepPsk};

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected);

  for (const auto& security : security_types) {
    wifi_network->type_state->get_wifi()->security = security;
    const std::u16string security_label = l10n_util::GetStringUTF16(
        security == SecurityType::kWepPsk
            ? IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SECURED
            : IDS_ASH_STATUS_TRAY_NETWORK_STATUS_UNSECURED);

    for (const auto& connection : GetConnectionStateTypes()) {
      wifi_network->connection_state = connection;
      wifi_network->portal_state = PortalState::kUnknown;  // default
      std::u16string connection_status;
      int desc_id;
      for (const auto& policy : GetPolicies()) {
        wifi_network->source = policy;
        // Set desc_id for portal, online, connecting
        switch (policy) {
          case OncSource::kDevicePolicy:
            desc_id =
                IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS;
            break;
          case OncSource::kNone:
            desc_id =
                IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS;
            break;
          default:
            NOTREACHED();
        }
        switch (connection) {
          case ConnectionStateType::kPortal: {
            for (const auto& portal_state : GetPortalStates()) {
              wifi_network->portal_state = portal_state;
              switch (portal_state) {
                case PortalState::kPortal:
                  connection_status = l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN);
                  break;
                case PortalState::kNoInternet:
                  connection_status = l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED_NO_INTERNET);
                  break;
                default:
                  NOTREACHED();
              }
              AssertA11yDescription(
                  wifi_network, l10n_util::GetStringFUTF16(
                                    desc_id, security_label, connection_status,
                                    base::FormatPercent(kSignalStrength)));
            }
            break;
          }
          case ConnectionStateType::kConnected:
            [[fallthrough]];
          case ConnectionStateType::kOnline: {
            connection_status = l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
            AssertA11yDescription(
                wifi_network, l10n_util::GetStringFUTF16(
                                  desc_id, security_label, connection_status,
                                  base::FormatPercent(kSignalStrength)));
            break;
          }
          case ConnectionStateType::kConnecting: {
            connection_status = l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
            AssertA11yDescription(
                wifi_network, l10n_util::GetStringFUTF16(
                                  desc_id, security_label, connection_status,
                                  base::FormatPercent(kSignalStrength)));
            break;
          }
          case ConnectionStateType::kNotConnected: {
            switch (policy) {
              case OncSource::kDevicePolicy:
                desc_id = IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED;
                break;
              case OncSource::kNone:
                desc_id = IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC;
                break;
              default:
                NOTREACHED();
            }
            AssertA11yDescription(wifi_network,
                                  l10n_util::GetStringFUTF16(
                                      desc_id, security_label,
                                      base::FormatPercent(kSignalStrength)));
            break;
          }
        }
      }
    }
  }
}

TEST_F(NetworkListNetworkItemViewTest, NetworkIconAnimating) {
  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnecting);

  UpdateViewForNetwork(wifi_network);

  EXPECT_FALSE(static_cast<views::ImageView*>(
                   network_list_network_item_view()->left_view())
                   ->GetImage()
                   .isNull());

  // Override current icon with an empty icon, check it is updated when
  // animation starts.
  static_cast<views::ImageView*>(network_list_network_item_view()->left_view())
      ->SetImage(gfx::ImageSkia());

  EXPECT_TRUE(static_cast<views::ImageView*>(
                  network_list_network_item_view()->left_view())
                  ->GetImage()
                  .isNull());

  // Simulate icon animation observer being called.
  NetworkIconChanged();

  EXPECT_FALSE(static_cast<views::ImageView*>(
                   network_list_network_item_view()->left_view())
                   ->GetImage()
                   .isNull());
}

TEST_F(NetworkListNetworkItemViewTest, WiFiIcon) {
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnecting);

  UpdateViewForNetwork(wifi_network);

  gfx::ImageSkia image = static_cast<views::ImageView*>(
                             network_list_network_item_view()->left_view())
                             ->GetImage();

  gfx::Image default_image =
      gfx::Image(network_icon::GetImageForNonVirtualNetwork(
          GetColorProvider(), wifi_network.get(), network_icon::ICON_TYPE_LIST,
          false));

  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(image), default_image));

  // Test that theme changes cause network icon change.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);

  image = static_cast<views::ImageView*>(
              network_list_network_item_view()->left_view())
              ->GetImage();

  EXPECT_FALSE(gfx::test::AreImagesEqual(gfx::Image(image), default_image));
}

TEST_F(NetworkListNetworkItemViewTest, CellularIcon) {
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected);

  UpdateViewForNetwork(cellular_network);

  gfx::ImageSkia image = static_cast<views::ImageView*>(
                             network_list_network_item_view()->left_view())
                             ->GetImage();

  gfx::Image default_image =
      gfx::Image(network_icon::GetImageForNonVirtualNetwork(
          GetColorProvider(), cellular_network.get(),
          network_icon::ICON_TYPE_LIST, false));

  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(image), default_image));

  // Test that theme changes cause network icon change.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);

  image = static_cast<views::ImageView*>(
              network_list_network_item_view()->left_view())
              ->GetImage();

  EXPECT_FALSE(gfx::test::AreImagesEqual(gfx::Image(image), default_image));
}

}  // namespace ash
