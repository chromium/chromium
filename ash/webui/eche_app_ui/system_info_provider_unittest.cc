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
const bool kFakeDebugMode = false;

void ParseJson(const std::string& json,
               std::string& device_name,
               std::string& board_name,
               bool& tablet_mode,
               std::string& wifi_connection_state,
               bool& debug_mode) {
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(json);
  base::DictionaryValue* message_dictionary;
  message_value->GetAsDictionary(&message_dictionary);
  const std::string* device_name_ptr =
      message_dictionary->FindStringKey(kJsonDeviceNameKey);
  if (device_name_ptr)
    device_name = *device_name_ptr;
  const std::string* board_name_ptr =
      message_dictionary->FindStringKey(kJsonBoardNameKey);
  if (board_name_ptr)
    board_name = *board_name_ptr;
  absl::optional<bool> tablet_mode_opt =
      message_dictionary->FindBoolKey(kJsonTabletModeKey);
  if (tablet_mode_opt.has_value())
    tablet_mode = tablet_mode_opt.value();
  const std::string* wifi_connection_state_ptr =
      message_dictionary->FindStringKey(kJsonWifiConnectionStateKey);
  if (wifi_connection_state_ptr)
    wifi_connection_state = *wifi_connection_state_ptr;
  absl::optional<bool> debug_mode_opt =
      message_dictionary->FindBoolKey(kJsonDebugModeKey);
  if (debug_mode_opt.has_value())
    debug_mode = debug_mode_opt.value();
}

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
};

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

class FakeObserver : public mojom::SystemInfoObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_backlight_state_calls() const {
    return num_backlight_state_calls_;
  }
  size_t num_tablet_state_calls() const { return num_tablet_state_calls_; }

  // mojom::SystemInfoObserver:
  void OnScreenBacklightStateChanged(
      ash::ScreenBacklightState screen_state) override {
    ++num_backlight_state_calls_;
    if (task_runner_) {
      task_runner_->Finish();
    }
  }

  // mojom::SystemInfoObserver:
  void OnReceivedTabletModeChanged(bool is_tablet_mode) override {
    ++num_tablet_state_calls_;
    if (task_runner_) {
      task_runner_->Finish();
    }
  }

  static void setTaskRunner(TaskRunner* task_runner) {
    task_runner_ = task_runner;
  }

  mojo::Receiver<mojom::SystemInfoObserver> receiver{this};

 private:
  size_t num_backlight_state_calls_ = 0;
  size_t num_tablet_state_calls_ = 0;
  static TaskRunner* task_runner_;
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

ash::eche_app::TaskRunner* ash::eche_app::FakeObserver::task_runner_ = nullptr;
std::string ash::eche_app::Callback::system_info_ = "";

class SystemInfoProviderTest : public testing::Test {
 protected:
  SystemInfoProviderTest() {
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
  }
  SystemInfoProviderTest(const SystemInfoProviderTest&) = delete;
  SystemInfoProviderTest& operator=(const SystemInfoProviderTest&) = delete;
  ~SystemInfoProviderTest() override = default;
  std::unique_ptr<FakeTabletMode> tablet_mode_controller_;
  std::unique_ptr<FakeObserver> fake_observer_;

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
    fake_observer_ = std::make_unique<FakeObserver>();
    system_info_provider_->SetSystemInfoObserver(
        fake_observer_->receiver.BindNewPipeAndPassRemote());
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

  void SetOnScreenBacklightStateChanged() {
    system_info_provider_->OnScreenBacklightStateChanged(
        ash::ScreenBacklightState::OFF);
  }

  void OnTabletModeStarted() { system_info_provider_->OnTabletModeStarted(); }

  void OnTabletModeEnded() { system_info_provider_->OnTabletModeEnded(); }

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

  size_t GetNumTabletStateObserverCalls() const {
    return fake_observer_->num_tablet_state_calls();
  }
  size_t GetNumBacklightStateObserverCalls() const {
    return fake_observer_->num_backlight_state_calls();
  }
  TaskRunner task_runner_;

 private:
  // base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
};

TEST_F(SystemInfoProviderTest, GetSystemInfoHasCorrectJson) {
  std::string device_name = "";
  std::string board_name = "";
  bool tablet_mode = false;
  std::string wifi_connection_state = "";
  bool debug_mode = true;

  GetSystemInfo();
  std::string json = Callback::GetSystemInfo();
  ParseJson(json, device_name, board_name, tablet_mode, wifi_connection_state,
            debug_mode);

  EXPECT_EQ(device_name, kFakeDeviceName);
  EXPECT_EQ(board_name, kFakeBoardName);
  EXPECT_EQ(tablet_mode, kFakeTabletMode);
  EXPECT_EQ(wifi_connection_state, "connected");
  EXPECT_EQ(debug_mode, kFakeDebugMode);
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenBacklightChanged) {
  FakeObserver::setTaskRunner(&task_runner_);
  SetOnScreenBacklightStateChanged();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumBacklightStateObserverCalls());
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenTabletModeStarted) {
  FakeObserver::setTaskRunner(&task_runner_);
  OnTabletModeStarted();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumTabletStateObserverCalls());
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenTabletModeEnded) {
  FakeObserver::setTaskRunner(&task_runner_);
  OnTabletModeEnded();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumTabletStateObserverCalls());
}

}  // namespace eche_app
}  // namespace ash
