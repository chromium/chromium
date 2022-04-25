// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_item_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/network/fake_network_detailed_network_view.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_info.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/number_formatting.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
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

using chromeos::network_config::CrosNetworkConfigTestHelper;

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;
using chromeos::network_config::mojom::SecurityType;

const std::string kWiFiName = "Silent WiFi";
const std::string kCellularName = "Strong WiFi";
const std::string kTetherName = "tether";
const std::string kEid = "sim_eid";
const std::string kEthernet = "ethernet";

const char kStubEthernetDeviceName[] = "stub_ethernet_device";
const char kStubEthernetDevicePath[] = "/device/stub_ethernet_device";
const char kStubWiFiDeviceName[] = "stub_wifi_device";
const char kStubWiFiDevicePath[] = "/device/stub_wifi_device";
const char kStubCellularDeviceName[] = "stub_cellular_device";
const char kStubCellularDevicePath[] = "/device/stub_cellular_device";

}  // namespace

class NetworkListNetworkItemViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

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
        kWiFiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected, 50);

    network_list_network_item_view_ =
        widget_->SetContentsView(std::move(network_list_network_item_view));
    network_list_network_item_view_->UpdateViewForNetwork(wifi_network);
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  NetworkListNetworkItemView* network_list_network_item_view() {
    return network_list_network_item_view_;
  }

  const NetworkListItemView* last_clicked_network_list_item() {
    return fake_network_detailed_network_view_
        ->last_clicked_network_list_item();
  }

  NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      NetworkType type,
      ConnectionStateType connection_state,
      int signal_strength) {
    return network_config_helper_.CreateStandaloneNetworkProperties(
        id, type, connection_state, signal_strength);
  }

  void UpdateViewForNetwork(NetworkStatePropertiesPtr& network) {
    network_list_network_item_view()->UpdateViewForNetwork(network);
  }

  void AssertA11yDescription(NetworkStatePropertiesPtr& network_properties) {
    ui::AXNodeData node_data;
    UpdateViewForNetwork(network_properties);
    network_list_network_item_view()
        ->GetViewAccessibility()
        .GetAccessibleNodeData(&node_data);
    std::u16string description =
        network_list_network_item_view()->GenerateAccessibilityDescription();
    std::string a11ydescription =
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    EXPECT_EQ(base::UTF8ToUTF16(a11ydescription), description);
  }

 private:
  void SetUpDefaultNetworkDevices() {
    network_state_helper()->ClearDevices();
    network_state_helper()->AddDevice(
        kStubCellularDevicePath, shill::kTypeCellular, kStubCellularDeviceName);
    network_state_helper()->AddDevice(
        kStubEthernetDevicePath, shill::kTypeEthernet, kStubEthernetDeviceName);
    network_state_helper()->AddDevice(kStubWiFiDevicePath, shill::kTypeWifi,
                                      kStubWiFiDeviceName);
  }

  chromeos::NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeNetworkDetailedNetworkView>
      fake_network_detailed_network_view_;
  CrosNetworkConfigTestHelper network_config_helper_;
  NetworkListNetworkItemView* network_list_network_item_view_;
};

TEST_F(NetworkListNetworkItemViewTest, HasCorrectLabel) {
  ASSERT_TRUE(network_list_network_item_view()->text_label());

  EXPECT_EQ(base::UTF8ToUTF16(kWiFiName),
            network_list_network_item_view()->text_label()->GetText());

  const std::string kNewWifiName = "New wifi";

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kNotConnected, 50);
  wifi_network->name = kNewWifiName;
  UpdateViewForNetwork(wifi_network);

  EXPECT_EQ(base::UTF8ToUTF16(kNewWifiName),
            network_list_network_item_view()->text_label()->GetText());
}

TEST_F(NetworkListNetworkItemViewTest, HasCorrectSublabel) {
  EXPECT_FALSE(network_list_network_item_view()->sub_text_label());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected, 50);

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

  // Label for pSIM networks that are connected but not yet activated.
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kWiFiName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected, 50);
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE),
            network_list_network_item_view()->sub_text_label()->GetText());

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
}

TEST_F(NetworkListNetworkItemViewTest, NotifiesListenerWhenClicked) {
  EXPECT_FALSE(last_clicked_network_list_item());
  LeftClickOn(network_list_network_item_view());
  EXPECT_EQ(last_clicked_network_list_item(), network_list_network_item_view());
}

TEST_F(NetworkListNetworkItemViewTest, HasEnterpriseIconWhenBlockedByPolicy) {
  EXPECT_FALSE(network_list_network_item_view()->right_view());

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected, 50);
  wifi_network->source = OncSource::kDevicePolicy;
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

  wifi_network->source = OncSource::kNone;
  UpdateViewForNetwork(wifi_network);
  ASSERT_FALSE(network_list_network_item_view()->right_view());
}

