// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/hotspot_notifier.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";
const char kHotspotConfigSSID[] = "hotspot_SSID";
const char kHotspotConfigPassphrase[] = "hotspot_passphrase";

hotspot_config::mojom::HotspotConfigPtr GenerateTestConfig() {
  auto mojom_config = hotspot_config::mojom::HotspotConfig::New();
  mojom_config->auto_disable = false;
  mojom_config->band = hotspot_config::mojom::WiFiBand::kAutoChoose;
  mojom_config->security = hotspot_config::mojom::WiFiSecurityMode::kWpa2;
  mojom_config->ssid = kHotspotConfigSSID;
  mojom_config->passphrase = kHotspotConfigPassphrase;
  return mojom_config;
}

}  // namespace

class HotspotNotifierTest : public NoSessionAshTestBase {
 public:
  HotspotNotifierTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kHotspot);
  }
  HotspotNotifierTest(const HotspotNotifierTest&) = delete;
  HotspotNotifierTest& operator=(const HotspotNotifierTest&) = delete;
  ~HotspotNotifierTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();

    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    cros_hotspot_config_test_helper_ =
        std::make_unique<hotspot_config::CrosHotspotConfigTestHelper>(
            /*use_fake_implementation=*/false);

    NoSessionAshTestBase::SetUp();
    LogIn();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    cros_hotspot_config_test_helper_.reset();
    cros_network_config_test_helper_.reset();
    network_handler_test_helper_.reset();
  }

  void SetValidHotspotCapabilities() {
    base::Value::Dict capabilities_dict;
    base::Value::List upstream_list;
    upstream_list.Append(shill::kTypeCellular);
    capabilities_dict.Set(shill::kTetheringCapUpstreamProperty,
                          std::move(upstream_list));
    // Add WiFi to the downstream technology list in Shill
    base::Value::List downstream_list;
    downstream_list.Append(shill::kTypeWifi);
    capabilities_dict.Set(shill::kTetheringCapDownstreamProperty,
                          std::move(downstream_list));
    // Add allowed WiFi security mode in Shill
    base::Value::List security_list;
    security_list.Append(shill::kSecurityWpa2);
    security_list.Append(shill::kSecurityWpa3);
    capabilities_dict.Set(shill::kTetheringCapSecurityProperty,
                          std::move(security_list));
    network_handler_test_helper_->manager_test()->SetManagerProperty(
        shill::kTetheringCapabilitiesProperty,
        base::Value(std::move(capabilities_dict)));
    base::RunLoop().RunUntilIdle();
  }

  void SetReadinessCheckResultReady() {
    network_handler_test_helper_->manager_test()
        ->SetSimulateCheckTetheringReadinessResult(
            FakeShillSimulatedResult::kSuccess,
            shill::kTetheringReadinessReady);
    base::RunLoop().RunUntilIdle();
  }

  NetworkHandlerTestHelper* helper() {
    return network_handler_test_helper_.get();
  }

  void EnableHotspot() {
    cros_hotspot_config_test_helper_->EnableHotspot();
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr mojom_config) {
    cros_hotspot_config_test_helper_->SetHotspotConfig(std::move(mojom_config));
    base::RunLoop().RunUntilIdle();
  }

  void NotifyHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) {
    NetworkHandler* network_handler = NetworkHandler::Get();
    network_handler->hotspot_enabled_state_notifier()->OnHotspotTurnedOff(
        disable_reason);
  }

  void AddActiveCellularService() {
    network_handler_test_helper_->service_test()->AddService(
        kCellularServicePath, kCellularServiceGuid, kCellularServiceName,
        shill::kTypeCellular, shill::kStateOnline, /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotStateInShill(const std::string& state) {
    base::Value::Dict status_dict;
    status_dict.Set(shill::kTetheringStatusStateProperty, state);
    network_handler_test_helper_->manager_test()->SetManagerProperty(
        shill::kTetheringStatusProperty, base::Value(std::move(status_dict)));
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  std::unique_ptr<hotspot_config::CrosHotspotConfigTestHelper>
      cros_hotspot_config_test_helper_;
  std::unique_ptr<HotspotNotifier> hotspot_notifier_;
  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig> cros_hotspot_config_;
};

TEST_F(HotspotNotifierTest, WiFiTurnedOff) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      HotspotNotifier::kWiFiTurnedOffNotificationId, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kWiFiTurnedOffNotificationId));
}

TEST_F(HotspotNotifierTest, AdminRestricted) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));

  NotifyHotspotTurnedOff(
      hotspot_config::mojom::DisableReason::kProhibitedByPolicy);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kAdminRestrictedNotificationId));
}

TEST_F(HotspotNotifierTest, WiFiTurnedOn) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));

  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kWifiEnabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOnNotificationId));
}

TEST_F(HotspotNotifierTest, AutoDisabled) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));

  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kAutoDisabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kAutoDisabledNotificationId));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      HotspotNotifier::kAutoDisabledNotificationId, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kAutoDisabledNotificationId));
}

TEST_F(HotspotNotifierTest, InternalError) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  EnableHotspot();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));

  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kInternalError);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kInternalErrorNotificationId));
}

TEST_F(HotspotNotifierTest, HotspotTurnedOn) {
  SetValidHotspotCapabilities();
  SetReadinessCheckResultReady();
  AddActiveCellularService();
  helper()->manager_test()->SetSimulateTetheringEnableResult(
      FakeShillSimulatedResult::kSuccess, shill::kTetheringEnableResultSuccess);
  base::RunLoop().RunUntilIdle();

  SetHotspotConfig(GenerateTestConfig());
  EnableHotspot();
  SetHotspotStateInShill(shill::kTetheringStateActive);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kHotspotTurnedOnNotificationId));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      HotspotNotifier::kHotspotTurnedOnNotificationId, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kHotspotTurnedOnNotificationId));
}

}  // namespace ash
