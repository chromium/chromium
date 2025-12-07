// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_network_state_observer.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

// TODO(b/365490226): replace this error with shill constant.
constexpr char kConfigFailureTemporaryServiceConfiguredButNotUsable[] =
    "Config.CreateConfiguration Temporary service configured but not usable";

void CopyPropertyIfExists(std::string_view key,
                          const base::Value::Dict& shill_properties,
                          base::Value::Dict& new_properties) {
  const base::Value* property_value = shill_properties.Find(key);
  if (property_value) {
    new_properties.Set(key, (*property_value).Clone());
  }
}

}  // namespace

KioskNetworkStateObserver::KioskNetworkStateObserver(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled,
      base::BindRepeating(
          &KioskNetworkStateObserver::PolicyChanged,
          // It is safe to use `base::Unretained` since this class
          // owns `pref_change_registrar_` and  it is destroyed before `this` .
          base::Unretained(this)));

  if (IsPolicyEnabled()) {
    StartActiveWifiExposureProcess();
  }
}

KioskNetworkStateObserver::~KioskNetworkStateObserver() = default;

bool KioskNetworkStateObserver::IsPolicyEnabled() const {
  return pref_service_->GetBoolean(
      prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled);
}

void KioskNetworkStateObserver::SetWifiExposureAttemptCallbackForTesting(
    base::RepeatingCallback<void(bool is_successful_attempt)> callback) {
  wifi_exposure_attempt_callback_ = std::move(callback);
}

void KioskNetworkStateObserver::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  ExposeActiveWifiConfiguration();
}

void KioskNetworkStateObserver::StartActiveWifiExposureProcess() {
  if (active_wifi_exposed_) {
    return;
  }
  network_state_handler_observation_.Observe(
      NetworkHandler::Get()->network_state_handler());
  ExposeActiveWifiConfiguration();
}

void KioskNetworkStateObserver::StopActiveWifiExposureProcess() {
  network_state_handler_observation_.Reset();
}

void KioskNetworkStateObserver::ExposeActiveWifiConfiguration() {
  if (active_wifi_exposed_) {
    return;
  }

  const auto* network_state =
      ash::NetworkHandler::Get()->network_state_handler()->ActiveNetworkByType(
          NetworkTypePattern::WiFi());
  if (!network_state) {
    MaybeRunWifiExposureAttemptCallback(false);
    return;
  }

  active_wifi_exposed_ = true;
  ShillServiceClient::Get()->GetWiFiPassphrase(
      dbus::ObjectPath(network_state->path()),
      base::BindOnce(&KioskNetworkStateObserver::OnGetWifiPassphraseResult,
                     weak_ptr_factory_.GetWeakPtr(), network_state->path()),
      base::BindOnce(&KioskNetworkStateObserver::OnGetWifiPassphraseError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskNetworkStateObserver::OnGetWifiPassphraseResult(
    const std::string& service_path,
    const std::string& passphrase) {
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      service_path,
      base::BindOnce(&KioskNetworkStateObserver::ReceiveProperties,
                     weak_ptr_factory_.GetWeakPtr(), passphrase));
}

void KioskNetworkStateObserver::OnGetWifiPassphraseError(
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Wi-Fi passphrase error:" << error_name << " "
                 << error_message;
  FailCurrentAttempt();
}

void KioskNetworkStateObserver::ReceiveProperties(
    const std::string& passphrase,
    const std::string& service_path,
    std::optional<base::Value::Dict> optional_shill_properties) {
  if (!optional_shill_properties) {
    NET_LOG(ERROR) << "Received shill properties are empty.";
    return;
  }
  const base::Value::Dict& shill_properties = optional_shill_properties.value();
  base::Value::Dict properties;

  properties.Set(shill::kProfileProperty,
                 NetworkProfileHandler::GetSharedProfilePath());
  properties.Set(shill::kPassphraseProperty, passphrase);

  CopyPropertyIfExists(shill::kSecurityClassProperty, shill_properties,
                       properties);
  CopyPropertyIfExists(shill::kWifiHexSsid, shill_properties, properties);
  CopyPropertyIfExists(shill::kGuidProperty, shill_properties, properties);
  CopyPropertyIfExists(shill::kAutoConnectProperty, shill_properties,
                       properties);
  CopyPropertyIfExists(shill::kTypeProperty, shill_properties, properties);
  CopyPropertyIfExists(shill::kSaveCredentialsProperty, shill_properties,
                       properties);
  CopyPropertyIfExists(shill::kModeProperty, shill_properties, properties);
  CopyPropertyIfExists(shill::kONCSourceProperty, shill_properties, properties);
  CopyPropertyIfExists(shill::kWifiHiddenSsid, shill_properties, properties);

  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(
          std::move(properties),
          base::BindOnce(
              &KioskNetworkStateObserver::OnCreatedShillConfigSuccess,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &KioskNetworkStateObserver::OnCreatedShillConfigFailure,
              weak_ptr_factory_.GetWeakPtr()));
}

void KioskNetworkStateObserver::OnCreatedShillConfigSuccess(
    const std::string&,
    const std::string&) {
  NET_LOG(DEBUG)
      << "Active WiFi configuration is successfully copied to the device "
         "level settings.";
  // Copy only the first active wifi configuration.
  StopActiveWifiExposureProcess();
  MaybeRunWifiExposureAttemptCallback(true);
}

void KioskNetworkStateObserver::OnCreatedShillConfigFailure(
    const std::string& error) {
  if (error == kConfigFailureTemporaryServiceConfiguredButNotUsable) {
    // The service is moved to the default profile, which means the successful
    // WiFi scope change.
    OnCreatedShillConfigSuccess("", "");
    return;
  }

  NET_LOG(ERROR) << "Shill config failure: " << error;
  FailCurrentAttempt();
}

void KioskNetworkStateObserver::PolicyChanged() {
  if (IsPolicyEnabled()) {
    StartActiveWifiExposureProcess();
  } else {
    StopActiveWifiExposureProcess();
  }
}

void KioskNetworkStateObserver::FailCurrentAttempt() {
  // We set `active_wifi_exposed_` to true when we attempt to expose the active
  // WiFi. If the attempt fails, we need to reset it.
  active_wifi_exposed_ = false;

  if (++wifi_exposure_attempts_ >= kMaxWifiExposureAttempts) {
    StopActiveWifiExposureProcess();
  }
  MaybeRunWifiExposureAttemptCallback(false);
}

void KioskNetworkStateObserver::MaybeRunWifiExposureAttemptCallback(
    bool is_successful_attempt) {
  if (wifi_exposure_attempt_callback_) {
    wifi_exposure_attempt_callback_.Run(is_successful_attempt);
  }
}

}  // namespace ash
