// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace {

const char* kStubCellularDevice = "/device/stub_cellular_device";

// The GUIDs used for the different network types.
const char* kNetworkGuidCellular = "cellular_guid";
const char* kNetworkGuidTether = "tether_guid";
const char* kNetworkGuidWifi = "wifi_guid";
constexpr char kNetworkGuidEthernet[] = "ethernet_guid";
constexpr char kNetworkGuidTetherWiFi[] = "tether_wifi_guid";

// The templates used to configure services for different network types.
constexpr char kServicePatternCellular[] = R"({
    "GUID": "%s", "Type": "cellular", "State": "online", "Strength": 100,
    "Device": "%s", "Cellular.NetworkTechnology": "LTE"})";

constexpr char kServicePatternEthernet[] = R"({
    "GUID": "%s", "Type": "ethernet", "State": "online"})";

constexpr char kServicePatternWiFi[] = R"({
    "GUID": "%s", "Type": "wifi", "State": "online", "Strength": 100})";

constexpr char kServicePatternTetherWiFi[] = R"({
    "GUID": "%s", "Type": "wifi", "State": "idle"})";

// Compute the next signal strength that is larger than |signal_strength| that
// would be placed into a different bucket (e.g. NONE to WEAK).
int ComputeNextSignalStrength(int signal_strength) {
  EXPECT_LE(0, signal_strength);

  const network_icon::SignalStrength previous =
      network_icon::GetSignalStrength(signal_strength);

  do {
    // Don't bother incrementing past |100|.
    if (signal_strength >= 90) {
      return 100;
    }

    // Small signal strength changes don't cause the network to be "changed".
    signal_strength += 10;
  } while (network_icon::GetSignalStrength(signal_strength) == previous);

  return signal_strength;
}

}  // namespace

class NetworkFeaturePodControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitAndEnableFeature(features::kQuickSettingsNetworkRevamp);

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    network_feature_pod_controller_ =
        std::make_unique<NetworkFeaturePodController>(tray_controller());
    feature_pod_button_.reset(network_feature_pod_controller_->CreateButton());

    // Add the non-default cellular and ethernet devices to Shill.
    network_state_helper()->manager_test()->AddTechnology(shill::kTypeCellular,
                                                          /*enabled=*/true);
    network_state_helper()->AddDevice(kStubCellularDevice, shill::kTypeCellular,
                                      "stub_cellular_device");
    network_state_helper()->AddDevice("/device/stub_eth_device",
                                      shill::kTypeEthernet, "stub_eth_device");

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_feature_pod_controller_.reset();
    feature_pod_button_.reset();

    AshTestBase::TearDown();
  }

 protected:
  // Disabling a network technology does not remove corresponding networks from
  // the testing fakes. This function is used to clear the existing networks.
  void ClearNetworks() {
    network_state_handler()->RemoveTetherNetworkState(kNetworkGuidTether);
    network_state_helper()->ClearServices();
    base::RunLoop().RunUntilIdle();
  }

  void LockScreen() {
    GetSessionControllerClient()->LockScreen();

    // Perform an action to cause the button to be updated since we do not
    // actually observe session state changes.
    PressLabel();
  }

  void PressIcon() {
    network_feature_pod_controller_->OnIconPressed();
    base::RunLoop().RunUntilIdle();
  }

  void PressLabel() {
    network_feature_pod_controller_->OnLabelPressed();
    base::RunLoop().RunUntilIdle();
  }

  void SetupCellular() {
    ASSERT_TRUE(cellular_path_.empty());
    cellular_path_ = ConfigureService(base::StringPrintf(
        kServicePatternCellular, kNetworkGuidCellular, kStubCellularDevice));
    base::RunLoop().RunUntilIdle();
  }

  void SetupEthernet() {
    ASSERT_TRUE(ethernet_path_.empty());
    ethernet_path_ = ConfigureService(
        base::StringPrintf(kServicePatternEthernet, kNetworkGuidEthernet));
    base::RunLoop().RunUntilIdle();
  }

  void SetupTether() {
    ASSERT_TRUE(tether_path_.empty());
    ASSERT_TRUE(tether_wifi_path_.empty());

    tether_wifi_path_ = ConfigureService(
        base::StringPrintf(kServicePatternTetherWiFi, kNetworkGuidTetherWiFi));
    base::RunLoop().RunUntilIdle();

    network_state_handler()->AddTetherNetworkState(
        kNetworkGuidTether, /*name=*/kNetworkGuidTether,
        /*carrier=*/"carrier_stub",
        /*battery_percentage=*/0, /*signal_strength=*/0,
        /*has_connected_to_host=*/false);

    network_state_handler()->SetTetherNetworkStateConnecting(
        /*guid=*/kNetworkGuidTether);

    ASSERT_TRUE(
        network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
            /*tether_network_guid=*/kNetworkGuidTether,
            /*wifi_network_guid=*/kNetworkGuidTetherWiFi));

    network_state_handler()->SetTetherNetworkStateConnected(
        /*guid=*/kNetworkGuidTether);

    SetServiceProperty(/*service_path=*/tether_wifi_path_,
                       /*key=*/shill::kStateProperty,
                       /*value=*/base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();

    network_state_handler()->SetTetherNetworkStateConnected(
        /*guid=*/kNetworkGuidTether);
  }

  void SetupWiFi() {
    ASSERT_TRUE(wifi_path_.empty());
    wifi_path_ = ConfigureService(
        base::StringPrintf(kServicePatternWiFi, kNetworkGuidWifi));
    base::RunLoop().RunUntilIdle();
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    network_state_helper()->SetServiceProperty(service_path, key, value);
    base::RunLoop().RunUntilIdle();
  }

  void CheckNetworkDetailedViewFocused() {
    EXPECT_TRUE(tray_view()->detailed_view());
    const views::View::Views& children =
        tray_view()->detailed_view()->children();
    EXPECT_EQ(1u, children.size());
    EXPECT_STREQ("NetworkDetailedNetworkViewImpl",
                 children.at(0)->GetClassName());
  }

  void CheckSignalStrengthSubLabel(
      base::RepeatingCallback<void(int)> set_signal_strength) {
    int signal_strength = 0;

    set_signal_strength.Run(signal_strength);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED),
        feature_pod_label_button()->GetSubLabelText());

    signal_strength = ComputeNextSignalStrength(signal_strength);
    set_signal_strength.Run(signal_strength);
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK_SUBLABEL),
              feature_pod_label_button()->GetSubLabelText());

    signal_strength = ComputeNextSignalStrength(signal_strength);
    set_signal_strength.Run(signal_strength);
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM_SUBLABEL),
              feature_pod_label_button()->GetSubLabelText());

    signal_strength = ComputeNextSignalStrength(signal_strength);
    set_signal_strength.Run(signal_strength);
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG_SUBLABEL),
              feature_pod_label_button()->GetSubLabelText());
  }

  NetworkStateTestHelper* network_state_helper() {
    return &network_config_helper_.network_state_helper();
  }

  NetworkStateHandler* network_state_handler() {
    return network_state_helper()->network_state_handler();
  }

  FeaturePodButton* feature_pod_button() { return feature_pod_button_.get(); }

  FeaturePodIconButton* feature_pod_icon_button() {
    return feature_pod_button_->icon_button_;
  }

  FeaturePodLabelButton* feature_pod_label_button() {
    return feature_pod_button_->label_button_;
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  UnifiedSystemTrayView* tray_view() {
    return GetPrimaryUnifiedSystemTray()->bubble()->unified_view();
  }

  const std::string& cellular_path() const { return cellular_path_; }
  const std::string& ethernet_path() const { return ethernet_path_; }
  const std::string& tether_wifi_path() const { return tether_wifi_path_; }
  const std::string& wifi_path() const { return wifi_path_; }

 private:
  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper()->ConfigureService(shill_json_string);
  }

  chromeos::network_config::CrosNetworkConfigTestHelper network_config_helper_;
  base::test::ScopedFeatureList feature_list_;
  std::string cellular_path_;
  std::string ethernet_path_;
  std::string wifi_path_;
  std::string tether_path_;
  std::string tether_wifi_path_;
  std::unique_ptr<FeaturePodButton> feature_pod_button_;
  std::unique_ptr<NetworkFeaturePodController> network_feature_pod_controller_;
};

TEST_F(NetworkFeaturePodControllerTest, PressingLabelShowsNetworkDetailedView) {
  ASSERT_TRUE(tray_view()->detailed_view()->children().empty());
  PressLabel();
  CheckNetworkDetailedViewFocused();
}

