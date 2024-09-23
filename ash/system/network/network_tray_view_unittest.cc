// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_tray_view.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using ::chromeos::network_config::FakeCrosNetworkConfig;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PortalState;
using network_config::CrosNetworkConfigTestHelper;

const char kWifiName[] = "wifi";

}  // namespace

class NetworkTrayViewTest : public AshTestBase {
 public:
  NetworkTrayViewTest() = default;
  ~NetworkTrayViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    cros_network_ = std::make_unique<FakeCrosNetworkConfig>();
    Shell::Get()
        ->system_tray_model()
        ->network_state_model()
        ->ConfigureRemoteForTesting(cros_network()->GetPendingRemote());
    base::RunLoop().RunUntilIdle();

    std::unique_ptr<NetworkTrayView> network_tray_view =
        std::make_unique<NetworkTrayView>(GetPrimaryShelf(),
                                          ActiveNetworkIcon::Type::kSingle);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_tray_view_ = widget_->SetContentsView(std::move(network_tray_view));
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  FakeCrosNetworkConfig* cros_network() { return cros_network_.get(); }

  NetworkTrayView* network_tray_view() { return network_tray_view_; }

  std::u16string get_tooltip() { return network_tray_view_->tooltip_; }

 private:
  std::unique_ptr<FakeCrosNetworkConfig> cros_network_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<NetworkTrayView, DanglingUntriaged> network_tray_view_;
};

TEST_F(NetworkTrayViewTest, NetworkIconTooltip) {
  auto wifi_network =
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          kWifiName, NetworkType::kWiFi, ConnectionStateType::kPortal);
  wifi_network->portal_state = PortalState::kPortal;

  auto wifi = mojo::Clone(wifi_network);
  cros_network()->AddNetworkAndDevice(std::move(wifi));

  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_PORTAL, u"wifi",
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGNIN)),
      get_tooltip());

  wifi_network->portal_state = PortalState::kOnline;

  cros_network()->UpdateNetworkProperties(std::move(wifi_network));

  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                       u"wifi"),
            get_tooltip());
}

TEST_F(NetworkTrayViewTest, AccessibleDescription) {
  auto cellular =
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "cellular", NetworkType::kCellular, ConnectionStateType::kConnected,
          50);

  auto cell = mojo::Clone(cellular);
  cros_network()->AddNetworkAndDevice(std::move(cell));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK),
            network_tray_view()->GetViewAccessibility().GetCachedDescription());

  cellular->type_state->get_cellular()->signal_strength = 150;

  cros_network()->UpdateNetworkProperties(std::move(cellular));

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG),
      network_tray_view()->GetViewAccessibility().GetCachedDescription());
}

// Regression test for http://b/284983806
TEST_F(NetworkTrayViewTest, EthernetVpnIconIsNotClipped) {
  // Set up an Ethernet network with a VPN.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "ethernet", NetworkType::kEthernet, ConnectionStateType::kConnected));
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "vpn", NetworkType::kVPN, ConnectionStateType::kConnected));

  // The view's preferred size is as least as large as the image (so it doesn't
  // clip).
  gfx::Size view_size = network_tray_view()->CalculatePreferredSize({});
  gfx::Size image_size = network_tray_view()->image_view()->GetImage().size();
  EXPECT_GE(view_size.width(), image_size.width());
  EXPECT_GE(view_size.height(), image_size.height());
}

TEST_F(NetworkTrayViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  // Initial Accessible Properties.
  network_tray_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kImage);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED_A11Y));

  // Set up an Ethernet network.
  cros_network()->AddNetworkAndDevice(
      CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
          "ethernet", NetworkType::kEthernet, ConnectionStateType::kConnected));
  data = ui::AXNodeData();
  network_tray_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                 /* network_name= */ l10n_util::GetStringUTF16(
                                     IDS_ASH_STATUS_TRAY_ETHERNET)));
}

}  // namespace ash
