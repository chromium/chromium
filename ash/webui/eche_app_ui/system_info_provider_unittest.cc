// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info_provider.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

using chromeos::network_config::mojom::ConnectionStateType;
// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace network_config = ::chromeos::network_config;

const char kFakeDeviceName[] = "Guanru's Chromebook";
const char kFakeBoardName[] = "atlas";
const bool kFakeTabletMode = true;
const ConnectionStateType kFakeWifiConnectionState =
    ConnectionStateType::kConnected;

void ParseJson(const std::string& json,
               std::string& device_name,
               std::string& board_name,
               bool& tablet_mode,
               std::string& wifi_connection_state) {
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(json);
  base::DictionaryValue* message_dictionary;
  message_value->GetAsDictionary(&message_dictionary);
  message_dictionary->GetString(kJsonDeviceNameKey, &device_name);
  message_dictionary->GetString(kJsonBoardNameKey, &board_name);
  absl::optional<bool> tablet_mode_opt =
      message_dictionary->FindBoolKey(kJsonTabletModeKey);
  if (tablet_mode_opt.has_value())
    tablet_mode = tablet_mode_opt.value();
  message_dictionary->GetString(kJsonWifiConnectionStateKey,
                                &wifi_connection_state);
}

class FakeTabletMode : public ash::TabletMode {
 public:
  FakeTabletMode() = default;
  ~FakeTabletMode() override = default;

  // ash::TabletMode:
  void AddObserver(ash::TabletModeObserver* observer) override {
    DCHECK(!observer_);
    observer_ = observer;
  }

  void RemoveObserver(ash::TabletModeObserver* observer) override {
    DCHECK_EQ(observer_, observer);
    observer_ = nullptr;
  }

  bool InTabletMode() const override { return in_tablet_mode; }

  bool ForceUiTabletModeState(absl::optional<bool> enabled) override {
    return false;
  }

  void SetEnabledForTest(bool enabled) override {
    bool changed = (in_tablet_mode != enabled);
    in_tablet_mode = enabled;

    if (changed && observer_) {
      if (in_tablet_mode)
        observer_->OnTabletModeStarted();
      else
        observer_->OnTabletModeEnded();
    }
  }

 private:
  ash::TabletModeObserver* observer_ = nullptr;
  bool in_tablet_mode = false;
};

class Callback {
 public:
  static void GetSystemInfoCallback(const std::string& system_info) {
    system_info_ = system_info;
  }
  static std::string GetSystemInfo() { return system_info_; }
  static void resetSystemInfo() { system_info_ = ""; }

 private:
  static std::string system_info_;
};

std::string ash::eche_app::Callback::system_info_ = "";

class SystemInfoProviderTest : public testing::Test {
 protected:
  SystemInfoProviderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
  }
  SystemInfoProviderTest(const SystemInfoProviderTest&) = delete;
  SystemInfoProviderTest& operator=(const SystemInfoProviderTest&) = delete;
  ~SystemInfoProviderTest() override = default;
  std::unique_ptr<FakeTabletMode> tablet_mode_controller_;

  // testing::Test:
  void SetUp() override {
    tablet_mode_controller_ = std::make_unique<FakeTabletMode>();
    tablet_mode_controller_->SetEnabledForTest(true);
    system_info_provider_ =
        std::make_unique<SystemInfoProvider>(SystemInfo::Builder()
                                                 .SetDeviceName(kFakeDeviceName)
                                                 .SetBoardName(kFakeBoardName)
                                                 .Build(),
                                             remote_cros_network_config_.get());
    SetWifiConnectionStateList();
  }
  void TearDown() override {
    system_info_provider_.reset();
    Callback::resetSystemInfo();
  }
  void GetSystemInfo() {
    system_info_provider_->GetSystemInfo(
        base::BindOnce(&Callback::GetSystemInfoCallback));
  }
  void SetWifiConnectionStateList() {
    system_info_provider_->OnWifiNetworkList(GetWifiNetworkStateList());
  }
  std::vector<network_config::mojom::NetworkStatePropertiesPtr>
  GetWifiNetworkStateList() {
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> result;
    auto network =
        ::chromeos::network_config::mojom::NetworkStateProperties::New();
    network->type = chromeos::network_config::mojom::NetworkType::kWiFi;
    network->connection_state = kFakeWifiConnectionState;
    result.emplace_back(std::move(network));
    return result;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
};

TEST_F(SystemInfoProviderTest, GetSystemInfoHasCorrectJson) {
  std::string device_name = "";
  std::string board_name = "";
  bool tablet_mode = false;
  std::string wifi_connection_state = "";

  GetSystemInfo();
  std::string json = Callback::GetSystemInfo();
  ParseJson(json, device_name, board_name, tablet_mode, wifi_connection_state);

  EXPECT_EQ(device_name, kFakeDeviceName);
  EXPECT_EQ(board_name, kFakeBoardName);
  EXPECT_EQ(tablet_mode, kFakeTabletMode);
  EXPECT_EQ(wifi_connection_state, "connected");
}

}  // namespace eche_app
}  // namespace ash
