// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/hotspot_notifier.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

namespace {

const char kCellularServicePath[] = "/service/cellular0";
const char kCellularServiceGuid[] = "cellular_guid0";
const char kCellularServiceName[] = "cellular_name0";

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
    GetHotspotConfigService(cros_hotspot_config_.BindNewPipeAndPassReceiver());
    hotspot_notifier_ = std::make_unique<ash::HotspotNotifier>();

    NoSessionAshTestBase::SetUp();
    LogIn();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    network_config_helper_.reset();
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

  hotspot_config::mojom::HotspotControlResult EnableHotspot() {
    base::RunLoop run_loop;
    hotspot_config::mojom::HotspotControlResult out_result;
    cros_hotspot_config_->EnableHotspot(base::BindLambdaForTesting(
        [&](hotspot_config::mojom::HotspotControlResult result) {
          out_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_result;
  }

  void AddActiveCellularService() {
    network_handler_test_helper_->service_test()->AddService(
        kCellularServicePath, kCellularServiceGuid, kCellularServiceName,
        shill::kTypeCellular, shill::kStateOnline, /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
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

  EXPECT_EQ(hotspot_config::mojom::HotspotControlResult::kSuccess,
            EnableHotspot());
  EXPECT_TRUE(message_center::MessageCenter::Get()->FindVisibleNotificationById(
      HotspotNotifier::kWiFiTurnedOffNotificationId));
}

}  // namespace ash
