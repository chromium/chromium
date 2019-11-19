// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include <memory>
#include <string>

#include "ash/public/cpp/network_config_service.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device";
const char kCellularNetworkGuid[] = "cellular_guid";

}  // namespace

class ActiveNetworkIconTest : public AshTestBase {
 public:
  ActiveNetworkIconTest() = default;
  ~ActiveNetworkIconTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    network_state_model_ = std::make_unique<TrayNetworkStateModel>();
    active_network_icon_ =
        std::make_unique<ActiveNetworkIcon>(network_state_model_.get());
  }

  void TearDown() override {
    active_network_icon_.reset();
    AshTestBase::TearDown();
  }

  void SetupEthernet() {
    if (eth_path_.empty()) {
      network_state_helper().device_test()->AddDevice(
          "/device/stub_eth_device", shill::kTypeEthernet, "stub_eth_device");
      eth_path_ = ConfigureService(
          R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    }
    base::RunLoop().RunUntilIdle();
  }

  void SetupWiFi(const char* state) {
    if (wifi_path_.empty()) {
      // WiFi device already exists.
      wifi_path_ = ConfigureService(
          R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle"})");
    }
    SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
    SetServiceProperty(wifi_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    base::RunLoop().RunUntilIdle();
  }

  void SetupCellular(const char* state) {
    if (cellular_path_.empty()) {
      network_state_helper().manager_test()->AddTechnology(shill::kTypeCellular,
                                                           true /* enabled */);
      network_state_helper().AddDevice(kShillManagerClientStubCellularDevice,
                                       shill::kTypeCellular,
                                       "stub_cellular_device");
      cellular_path_ = ConfigureService(base::StringPrintf(
          R"({"GUID": "%s", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})",
          kCellularNetworkGuid));
      SetServiceProperty(cellular_path_, shill::kDeviceProperty,
                         base::Value(kShillManagerClientStubCellularDevice));
    }
    SetServiceProperty(cellular_path_, shill::kStateProperty,
                       base::Value(state));
    SetServiceProperty(cellular_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularUninitialized(bool scanning) {
    const bool enabled = scanning;
    network_state_helper().manager_test()->RemoveTechnology(
        shill::kTypeCellular);
    network_state_helper().manager_test()->AddTechnology(shill::kTypeCellular,
                                                         enabled);
    network_state_helper().device_test()->AddDevice(
        kShillManagerClientStubCellularDevice, shill::kTypeCellular,
        "stub_cellular_device");
    if (scanning) {
      network_state_helper().device_test()->SetDeviceProperty(
          kShillManagerClientStubCellularDevice, shill::kScanningProperty,
          base::Value(true), true /* notify_changed */);
    } else {
      network_state_helper().manager_test()->SetTechnologyInitializing(
          shill::kTypeCellular, true);
    }
    base::RunLoop().RunUntilIdle();
  }

  gfx::ImageSkia ImageForNetwork(
      chromeos::network_config::mojom::NetworkType type,
      chromeos::network_config::mojom::ConnectionStateType connection_state,
      int signal_strength = 100) {
    std::string id = base::StringPrintf("reference_%d", reference_count_++);
    chromeos::network_config::mojom::NetworkStatePropertiesPtr
        reference_properties =
            network_state_helper().CreateStandaloneNetworkProperties(
                id, type, connection_state, signal_strength);
    return network_icon::GetImageForNonVirtualNetwork(
        reference_properties.get(), icon_type_, false /* show_vpn_badge */);
  }

  bool AreImagesEqual(const gfx::ImageSkia& image,
                      const gfx::ImageSkia& reference) {
    return gfx::test::AreBitmapsEqual(*image.bitmap(), *reference.bitmap());
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper().ConfigureService(shill_json_string);
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    network_state_helper().SetServiceProperty(service_path, key, value);
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return network_config_helper_.network_state_helper();
  }
  chromeos::NetworkStateHandler* network_state_handler() {
    return network_state_helper().network_state_handler();
  }
  ActiveNetworkIcon* active_network_icon() {
    return active_network_icon_.get();
  }

  const std::string& eth_path() const { return eth_path_; }
  const std::string& wifi_path() const { return wifi_path_; }
  const std::string& cellular_path() const { return cellular_path_; }

  network_icon::IconType icon_type() { return icon_type_; }

 private:
  chromeos::network_config::CrosNetworkConfigTestHelper network_config_helper_;
  std::unique_ptr<TrayNetworkStateModel> network_state_model_;
  std::unique_ptr<ActiveNetworkIcon> active_network_icon_;

  std::string eth_path_;
  std::string wifi_path_;
  std::string cellular_path_;

  network_icon::IconType icon_type_ = network_icon::ICON_TYPE_TRAY_REGULAR;
  // Counter to provide unique ids for reference networks.
  int reference_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ActiveNetworkIconTest);
};

TEST_F(ActiveNetworkIconTest, GetConnectionStatusStrings) {
  // TODO(902409): Test multi icon and improve coverage.
  SetupCellular(shill::kStateOnline);
  base::string16 name, desc, tooltip;
  active_network_icon()->GetConnectionStatusStrings(
      ActiveNetworkIcon::Type::kSingle, &name, &desc, &tooltip);
  // Note: The guid is used for the name in ConfigureService.
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                       base::UTF8ToUTF16(kCellularNetworkGuid)),
            name);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED_TOOLTIP,
          base::UTF8ToUTF16(kCellularNetworkGuid),
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG)),
      tooltip);
}

