// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/mojom/types_mojom_traits.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "crypto/sha2.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash::eche_app {

namespace network_config = ::chromeos::network_config;
using network_config::mojom::ConnectionStateType;

const char kJsonDeviceNameKey[] = "device_name";
const char kJsonBoardNameKey[] = "board_name";
const char kJsonTabletModeKey[] = "tablet_mode";
const char kJsonWifiConnectionStateKey[] = "wifi_connection_state";
const char kJsonDebugModeKey[] = "debug_mode";
const char kJsonGaiaIdKey[] = "gaia_id";
const char kJsonDeviceTypeKey[] = "device_type";
const char kJsonOsVersionKey[] = "os_version";
const char kJsonChannelKey[] = "channel";
const char kJsonMeasureLatencyKey[] = "measure_latency";
const char kJsonSendStartSignalingKey[] = "send_start_signaling";
const char kJsonDisableStunServerKey[] = "disable_stun_server";
const char kJsonCheckAndroidNetworkInfoKey[] = "check_android_network_info";
const char kJsonProcessAndroidAccessibilityTreeKey[] = "process_android_accessibility_tree";

const std::map<ConnectionStateType, const char*> CONNECTION_STATE_TYPE{
    {ConnectionStateType::kOnline, "online"},
    {ConnectionStateType::kConnected, "connected"},
    {ConnectionStateType::kPortal, "portal"},
    {ConnectionStateType::kConnecting, "connecting"},
    {ConnectionStateType::kNotConnected, "not_connected"}};

SystemInfoProvider::SystemInfoProvider(
    std::unique_ptr<SystemInfo> system_info,
    network_config::mojom::CrosNetworkConfig* cros_network_config)
    : system_info_(std::move(system_info)),
      cros_network_config_(cros_network_config),
      wifi_connection_state_(ConnectionStateType::kNotConnected) {
  // TODO(samchiu): The intention of null check was for unit test. Add a fake
  // ScreenBacklight object to remove null check.
  if (ScreenBacklight::Get()) {
    ScreenBacklight::Get()->AddObserver(this);
  }
  cros_network_config_->AddObserver(
      cros_network_config_receiver_.BindNewPipeAndPassRemote());
  FetchWifiNetworkList();
}

SystemInfoProvider::SystemInfoProvider()
    : system_info_(nullptr),
      cros_network_config_(nullptr),
      wifi_connection_state_(ConnectionStateType::kNotConnected) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider SystemInfoProvider";
}

SystemInfoProvider::~SystemInfoProvider() {
  // Ash may be released before us.
  if (ScreenBacklight::Get()) {
    ScreenBacklight::Get()->RemoveObserver(this);
  }
}

std::string SystemInfoProvider::GetHashedWiFiSsid() {
  return hashed_wifi_ssid_;
}

void SystemInfoProvider::GetSystemInfo(
    base::OnceCallback<void(const std::string&)> callback) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider GetSystemInfo";
  base::Value::Dict json_dictionary;
  json_dictionary.Set(kJsonDeviceNameKey, system_info_->GetDeviceName());
  json_dictionary.Set(kJsonBoardNameKey, system_info_->GetBoardName());
  json_dictionary.Set(kJsonTabletModeKey,
                      display::Screen::GetScreen()->InTabletMode());
  json_dictionary.Set(kJsonGaiaIdKey, system_info_->GetGaiaId());
  json_dictionary.Set(kJsonDeviceTypeKey, system_info_->GetDeviceType());
  if (features::IsEcheMetricsRevampEnabled()) {
    json_dictionary.Set(kJsonOsVersionKey, system_info_->GetOsVersion());
    json_dictionary.Set(kJsonChannelKey, system_info_->GetChannel());
  }
  auto found_type = CONNECTION_STATE_TYPE.find(wifi_connection_state_);
  std::string connecton_state_string =
      found_type == CONNECTION_STATE_TYPE.end() ? "" : found_type->second;
  json_dictionary.Set(kJsonWifiConnectionStateKey, connecton_state_string);
  json_dictionary.Set(kJsonDebugModeKey, base::FeatureList::IsEnabled(
                                             features::kEcheSWADebugMode));
  json_dictionary.Set(
      kJsonMeasureLatencyKey,
      base::FeatureList::IsEnabled(features::kEcheSWAMeasureLatency));
  json_dictionary.Set(
      kJsonSendStartSignalingKey,
      base::FeatureList::IsEnabled(features::kEcheSWASendStartSignaling));
  json_dictionary.Set(
      kJsonDisableStunServerKey,
      base::FeatureList::IsEnabled(features::kEcheSWADisableStunServer));
  json_dictionary.Set(
      kJsonCheckAndroidNetworkInfoKey,
      base::FeatureList::IsEnabled(features::kEcheSWACheckAndroidNetworkInfo));
  json_dictionary.Set(
      kJsonProcessAndroidAccessibilityTreeKey,
      base::FeatureList::IsEnabled(features::kEcheSWAProcessAndroidAccessibilityTree));

  std::string json_message;
  base::JSONWriter::Write(json_dictionary, &json_message);
  std::move(callback).Run(json_message);
}

