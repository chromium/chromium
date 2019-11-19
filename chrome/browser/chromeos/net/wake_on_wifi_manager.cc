// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_connection_observer.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kWakeOnWifiNone[] = "none";
const char kWakeOnWifiPacket[] = "packet";
const char kWakeOnWifiDarkConnect[] = "darkconnect";
const char kWakeOnWifiPacketAndDarkConnect[] = "packet_and_darkconnect";

std::string WakeOnWifiFeatureToString(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  switch (feature) {
    case WakeOnWifiManager::WAKE_ON_WIFI_NONE:
      return kWakeOnWifiNone;
    case WakeOnWifiManager::WAKE_ON_WIFI_PACKET:
      return kWakeOnWifiPacket;
    case WakeOnWifiManager::WAKE_ON_WIFI_DARKCONNECT:
      return kWakeOnWifiDarkConnect;
    case WakeOnWifiManager::WAKE_ON_WIFI_PACKET_AND_DARKCONNECT:
      return kWakeOnWifiPacketAndDarkConnect;
    case WakeOnWifiManager::INVALID:
      return std::string();
    case WakeOnWifiManager::NOT_SUPPORTED:
      NOTREACHED();
      return std::string();
  }

  NOTREACHED() << "Unknown wake on wifi feature: " << feature;
  return std::string();
}

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
WakeOnWifiManager* g_wake_on_wifi_manager = NULL;

}  // namespace

// static
WakeOnWifiManager* WakeOnWifiManager::Get() {
  DCHECK(g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_wake_on_wifi_manager;
}

// static
bool WakeOnWifiManager::IsWakeOnPacketEnabled(WakeOnWifiFeature feature) {
  return feature & WakeOnWifiManager::WAKE_ON_WIFI_PACKET;
}

WakeOnWifiManager::WakeOnWifiManager()
    : current_feature_(WakeOnWifiManager::INVALID),
      wifi_properties_received_(false),
      extension_event_observer_(new ExtensionEventObserver()) {
  // This class must be constructed before any users are logged in, i.e., before
  // any profiles are created or added to the ProfileManager.  Additionally,
  // IsUserLoggedIn always returns true when we are not running on a Chrome OS
  // device so this check should only run on real devices.
  CHECK(!base::SysInfo::IsRunningOnChromeOS() ||
        !LoginState::Get()->IsUserLoggedIn());
  DCHECK(!g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_wake_on_wifi_manager = this;

  g_browser_process->profile_manager()->AddObserver(this);

  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);

  GetWifiDeviceProperties();
}

WakeOnWifiManager::~WakeOnWifiManager() {
  g_browser_process->profile_manager()->RemoveObserver(this);
  if (current_feature_ != NOT_SUPPORTED) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  DCHECK_EQ(this, g_wake_on_wifi_manager);
  g_wake_on_wifi_manager = nullptr;
}

void WakeOnWifiManager::OnPreferenceChanged(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (current_feature_ == NOT_SUPPORTED)
    return;
  if (!switches::WakeOnWifiEnabled())
    feature = WAKE_ON_WIFI_NONE;
  if (feature == current_feature_)
    return;

  current_feature_ = feature;

  // Update value of member variable feature for all connection observers.
  for (const auto& kv_pair : connection_observers_) {
    kv_pair.second->set_feature(current_feature_);
  }

  if (wifi_properties_received_)
    HandleWakeOnWifiFeatureUpdated();
}

bool WakeOnWifiManager::WakeOnWifiSupported() {
  return current_feature_ != NOT_SUPPORTED && current_feature_ != INVALID;
}

void WakeOnWifiManager::OnProfileAdded(Profile* profile) {
  auto result = connection_observers_.find(profile);

  // Only add the profile if it is not already present.
  if (result != connection_observers_.end())
    return;

  connection_observers_[profile] =
      std::make_unique<WakeOnWifiConnectionObserver>(
          profile, wifi_properties_received_, current_feature_,
          NetworkHandler::Get()->network_device_handler());

  gcm::GCMProfileServiceFactory::GetForProfile(profile)
      ->driver()
      ->WakeFromSuspendForHeartbeat(IsWakeOnPacketEnabled(current_feature_));
}

void WakeOnWifiManager::DeviceListChanged() {
  if (current_feature_ != NOT_SUPPORTED)
    GetWifiDeviceProperties();
}

void WakeOnWifiManager::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->Matches(NetworkTypePattern::WiFi()) &&
      current_feature_ != NOT_SUPPORTED) {
    GetWifiDeviceProperties();
  }
}

void WakeOnWifiManager::HandleWakeOnWifiFeatureUpdated() {
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::WiFi());
  if (!device)
    return;

  std::string feature_string(WakeOnWifiFeatureToString(current_feature_));
  DCHECK(!feature_string.empty());

  NetworkHandler::Get()->network_device_handler()->SetDeviceProperty(
      device->path(), shill::kWakeOnWiFiFeaturesEnabledProperty,
      base::Value(feature_string), base::DoNothing(),
      network_handler::ErrorCallback());

  bool wake_on_packet_enabled = IsWakeOnPacketEnabled(current_feature_);
  for (const auto& kv_pair : connection_observers_) {
    Profile* profile = kv_pair.first;
    gcm::GCMProfileServiceFactory::GetForProfile(profile)
        ->driver()
        ->WakeFromSuspendForHeartbeat(wake_on_packet_enabled);
  }

  extension_event_observer_->SetShouldDelaySuspend(wake_on_packet_enabled);
}

void WakeOnWifiManager::GetWifiDeviceProperties() {
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::WiFi());
  if (!device)
    return;

  NetworkHandler::Get()->network_device_handler()->GetDeviceProperties(
      device->path(),
      base::Bind(&WakeOnWifiManager::GetDevicePropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      network_handler::ErrorCallback());
}

void WakeOnWifiManager::GetDevicePropertiesCallback(
    const std::string& device_path,
    const base::DictionaryValue& properties) {
  std::string enabled;
  if (!properties.HasKey(shill::kWakeOnWiFiFeaturesEnabledProperty) ||
      !properties.GetString(shill::kWakeOnWiFiFeaturesEnabledProperty,
                            &enabled) ||
      enabled == shill::kWakeOnWiFiFeaturesEnabledNotSupported) {
    current_feature_ = NOT_SUPPORTED;
    connection_observers_.clear();
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
    extension_event_observer_.reset();
    g_browser_process->profile_manager()->RemoveObserver(this);

    return;
  }

  // We always resend the wake on wifi setting unless it hasn't been set yet.
  // This covers situations where shill restarts or ends up recreating the wifi
  // device (crbug.com/475199).
  if (current_feature_ != INVALID)
    HandleWakeOnWifiFeatureUpdated();

  if (wifi_properties_received_)
    return;

  wifi_properties_received_ = true;

  NetworkHandler::Get()
      ->network_device_handler()
      ->RemoveAllWifiWakeOnPacketConnections(base::DoNothing(),
                                             network_handler::ErrorCallback());

  for (const auto& kv_pair : connection_observers_) {
    kv_pair.second->HandleWifiDevicePropertiesReady();
  }
}

}  // namespace chromeos