TEST_F(ActiveNetworkIconTest, GetSingleImage) {
  // Cellular only = Cellular icon
  SetupCellular(shill::kStateOnline);
  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      ActiveNetworkIcon::Type::kSingle, icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image,
      ImageForNetwork(NetworkType::kCellular, ConnectionStateType::kOnline)));
  EXPECT_FALSE(animating);

  // Cellular + WiFi connected = WiFi connected icon
  SetupWiFi(shill::kStateOnline);
  image = active_network_icon()->GetImage(ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image,
      ImageForNetwork(NetworkType::kWiFi, ConnectionStateType::kOnline)));
  EXPECT_FALSE(animating);

  // Cellular + WiFi connecting = WiFi connecting icon
  SetupWiFi(shill::kStateAssociation);
  network_state_handler()->SetNetworkConnectRequested(wifi_path(), true);
  SetServiceProperty(wifi_path(), shill::kSignalStrengthProperty,
                     base::Value(50));
  base::RunLoop().RunUntilIdle();
  image = active_network_icon()->GetImage(ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image, ImageForNetwork(NetworkType::kWiFi,
                             ConnectionStateType::kConnecting, 50)));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connecting + Ethernet = WiFi connecting icon
  SetupEthernet();
  image = active_network_icon()->GetImage(ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image, ImageForNetwork(NetworkType::kWiFi,
                             ConnectionStateType::kConnecting, 50)));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connected + Ethernet = No icon
  SetupWiFi(shill::kStateOnline);
  network_state_handler()->SetNetworkConnectRequested(wifi_path(), false);
  image = active_network_icon()->GetImage(ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(image.isNull());
  EXPECT_FALSE(animating);
}

TEST_F(ActiveNetworkIconTest, CellularUninitialized) {
  SetCellularUninitialized(false /* scanning */);

  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      ActiveNetworkIcon::Type::kSingle, icon_type(), &animating);
  EXPECT_TRUE(
      AreImagesEqual(image, ImageForNetwork(NetworkType::kCellular,
                                            ConnectionStateType::kConnecting)));
  EXPECT_TRUE(animating);
}

TEST_F(ActiveNetworkIconTest, CellularScanning) {
  SetCellularUninitialized(true /* scanning */);

  ASSERT_TRUE(network_state_handler()->GetScanningByType(
      chromeos::NetworkTypePattern::Cellular()));

  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      ActiveNetworkIcon::Type::kSingle, icon_type(), &animating);
  EXPECT_TRUE(
      AreImagesEqual(image, ImageForNetwork(NetworkType::kCellular,
                                            ConnectionStateType::kConnecting)));
  EXPECT_TRUE(animating);
}

// TODO(stevenjb): Test GetDualImagePrimary, GetDualImageCellular.

}  // namespace ash