// This test validates that pressing the icon will show the detailed Network
// view when the Quick Settings is collapsed and the technology of the active
// network cannot be toggled, e.g. ethernet.
TEST_F(NetworkFeaturePodControllerTest,
       PressingIconConditionallyShowsDetailedView) {
  EXPECT_TRUE(tray_view()->detailed_view()->children().empty());

  tray_controller()->CollapseWithoutAnimating();
  base::RunLoop().RunUntilIdle();

  // Disable WiFi.
  PressIcon();
  EXPECT_TRUE(tray_view()->detailed_view()->children().empty());

  // Disable Cellular.
  PressIcon();
  EXPECT_TRUE(tray_view()->detailed_view()->children().empty());

  SetupEthernet();

  PressIcon();
  CheckNetworkDetailedViewFocused();
}

TEST_F(NetworkFeaturePodControllerTest,
       EnablingNetworkTechnologyShowsNetworkDetailedView) {
  // Disable WiFi.
  PressIcon();

  // We do not navigate to the detailed view when a network technology becomes
  // disabled.
  EXPECT_TRUE(tray_view()->detailed_view()->children().empty());

  PressIcon();
  CheckNetworkDetailedViewFocused();
}

TEST_F(NetworkFeaturePodControllerTest,
       HasCorrectButtonStateWhenNetworkStateChanges) {
  EXPECT_TRUE(feature_pod_button()->GetEnabled());
  EXPECT_TRUE(feature_pod_button()->GetVisible());

  // When WiFi is available the button will always be toggled, even when there
  // are no connected networks.
  EXPECT_TRUE(feature_pod_button()->IsToggled());

  SetupWiFi();
  EXPECT_TRUE(feature_pod_button()->IsToggled());
  ClearNetworks();

  SetupTether();
  EXPECT_TRUE(feature_pod_button()->IsToggled());
  ClearNetworks();

  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();

  // Any connected network should cause the button to be toggled.
  EXPECT_FALSE(feature_pod_button()->IsToggled());
  SetupCellular();
  EXPECT_TRUE(feature_pod_button()->IsToggled());
  ClearNetworks();

  EXPECT_FALSE(feature_pod_button()->IsToggled());
  SetupEthernet();
  EXPECT_TRUE(feature_pod_button()->IsToggled());
  ClearNetworks();
}

TEST_F(NetworkFeaturePodControllerTest, CannotBeModifiedWhenScreenIsLocked) {
  EXPECT_TRUE(feature_pod_button()->GetEnabled());
  LockScreen();
  EXPECT_FALSE(feature_pod_button()->GetEnabled());
}

TEST_F(NetworkFeaturePodControllerTest,
       PressingIconOrLabelIsHandledCorrectly_Cellular) {
  ASSERT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));

  SetupCellular();

  // Make sure that Cellular cannot be toggled on when the icon or label is
  // pressed, only toggled off.
  for (int i = 0; i < 2; ++i) {
    PressIcon();
    EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
        NetworkTypePattern::Cellular()));
    ClearNetworks();
  }
  PressLabel();
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Cellular()));
}

TEST_F(NetworkFeaturePodControllerTest,
       PressingIconOrLabelIsHandledCorrectly_Ethernet) {
  ASSERT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Ethernet()));

  SetupEthernet();

  // Make sure that Ethernet cannot be toggled when the icon is pressed.
  PressIcon();
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Ethernet()));

  // Make sure that Ethernet cannot be toggled when the label is pressed.
  PressLabel();
  EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Ethernet()));
}

TEST_F(NetworkFeaturePodControllerTest,
       PressingIconOrLabelIsHandledCorrectly_Tether) {
  ASSERT_TRUE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Tether()));

  SetupTether();

  // Make sure that Tether can only be toggled off when the icon is pressed.
  for (int i = 0; i < 2; ++i) {
    PressIcon();
    EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
        NetworkTypePattern::Tether()));
    ClearNetworks();
  }

  // Make sure that Tether cannot be toggled on when the label is pressed.
  PressLabel();
  EXPECT_FALSE(network_state_handler()->IsTechnologyEnabled(
      NetworkTypePattern::Tether()));
}

