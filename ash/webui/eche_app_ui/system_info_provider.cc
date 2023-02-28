// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/webui/eche_app_ui/mojom/types_mojom_traits.h"
#include "ash/webui/eche_app_ui/system_info.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "crypto/sha2.h"

namespace ash {
namespace eche_app {

namespace network_config = ::chromeos::network_config;
using network_config::mojom::ConnectionStateType;

const char kJsonDeviceNameKey[] = "device_name";
const char kJsonBoardNameKey[] = "board_name";
const char kJsonTabletModeKey[] = "tablet_mode";
const char kJsonWifiConnectionStateKey[] = "wifi_connection_state";
const char kJsonDebugModeKey[] = "debug_mode";
const char kJsonGaiaIdKey[] = "gaia_id";
const char kJsonDeviceTypeKey[] = "device_type";
const char kJsonMeasureLatencyKey[] = "measure_latency";
const char kJsonSendStartSignalingKey[] = "send_start_signaling";
const char kJsonDisableStunServerKey[] = "disable_stun_server";
const char kJsonCheckAndroidNetworkInfoKey[] = "check_android_network_info";

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
  if (TabletMode::Get()) {
    TabletMode::Get()->AddObserver(this);
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
  if (TabletMode::Get()) {
    TabletMode::Get()->RemoveObserver(this);
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
  json_dictionary.Set(kJsonTabletModeKey, TabletMode::Get()->InTabletMode());
  json_dictionary.Set(kJsonGaiaIdKey, system_info_->GetGaiaId());
  json_dictionary.Set(kJsonDeviceTypeKey, system_info_->GetDeviceType());
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
  if (!observer_remote_.is_bound()) {
    return;
  }

  PA_LOG(VERBOSE) << "OnAndroidDeviceNetworkInfoChanged";
  observer_remote_->OnAndroidDeviceNetworkInfoChanged(
      is_different_network, android_device_on_cellular);
}

// TabletModeObserver implementation:
void SystemInfoProvider::OnTabletModeStarted() {
  SetTabletModeChanged(true);
}

void SystemInfoProvider::OnTabletModeEnded() {
  SetTabletModeChanged(false);
}

// network_config::mojom::CrosNetworkConfigObserver implementation:
void SystemInfoProvider::OnNetworkStateChanged(
    network_config::mojom::NetworkStatePropertiesPtr network) {
  FetchWifiNetworkList();
}

void SystemInfoProvider::FetchWifiNetworkSsidHash() {
  PA_LOG(INFO) << "echeapi SystemInfoProvider FetchWifiNetworkSsidHash";
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kVisible,
          network_config::mojom::NetworkType::kWiFi,
          network_config::mojom::kNoLimit),
      base::BindOnce(&SystemInfoProvider::OnWifiNetworkListSsidFetch,
                     base::Unretained(this)));
}

void SystemInfoProvider::OnWifiNetworkListSsidFetch(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  PA_LOG(INFO) << "echeapi SystemInfoProvider OnWifiNetworkListSsidFetch";
  for (const auto& network : networks) {
    if (network->type == chromeos::network_config::mojom::NetworkType::kWiFi) {
      std::string wifi_ssid = network->type_state->get_wifi()->ssid;
      std::string hashed_wifi_ssid = crypto::SHA256HashString(wifi_ssid);
      hashed_wifi_ssid_ = hashed_wifi_ssid;
      return;
    }
  }
  hashed_wifi_ssid_ = std::string();
}

void SystemInfoProvider::FetchWifiNetworkList() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kVisible,
          network_config::mojom::NetworkType::kWiFi,
          network_config::mojom::kNoLimit),
      base::BindOnce(&SystemInfoProvider::OnWifiNetworkList,
                     base::Unretained(this)));
}

void SystemInfoProvider::OnWifiNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  using network_config::mojom::NetworkType;

  for (const auto& network : networks) {
    if (network->type == NetworkType::kWiFi) {
      wifi_connection_state_ = network->connection_state;
      return;
    }
  }
}

}  // namespace eche_app
}  // namespace ash