void SystemInfoProvider::SetSystemInfoObserver(
    mojo::PendingRemote<mojom::SystemInfoObserver> observer) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider SetSystemInfoObserver";
  observer_remote_.reset();
  observer_remote_.Bind(std::move(observer));
}

void SystemInfoProvider::Bind(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  info_receiver_.reset();
  info_receiver_.Bind(std::move(receiver));
}

void SystemInfoProvider::OnScreenBacklightStateChanged(
    ash::ScreenBacklightState screen_state) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider OnScreenBacklightStateChanged";
  if (!observer_remote_.is_bound()) {
    return;
  }

  observer_remote_->OnScreenBacklightStateChanged(screen_state);
}

void SystemInfoProvider::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      SetTabletModeChanged(true);
      break;
    case display::TabletState::kInClamshellMode:
      SetTabletModeChanged(false);
      break;
  }
}

void SystemInfoProvider::SetTabletModeChanged(bool enabled) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider SetTabletModeChanged";
  if (!observer_remote_.is_bound()) {
    return;
  }
  PA_LOG(VERBOSE) << "OnReceivedTabletModeChanged:" << enabled;
  observer_remote_->OnReceivedTabletModeChanged(enabled);
}

void SystemInfoProvider::SetAndroidDeviceNetworkInfoChanged(
    bool is_different_network,
    bool android_device_on_cellular) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider "
                  "SetAndroidDeviceNetworkInfoChanged is_different_network:"
               << is_different_network;
  PA_LOG(INFO)
      << "echeapi SystemInfoProvider "
         "SetAndroidDeviceNetworkInfoChanged android_device_on_cellular:"
      << android_device_on_cellular;

  is_different_network_ = is_different_network;
  android_device_on_cellular_ = android_device_on_cellular;

  if (!observer_remote_.is_bound()) {
    return;
  }

  PA_LOG(VERBOSE) << "OnAndroidDeviceNetworkInfoChanged";
  observer_remote_->OnAndroidDeviceNetworkInfoChanged(
      is_different_network, android_device_on_cellular);
}

// network_config::mojom::CrosNetworkConfigObserver implementation:
void SystemInfoProvider::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  FetchWifiNetworkList();
}

void SystemInfoProvider::FetchWifiNetworkSsidHash() {
  PA_LOG(INFO) << "echeapi SystemInfoProvider FetchWifiNetworkSsidHash";
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kWiFi,
          network_config::mojom::kNoLimit),
      base::BindOnce(&SystemInfoProvider::OnActiveWifiNetworkListFetched,
                     base::Unretained(this)));
}

void SystemInfoProvider::FetchWifiNetworkList() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kWiFi,
          network_config::mojom::kNoLimit),
      base::BindOnce(&SystemInfoProvider::OnActiveWifiNetworkListFetched,
                     base::Unretained(this)));
}

void SystemInfoProvider::OnActiveWifiNetworkListFetched(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  for (const auto& network : networks) {
    if (network->type == chromeos::network_config::mojom::NetworkType::kWiFi) {
      hashed_wifi_ssid_ =
          crypto::SHA256HashString(network->type_state->get_wifi()->ssid);
      wifi_connection_state_ = network->connection_state;
      return;
    }
  }

  // Reset connection state and SSID hash if there is no active WiFi network.
  wifi_connection_state_ = ConnectionStateType::kNotConnected;
  hashed_wifi_ssid_ = std::string();
}

}  // namespace ash::eche_app