TEST_F(NetworkFeaturePodControllerTest,
       PressingIconOrLabelIsHandledCorrectly_WiFi) {
  ASSERT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Make sure that WiFi can be toggled on and off when the icon is pressed.
  PressIcon();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  PressIcon();
  EXPECT_TRUE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));
  PressIcon();
  EXPECT_FALSE(
      network_state_handler()->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  // Make sure that WiFi is toggled on, and only on, when the label is pressed.
  for (int i = 0; i < 2; ++i) {
    PressLabel();
    EXPECT_TRUE(network_state_handler()->IsTechnologyEnabled(
        NetworkTypePattern::WiFi()));
  }
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectLabel) {
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_LABEL),
      feature_pod_label_button()->GetLabelText());

  // For Ethernet we use a generic label.
  SetupEthernet();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET),
            feature_pod_label_button()->GetLabelText());
  ClearNetworks();

  // For all other networks we use the name.
  SetupCellular();
  EXPECT_EQ(base::ASCIIToUTF16(kNetworkGuidCellular),
            feature_pod_label_button()->GetLabelText());
  ClearNetworks();

  SetupTether();
  EXPECT_EQ(base::ASCIIToUTF16(kNetworkGuidTether),
            feature_pod_label_button()->GetLabelText());
  ClearNetworks();

  SetupWiFi();
  EXPECT_EQ(base::ASCIIToUTF16(kNetworkGuidWifi),
            feature_pod_label_button()->GetLabelText());
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectSubLabel_Cellular) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetupCellular();

  // A network exists but we are not connected to it.
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Mark the network as currently connecting.
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Cellular networks in the process of activating have a specific sub-label,
  // even if the network is in the connecting state.
  SetServiceProperty(cellular_path(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivating));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Cellular networks that are activated and online have a specific sub-label
  // depending on the network technology (e.g. LTE).
  SetServiceProperty(cellular_path(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivated));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  const base::flat_map<const char*, int> network_technology_to_text_id{{
      {shill::kNetworkTechnology1Xrtt,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_ONE_X},
      {shill::kNetworkTechnologyGsm,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GSM},
      {shill::kNetworkTechnologyGprs,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GPRS},
      {shill::kNetworkTechnologyEdge,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_EDGE},
      {shill::kNetworkTechnologyEvdo,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_THREE_G},
      {shill::kNetworkTechnologyUmts,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_THREE_G},
      {shill::kNetworkTechnologyHspa,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA},
      {shill::kNetworkTechnologyHspaPlus,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA_PLUS},
      {shill::kNetworkTechnologyLte,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE},
      {shill::kNetworkTechnologyLteAdvanced,
       IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE_PLUS},
  }};

  for (const auto& it : network_technology_to_text_id) {
    SetServiceProperty(cellular_path(), shill::kNetworkTechnologyProperty,
                       base::Value(it.first));
    EXPECT_EQ(l10n_util::GetStringUTF16(it.second),
              feature_pod_label_button()->GetSubLabelText());
  }
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectSubLabel_Ethernet) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetupEthernet();

  // A network exists but we are not connected to it.
  SetServiceProperty(ethernet_path(), shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Ethernet is not eligible to be the default network until it is connected.
  // Mark the network as currently connecting and ensure this is true.
  SetServiceProperty(ethernet_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));
  network_state_handler()->SetNetworkConnectRequested(ethernet_path(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetServiceProperty(ethernet_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED),
      feature_pod_label_button()->GetSubLabelText());
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectSubLabel_Tether) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetupTether();

  // A network exists but we are not connected to it.
  network_state_handler()->SetTetherNetworkStateDisconnected(
      /*guid=*/kNetworkGuidTether);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Mark the network as currently connecting.
  network_state_handler()->SetTetherNetworkStateConnecting(
      /*guid=*/kNetworkGuidTether);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Mark the network as connected.
  network_state_handler()->SetTetherNetworkStateConnected(
      /*guid=*/kNetworkGuidTether);
  base::RunLoop().RunUntilIdle();

  CheckSignalStrengthSubLabel(base::BindRepeating(
      [](NetworkStateHandler* handler, int signal_strength) {
        EXPECT_TRUE(handler->UpdateTetherNetworkProperties(
            kNetworkGuidTether, /*carrier=*/"carrier_stub",
            /*battery_percentage=*/0, signal_strength));
        base::RunLoop().RunUntilIdle();
      },
      network_state_handler()));
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectSubLabel_WiFi) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetupWiFi();

  // A network exists but we are not connected to it.
  SetServiceProperty(wifi_path(), shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  // Mark the network as currently connecting.
  SetServiceProperty(wifi_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));
  network_state_handler()->SetNetworkConnectRequested(wifi_path(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL),
            feature_pod_label_button()->GetSubLabelText());

  SetServiceProperty(wifi_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  CheckSignalStrengthSubLabel(base::BindRepeating(
      [](NetworkStateTestHelper* helper, const std::string& service_path,
         int signal_strength) {
        helper->SetServiceProperty(service_path, shill::kSignalStrengthProperty,
                                   base::Value(signal_strength));
      },
      network_state_helper(), wifi_path()));
}