TEST_F(NetworkListNetworkItemViewTest, HasPoweredIcon) {
  EXPECT_FALSE(network_list_network_item_view()->right_view());
  int battery_percentage = 50;

  NetworkStatePropertiesPtr tether_network = CreateStandaloneNetworkProperties(
      kTetherName, NetworkType::kTether, ConnectionStateType::kConnected,
      battery_percentage);

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
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected, 50);
  wifi_network->connection_state = ConnectionStateType::kConnected;

  UpdateViewForNetwork(wifi_network);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN,
                                 base::UTF8ToUTF16(kWiFiName)),
      network_list_network_item_view()->GetAccessibleName());

  // Network can be connected to.
  wifi_network->connectable = true;
  wifi_network->connection_state = ConnectionStateType::kNotConnected;
  UpdateViewForNetwork(wifi_network);

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT,
                                 base::UTF8ToUTF16(kWiFiName)),
      network_list_network_item_view()->GetAccessibleName());

  // Activate cellular network A11Y label is shown when a pSIM network is
  // connected but not yet activated.
  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected, 50);
  cellular_network->connectable = true;
  cellular_network->connection_state = ConnectionStateType::kConnected;
  cellular_network->type_state->get_cellular()->activation_state =
      ActivationStateType::kNotActivated;
  UpdateViewForNetwork(cellular_network);

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE,
                base::UTF8ToUTF16(kCellularName)),
            network_list_network_item_view()->GetAccessibleName());

  // Contact carrier A11Y label is shown when a eSIM network is connected but
  // not yet activated.
  cellular_network->type_state->get_cellular()->eid = kEid;
  UpdateViewForNetwork(cellular_network);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_A11Y_UNAVAILABLE_SIM_NETWORK,
                base::UTF8ToUTF16(kCellularName)),
            network_list_network_item_view()->GetAccessibleName());
}

TEST_F(NetworkListNetworkItemViewTest, HasExpectedDescription) {
  ConnectionStateType connection_states[3] = {
      ConnectionStateType::kConnected, ConnectionStateType::kConnecting,
      ConnectionStateType::kNotConnected};
  OncSource policies[2]{OncSource::kDevicePolicy, OncSource::kNone};

  // A11Y description for Ethernet networks.
  NetworkStatePropertiesPtr ethernet_network =
      CreateStandaloneNetworkProperties(kEthernet, NetworkType::kEthernet,
                                        ConnectionStateType::kConnected, 50);
  for (const auto& connection : connection_states) {
    ethernet_network->connection_state = connection;
    AssertA11yDescription(ethernet_network);
    for (const auto& policy : policies) {
      ethernet_network->source = policy;
      AssertA11yDescription(ethernet_network);
    }
  }

  // A11Y description for WiFi networks.
  SecurityType security_types[2] = {SecurityType::kNone, SecurityType::kWepPsk};

  NetworkStatePropertiesPtr wifi_network = CreateStandaloneNetworkProperties(
      kWiFiName, NetworkType::kWiFi, ConnectionStateType::kConnected, 50);

  for (const auto& connection : connection_states) {
    wifi_network->connection_state = connection;
    AssertA11yDescription(wifi_network);
    for (const auto& security : security_types) {
      wifi_network->type_state->get_wifi()->security = security;
      AssertA11yDescription(wifi_network);
      for (const auto& policy : policies) {
        wifi_network->source = policy;
        AssertA11yDescription(wifi_network);
      }
    }
  }

  // A11Y description for Celular networks.
  ActivationStateType activation_states[2]{ActivationStateType::kNotActivated,
                                           ActivationStateType::kActivated};
  bool locked[2]{true, false};

  NetworkStatePropertiesPtr cellular_network =
      CreateStandaloneNetworkProperties(kCellularName, NetworkType::kCellular,
                                        ConnectionStateType::kConnected, 50);

  for (const auto& connection : connection_states) {
    cellular_network->connection_state = connection;
    AssertA11yDescription(cellular_network);
    for (const auto& activation_state : activation_states) {
      cellular_network->type_state->get_cellular()->activation_state =
          activation_state;
      AssertA11yDescription(cellular_network);
      for (const auto& lock : locked) {
        cellular_network->type_state->get_cellular()->sim_locked = lock;
        AssertA11yDescription(cellular_network);
        for (const auto& policy : policies) {
          cellular_network->source = policy;
          AssertA11yDescription(cellular_network);
        }
      }
    }
  }

  // A11Y description for Tether networks.
  NetworkStatePropertiesPtr tether_network = CreateStandaloneNetworkProperties(
      kTetherName, NetworkType::kTether, ConnectionStateType::kConnected, 50);
  tether_network->type_state->get_tether()->battery_percentage = 20;
  for (const auto& connection : connection_states) {
    tether_network->connection_state = connection;
    AssertA11yDescription(tether_network);
  }
}

}  // namespace ash
