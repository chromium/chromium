// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_notifier.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/test/bind.h"
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
  HotspotNotifierTest() = default;
  HotspotNotifierTest(const HotspotNotifierTest&) = delete;
  HotspotNotifierTest& operator=(const HotspotNotifierTest&) = delete;
  ~HotspotNotifierTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();

    cros_network_config_test_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();

    NoSessionAshTestBase::SetUp();
    LogIn();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    cros_network_config_test_helper_.reset();
    network_handler_test_helper_.reset();
  }

  NetworkHandlerTestHelper* helper() {
    return network_handler_test_helper_.get();
  }

  void EnableHotspot() {
    ash_test_helper()->cros_hotspot_config_test_helper()->EnableHotspot();
    base::RunLoop().RunUntilIdle();
  }

  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr mojom_config) {
    ash_test_helper()->cros_hotspot_config_test_helper()->SetHotspotConfig(
        std::move(mojom_config));
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

  void UpdateHotspotInfo(hotspot_config::mojom::HotspotState state,
                         hotspot_config::mojom::HotspotAllowStatus allow_status,
                         uint32_t client_count = 0) {
    auto hotspot_info = hotspot_config::mojom::HotspotInfo::New();
    hotspot_info->state = state;
    hotspot_info->allow_status = allow_status;
    hotspot_info->client_count = client_count;
    ash_test_helper()->cros_hotspot_config_test_helper()->SetFakeHotspotInfo(
        std::move(hotspot_info));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  std::unique_ptr<HotspotNotifier> hotspot_notifier_;
};

TEST_F(HotspotNotifierTest, AdminRestricted) {
  EnableHotspot();
  NotifyHotspotTurnedOff(
      hotspot_config::mojom::DisableReason::kProhibitedByPolicy);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kAdminRestrictedNotificationId));

  UpdateHotspotInfo(hotspot_config::mojom::HotspotState::kEnabling,
                    hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kInternalErrorNotificationId));
}

TEST_F(HotspotNotifierTest, WiFiTurnedOn) {
  EnableHotspot();
  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kWifiEnabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOnNotificationId));

  UpdateHotspotInfo(hotspot_config::mojom::HotspotState::kEnabling,
                    hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kWiFiTurnedOnNotificationId));
}

TEST_F(HotspotNotifierTest, AutoDisabled) {
  EnableHotspot();
  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kAutoDisabled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kAutoDisabledNotificationId));

  UpdateHotspotInfo(hotspot_config::mojom::HotspotState::kEnabling,
                    hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kAutoDisabledNotificationId));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      HotspotNotifier::kAutoDisabledNotificationId, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kAutoDisabledNotificationId));
}

TEST_F(HotspotNotifierTest, InternalError) {
  EnableHotspot();
  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kInternalError);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kInternalErrorNotificationId));

  UpdateHotspotInfo(hotspot_config::mojom::HotspotState::kEnabling,
                    hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kInternalErrorNotificationId));
}

TEST_F(HotspotNotifierTest, DialogDismissWhenNotAllowed) {
  UpdateHotspotInfo(hotspot_config::mojom::HotspotState::kDisabled,
                    hotspot_config::mojom::HotspotAllowStatus::kAllowed);
  NotifyHotspotTurnedOff(hotspot_config::mojom::DisableReason::kInternalError);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kInternalErrorNotificationId));

  UpdateHotspotInfo(
      hotspot_config::mojom::HotspotState::kDisabled,
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData);
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          HotspotNotifier::kInternalErrorNotificationId));
}

TEST_F(HotspotNotifierTest, HotspotTurnedOn) {
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
