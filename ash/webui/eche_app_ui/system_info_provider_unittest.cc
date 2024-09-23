// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info_provider.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/display/test/test_screen.h"

namespace ash::eche_app {

namespace network_config = ::chromeos::network_config;
using network_config::mojom::ConnectionStateType;

const char kFakeDeviceName[] = "Guanru's Chromebook";
const char kFakeBoardName[] = "atlas";
const bool kFakeTabletMode = true;
const ConnectionStateType kFakeWifiConnectionState =
    ConnectionStateType::kConnected;
const bool kFakeDebugMode = false;
const char kFakeGaiaId[] = "123";
const char kFakeDeviceType[] = "Chromebook";
const char kFakeOsVersion[] = "1.2.3.4";
const char kFakeChannel[] = "Dev";
const bool kFakeMeasureLatency = false;
const bool kFakeSendStartSignaling = true;
const bool kFakeDisableStunServer = false;
const bool kFakeCheckAndroidNetworkInfo = true;
const bool kFakeProcessAndroidAccessibilityTree = true;

void ParseJson(const std::string& json,
               std::string& device_name,
               std::string& board_name,
               bool& tablet_mode,
               std::string& wifi_connection_state,
               bool& debug_mode,
               std::string& gaia_id,
               std::string& device_type,
               std::string& os_version,
               std::string& channel,
               bool& measure_latency,
               bool& send_start_signaling,
               bool& disable_stun_server,
               bool& check_android_network_info,
               bool& process_android_accessibility_tree) {
  std::optional<base::Value> message_value = base::JSONReader::Read(json);
  base::Value::Dict* message_dictionary = message_value->GetIfDict();
  const std::string* device_name_ptr =
      message_dictionary->FindString(kJsonDeviceNameKey);
  if (device_name_ptr)
    device_name = *device_name_ptr;
  const std::string* board_name_ptr =
      message_dictionary->FindString(kJsonBoardNameKey);
  if (board_name_ptr)
    board_name = *board_name_ptr;
  std::optional<bool> tablet_mode_opt =
      message_dictionary->FindBool(kJsonTabletModeKey);
  if (tablet_mode_opt.has_value())
    tablet_mode = tablet_mode_opt.value();
  const std::string* wifi_connection_state_ptr =
      message_dictionary->FindString(kJsonWifiConnectionStateKey);
  if (wifi_connection_state_ptr)
    wifi_connection_state = *wifi_connection_state_ptr;
  std::optional<bool> debug_mode_opt =
      message_dictionary->FindBool(kJsonDebugModeKey);
  if (debug_mode_opt.has_value())
    debug_mode = debug_mode_opt.value();
  const std::string* gaia_id_ptr =
      message_dictionary->FindString(kJsonGaiaIdKey);
  if (gaia_id_ptr)
    gaia_id = *gaia_id_ptr;
  const std::string* device_type_ptr =
      message_dictionary->FindString(kJsonDeviceTypeKey);
  if (device_type_ptr)
    device_type = *device_type_ptr;
  const std::string* os_version_ptr =
      message_dictionary->FindString(kJsonOsVersionKey);
  if (os_version_ptr) {
    os_version = *os_version_ptr;
  }
  const std::string* channel_ptr =
      message_dictionary->FindString(kJsonChannelKey);
  if (channel_ptr) {
    channel = *channel_ptr;
  }
  std::optional<bool> measure_latency_opt =
      message_dictionary->FindBool(kJsonMeasureLatencyKey);
  if (measure_latency_opt.has_value())
    measure_latency = measure_latency_opt.value();
  std::optional<bool> send_start_signaling_opt =
      message_dictionary->FindBool(kJsonSendStartSignalingKey);
  if (send_start_signaling_opt.has_value())
    send_start_signaling = send_start_signaling_opt.value();
  std::optional<bool> disable_stun_server_opt =
      message_dictionary->FindBool(kJsonDisableStunServerKey);
  if (disable_stun_server_opt.has_value())
    disable_stun_server = disable_stun_server_opt.value();
  std::optional<bool> check_android_network_info_opt =
      message_dictionary->FindBool(kJsonCheckAndroidNetworkInfoKey);
  if (check_android_network_info_opt.has_value())
    check_android_network_info = check_android_network_info_opt.value();
  std::optional<bool> process_android_accessibility_tree_opt =
      message_dictionary->FindBool(kJsonProcessAndroidAccessibilityTreeKey);
  if (process_android_accessibility_tree_opt.has_value())
    process_android_accessibility_tree = process_android_accessibility_tree_opt.value();
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

class FakeObserver : public mojom::SystemInfoObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_backlight_state_calls() const {
    return num_backlight_state_calls_;
  }
  size_t num_tablet_state_calls() const { return num_tablet_state_calls_; }
  size_t num_android_state_calls() const { return num_android_state_calls_; }

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

  void OnAndroidDeviceNetworkInfoChanged(
      bool is_different_network,
      bool android_device_on_cellular) override {
    ++num_android_state_calls_;
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
  size_t num_android_state_calls_ = 0;
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

  // testing::Test:
  void SetUp() override {
    display::Screen::GetScreen()->OverrideTabletStateForTesting(
        display::TabletState::kInTabletMode);

    system_info_provider_ =
        std::make_unique<SystemInfoProvider>(SystemInfo::Builder()
                                                 .SetDeviceName(kFakeDeviceName)
                                                 .SetBoardName(kFakeBoardName)
                                                 .SetGaiaId(kFakeGaiaId)
                                                 .SetDeviceType(kFakeDeviceType)
                                                 .SetOsVersion(kFakeOsVersion)
                                                 .SetChannel(kFakeChannel)
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
    system_info_provider_->OnActiveWifiNetworkListFetched(
        GetWifiNetworkStateList());
  }

  void SetOnScreenBacklightStateChanged() {
    system_info_provider_->OnScreenBacklightStateChanged(
        ash::ScreenBacklightState::OFF);
  }

  void SetAndroidDeviceNetworkInfoChanged() {
    system_info_provider_->SetAndroidDeviceNetworkInfoChanged(false, false);
  }

  void StartTabletMode() {
    system_info_provider_->OnDisplayTabletStateChanged(
        display::TabletState::kInTabletMode);
  }

  void EndTabletMode() {
    system_info_provider_->OnDisplayTabletStateChanged(
        display::TabletState::kInClamshellMode);
  }

  std::vector<network_config::mojom::NetworkStatePropertiesPtr>
  GetWifiNetworkStateList() {
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> result;

    // Create a WiFiStatePropertiesPtr object
    network_config::mojom::WiFiStatePropertiesPtr wifi_state_properties =
        network_config::mojom::WiFiStateProperties::New();

    // Set the required properties
    wifi_state_properties->bssid = "00:11:22:33:44:55";
    wifi_state_properties->frequency = 2412;
    wifi_state_properties->hex_ssid = "48656c6c6f";
    wifi_state_properties->security =
        network_config::mojom::SecurityType::kNone;
    wifi_state_properties->signal_strength = -50;
    wifi_state_properties->ssid = "Test";
    wifi_state_properties->hidden_ssid = false;

    // Create a new NetworkTypeStateProperties object with the
    // WiFiStateProperties
    network_config::mojom::NetworkTypeStatePropertiesPtr
        network_type_state_properties =
            network_config::mojom::NetworkTypeStateProperties::NewWifi(
                std::move(wifi_state_properties));

    auto network = network_config::mojom::NetworkStateProperties::New(
        /*connectable=*/true,
        /*connect_requested=*/false,
        /*connection_state=*/
        kFakeWifiConnectionState,
        /*error_state=*/std::nullopt,
        /*guid=*/"some_guid",
        /*name=*/"some_name",
        /*portal_state=*/
        network_config::mojom::PortalState::kUnknown,
        /*portal_probe_url=*/std::nullopt,
        /*priority=*/1,
        /*proxy_mode=*/network_config::mojom::ProxyMode::kDirect,
        /*prohibited_by_policy=*/false,
        /*source=*/
        network_config::mojom::OncSource::kUser,
        /*network_type*/ network_config::mojom::NetworkType::kWiFi,
        /*network_type_state*/ std::move(network_type_state_properties));

    network->type = network_config::mojom::NetworkType::kWiFi;
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
  size_t GetNumAndroidStateObserverCalls() const {
    return fake_observer_->num_android_state_calls();
  }

  TaskRunner task_runner_;

  std::unique_ptr<FakeObserver> fake_observer_;

 private:
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  mojo::Remote<network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
};

TEST_F(SystemInfoProviderTest, GetSystemInfoHasCorrectJson) {
  std::string device_name = "";
  std::string board_name = "";
  bool tablet_mode = false;
  std::string wifi_connection_state = "";
  bool debug_mode = true;
  std::string gaia_id = "";
  std::string device_type = "";
  std::string os_version = "";
  std::string channel = "";
  bool measure_latency = true;
  bool send_start_signaling = false;
  bool disable_stun_server = true;
  bool check_android_network_info = true;
  bool process_android_accessibility_tree = true;

  GetSystemInfo();
  std::string json = Callback::GetSystemInfo();
  ParseJson(json, device_name, board_name, tablet_mode, wifi_connection_state,
            debug_mode, gaia_id, device_type, os_version, channel,
            measure_latency, send_start_signaling, disable_stun_server,
            check_android_network_info, process_android_accessibility_tree);

  EXPECT_EQ(device_name, kFakeDeviceName);
  EXPECT_EQ(board_name, kFakeBoardName);
  EXPECT_EQ(tablet_mode, kFakeTabletMode);
  EXPECT_EQ(wifi_connection_state, "connected");
  EXPECT_EQ(debug_mode, kFakeDebugMode);
  EXPECT_EQ(gaia_id, kFakeGaiaId);
  EXPECT_EQ(device_type, kFakeDeviceType);
  EXPECT_EQ(os_version, kFakeOsVersion);
  EXPECT_EQ(channel, kFakeChannel);
  EXPECT_EQ(measure_latency, kFakeMeasureLatency);
  EXPECT_EQ(send_start_signaling, kFakeSendStartSignaling);
  EXPECT_EQ(disable_stun_server, kFakeDisableStunServer);
  EXPECT_EQ(check_android_network_info, kFakeCheckAndroidNetworkInfo);
  EXPECT_EQ(process_android_accessibility_tree, kFakeProcessAndroidAccessibilityTree);
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenBacklightChanged) {
  FakeObserver::setTaskRunner(&task_runner_);
  SetOnScreenBacklightStateChanged();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumBacklightStateObserverCalls());
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenTabletModeStarted) {
  FakeObserver::setTaskRunner(&task_runner_);
  StartTabletMode();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumTabletStateObserverCalls());
}

TEST_F(SystemInfoProviderTest, ObserverCalledWhenTabletModeEnded) {
  FakeObserver::setTaskRunner(&task_runner_);
  EndTabletMode();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumTabletStateObserverCalls());
}

TEST_F(SystemInfoProviderTest,
       ObserverCalledWhenAndroidDeviceNetworkStateChanged) {
  FakeObserver::setTaskRunner(&task_runner_);
  SetAndroidDeviceNetworkInfoChanged();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, GetNumAndroidStateObserverCalls());
}

}  // namespace ash::eche_app
