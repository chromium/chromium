// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include <memory>
#include <string>

#include "ash/public/cpp/network_config_service.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_util.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkType;

const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device";
const char kCellularNetworkGuid[] = "cellular_guid";
const char16_t kCellularNetworkGuid16[] = u"cellular_guid";

}  // namespace

class ActiveNetworkIconTest : public AshTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  ActiveNetworkIconTest() = default;

  ActiveNetworkIconTest(const ActiveNetworkIconTest&) = delete;
  ActiveNetworkIconTest& operator=(const ActiveNetworkIconTest&) = delete;

  ~ActiveNetworkIconTest() override = default;

  void SetUp() override {
    if (IsJellyrollEnabled()) {
      feature_list_.InitAndEnableFeature(chromeos::features::kJellyroll);
    } else {
      feature_list_.InitAndDisableFeature(chromeos::features::kJellyroll);
    }

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

  gfx::ImageSkia ImageForNetwork(NetworkType type,
                                 ConnectionStateType connection_state,
                                 int signal_strength = 100) {
    std::string id = base::StringPrintf("reference_%d", reference_count_++);
    chromeos::network_config::mojom::NetworkStatePropertiesPtr
        reference_properties =
            network_config_helper_.CreateStandaloneNetworkProperties(
                id, type, connection_state, signal_strength);
    return network_icon::GetImageForNonVirtualNetwork(
        GetColorProvider(), reference_properties.get(), icon_type_,
        false /* show_vpn_badge */);
  }

  bool IsJellyrollEnabled() const { return GetParam(); }

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

  NetworkStateTestHelper& network_state_helper() {
    return network_config_helper_.network_state_helper();
  }
  NetworkStateHandler* network_state_handler() {
    return network_state_helper().network_state_handler();
  }
  ActiveNetworkIcon* active_network_icon() {
    return active_network_icon_.get();
  }
  TrayNetworkStateModel* network_state_model() {
    return network_state_model_.get();
  }

  const std::string& eth_path() const { return eth_path_; }
  const std::string& wifi_path() const { return wifi_path_; }
  const std::string& cellular_path() const { return cellular_path_; }

  network_icon::IconType icon_type() { return icon_type_; }

  const ui::ColorProvider* GetColorProvider() {
    // TODO(b/279177422): Replace with a stable ColorProvider
    return ColorUtil::GetColorProviderSourceForWindow(
               Shell::GetPrimaryRootWindow())
        ->GetColorProvider();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  network_config::CrosNetworkConfigTestHelper network_config_helper_;
  std::unique_ptr<TrayNetworkStateModel> network_state_model_;
  std::unique_ptr<ActiveNetworkIcon> active_network_icon_;

  std::string eth_path_;
  std::string wifi_path_;
  std::string cellular_path_;

  network_icon::IconType icon_type_ = network_icon::ICON_TYPE_TRAY_REGULAR;
  // Counter to provide unique ids for reference networks.
  int reference_count_ = 0;
};

INSTANTIATE_TEST_SUITE_P(Jellyroll, ActiveNetworkIconTest, testing::Bool());

TEST_P(ActiveNetworkIconTest, GetConnectionStatusStrings) {
  // TODO(902409): Test multi icon and improve coverage.
  SetupCellular(shill::kStateOnline);
  std::u16string name, desc, tooltip;
  active_network_icon()->GetConnectionStatusStrings(
      ActiveNetworkIcon::Type::kSingle, &name, &desc, &tooltip);
  // Note: The guid is used for the name in ConfigureService.
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                       kCellularNetworkGuid16),
            name);
  std::u16string connected_string = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED, kCellularNetworkGuid16);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED_TOOLTIP, connected_string,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG)),
      tooltip);
}

