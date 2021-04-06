// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_handler.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/net/system_proxy_manager.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace {
const char kSystemProxyService[] = "system-proxy-service";
}

namespace policy {

SystemProxyHandler::SystemProxyHandler(chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings),
      system_proxy_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kSystemProxySettings,
          base::BindRepeating(
              &SystemProxyHandler::OnSystemProxySettingsPolicyChanged,
              base::Unretained(this)))) {
  // Fire it once so we're sure we get an invocation on startup.
  OnSystemProxySettingsPolicyChanged();
}

SystemProxyHandler::~SystemProxyHandler() = default;

void SystemProxyHandler::OnSystemProxySettingsPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &SystemProxyHandler::OnSystemProxySettingsPolicyChanged,
          base::Unretained(this)));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  const base::Value* proxy_settings =
      cros_settings_->GetPref(chromeos::kSystemProxySettings);

  if (!proxy_settings)
    return;

  bool system_proxy_enabled =
      proxy_settings->FindBoolKey(chromeos::kSystemProxySettingsKeyEnabled)
          .value_or(false);
  const std::string* username = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesUsername);

  const std::string* password = proxy_settings->FindStringKey(
      chromeos::kSystemProxySettingsKeySystemServicesPassword);

  const base::Value* auth_schemes =
      proxy_settings->FindListKey(chromeos::kSystemProxySettingsKeyAuthSchemes);

  std::vector<std::string> system_services_auth_schemes;
  if (auth_schemes) {
    for (const auto& auth_scheme : auth_schemes->GetList())
      system_services_auth_schemes.push_back(auth_scheme.GetString());
  }

  std::string system_services_username;
  std::string system_services_password;
  if (!username || username->empty() || !password || password->empty()) {
    NET_LOG(DEBUG) << "Proxy credentials for system traffic not set: "
                   << kSystemProxyService;
  } else {
    system_services_username = *username;
    system_services_password = *password;
  }

  auto* system_proxy_manager = GetSystemProxyManager();
  if (system_proxy_manager) {
    system_proxy_manager->SetPolicySettings(
        system_proxy_enabled, system_services_username,
        system_services_password, system_services_auth_schemes);
  } else {
    LOG(ERROR) << "SystemProxyManager was not initialized";
  }
}

void SystemProxyHandler::SetSystemProxyManagerForTesting(
    chromeos::SystemProxyManager* system_proxy_manager) {
  system_proxy_manager_for_testing_ = system_proxy_manager;
}

chromeos::SystemProxyManager* SystemProxyHandler::GetSystemProxyManager() {
  if (system_proxy_manager_for_testing_) {
    return system_proxy_manager_for_testing_;
  }
  return chromeos::SystemProxyManager::Get();
}

}  // namespace policy