TEST_F(NetworkFeaturePodControllerTest, HasCorrectTooltips) {
  std::u16string tooltip;
  ActiveNetworkIcon* active_network_icon =
      Shell::Get()->system_tray_model()->active_network_icon();

  SetupEthernet();

  active_network_icon->GetConnectionStatusStrings(
      ActiveNetworkIcon::Type::kSingle,
      /*a11y_name=*/nullptr,
      /*a11y_desc=*/nullptr, &tooltip);

  // When the network type cannot actually be toggled the tooltip should be the
  // same for the icon as it is for the label.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, tooltip),
            feature_pod_icon_button()->GetTooltipText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, tooltip),
            feature_pod_label_button()->GetTooltipText());

  ClearNetworks();

  active_network_icon->GetConnectionStatusStrings(
      ActiveNetworkIcon::Type::kSingle,
      /*a11y_name=*/nullptr,
      /*a11y_desc=*/nullptr, &tooltip);

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, tooltip),
            feature_pod_icon_button()->GetTooltipText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, tooltip),
            feature_pod_label_button()->GetTooltipText());

  PressIcon();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, tooltip),
            feature_pod_icon_button()->GetTooltipText());

  // Pressing the label when the network type is disabled but can be enabled
  // will enable that network type.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, tooltip),
            feature_pod_label_button()->GetTooltipText());

  LockScreen();

  active_network_icon->GetConnectionStatusStrings(
      ActiveNetworkIcon::Type::kSingle,
      /*a11y_name=*/nullptr,
      /*a11y_desc=*/nullptr, &tooltip);

  EXPECT_EQ(tooltip, feature_pod_icon_button()->GetTooltipText());
  EXPECT_EQ(tooltip, feature_pod_label_button()->GetTooltipText());
}

// This test does not check whether the icons are correct, and is only intended
// to cover whether the icons supplied by the ActiveNetworkIcon class are used.
TEST_F(NetworkFeaturePodControllerTest, HasCorrectIcons) {
  ActiveNetworkIcon* active_network_icon =
      Shell::Get()->system_tray_model()->active_network_icon();

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(active_network_icon->GetImage(
          ActiveNetworkIcon::Type::kSingle,
          network_icon::ICON_TYPE_FEATURE_POD_TOGGLED, /*animating=*/nullptr)),
      gfx::Image(
          feature_pod_icon_button()->GetImage(views::Button::STATE_NORMAL))));

  feature_pod_button()->SetEnabled(false);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(active_network_icon->GetImage(
          ActiveNetworkIcon::Type::kSingle,
          network_icon::ICON_TYPE_FEATURE_POD_DISABLED, /*animating=*/nullptr)),
      gfx::Image(
          feature_pod_icon_button()->GetImage(views::Button::STATE_DISABLED))));

  // Disable WiFi which will update the feature pod button state to be enabled
  // but not toggled.
  network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/false,
      network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(active_network_icon->GetImage(
          ActiveNetworkIcon::Type::kSingle, network_icon::ICON_TYPE_FEATURE_POD,
          /*animating=*/nullptr)),
      gfx::Image(
          feature_pod_icon_button()->GetImage(views::Button::STATE_NORMAL))));
}

TEST_F(NetworkFeaturePodControllerTest, UMATracking) {
  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Disable WiFi.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kNetwork,
      /*expected_count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Go to the detailed page.
  PressLabel();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                      QsFeatureCatalogName::kNetwork,
                                      /*expected_count=*/1);
}

}  // namespace ash