TEST_P(ActiveNetworkIconTest, GetSingleImage) {
  // Cellular only = Cellular icon
  SetupCellular(shill::kStateOnline);
  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      GetColorProvider(), ActiveNetworkIcon::Type::kSingle, icon_type(),
      &animating);
  EXPECT_TRUE(AreImagesEqual(
      image,
      ImageForNetwork(NetworkType::kCellular, ConnectionStateType::kOnline)));
  EXPECT_FALSE(animating);

  // Cellular + WiFi connected = WiFi connected icon
  SetupWiFi(shill::kStateOnline);
  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
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
  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image, ImageForNetwork(NetworkType::kWiFi,
                             ConnectionStateType::kConnecting, 50)));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connecting + Ethernet = WiFi connecting icon
  SetupEthernet();
  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image, ImageForNetwork(NetworkType::kWiFi,
                             ConnectionStateType::kConnecting, 50)));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connected + Ethernet = Ethernet connected icon
  SetupWiFi(shill::kStateOnline);
  network_state_handler()->SetNetworkConnectRequested(wifi_path(), false);
  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(
      image,
      ImageForNetwork(NetworkType::kEthernet, ConnectionStateType::kOnline)));
  EXPECT_FALSE(animating);
}

TEST_P(ActiveNetworkIconTest, CellularUninitialized) {
  SetCellularUninitialized(false /* scanning */);

  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      GetColorProvider(), ActiveNetworkIcon::Type::kSingle, icon_type(),
      &animating);
  EXPECT_TRUE(
      AreImagesEqual(image, ImageForNetwork(NetworkType::kCellular,
                                            ConnectionStateType::kConnecting)));
  EXPECT_TRUE(animating);
}

TEST_P(ActiveNetworkIconTest, CellularScanning) {
  SetCellularUninitialized(true /* scanning */);

  ASSERT_TRUE(network_state_handler()->GetScanningByType(
      NetworkTypePattern::Cellular()));

  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      GetColorProvider(), ActiveNetworkIcon::Type::kSingle, icon_type(),
      &animating);
  EXPECT_TRUE(
      AreImagesEqual(image, ImageForNetwork(NetworkType::kCellular,
                                            ConnectionStateType::kConnecting)));
  EXPECT_TRUE(animating);

  // Set scanning property to false, expect no network connections icon.
  network_state_helper().device_test()->SetDeviceProperty(
      kShillManagerClientStubCellularDevice, shill::kScanningProperty,
      base::Value(false), true /* notify_changed */);
  base::RunLoop().RunUntilIdle();

  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(image, network_icon::GetImageForWiFiNoConnections(
                                        GetColorProvider(), icon_type())));
  EXPECT_FALSE(animating);
}

TEST_P(ActiveNetworkIconTest, CellularDisable) {
  SetupCellular(shill::kStateOnline);
  bool animating;
  gfx::ImageSkia image = active_network_icon()->GetImage(
      GetColorProvider(), ActiveNetworkIcon::Type::kSingle, icon_type(),
      &animating);
  EXPECT_TRUE(AreImagesEqual(
      image,
      ImageForNetwork(NetworkType::kCellular, ConnectionStateType::kOnline)));
  EXPECT_FALSE(animating);

  // The cellular device's scanning property may be true while it's being
  // disabled, mock this.
  network_state_helper().device_test()->SetDeviceProperty(
      kShillManagerClientStubCellularDevice, shill::kScanningProperty,
      base::Value(true), true /* notify_changed */);

  // Disable the device.
  network_state_model()->SetNetworkTypeEnabledState(NetworkType::kCellular,
                                                    false);
  // Disabling the device doesn't actually remove the services in the fakes,
  // remove them explicitly.
  network_state_helper().ClearServices();
  base::RunLoop().RunUntilIdle();

  image = active_network_icon()->GetImage(GetColorProvider(),
                                          ActiveNetworkIcon::Type::kSingle,
                                          icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(image, network_icon::GetImageForWiFiNoConnections(
                                        GetColorProvider(), icon_type())));
  EXPECT_FALSE(animating);
}

// TODO(stevenjb): Test GetDualImagePrimary, GetDualImageCellular.

}  // namespace ash
