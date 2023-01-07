// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/values.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"

namespace chrome {
namespace {

ash::UpdateEngineClient* GetUpdateEngineClient() {
  ash::UpdateEngineClient* update_engine_client =
      ash::UpdateEngineClient::Get();
  DCHECK(update_engine_client);
  return update_engine_client;
}

chromeos::PowerManagerClient* GetPowerManagerClient() {
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  return power_manager_client;
}

// Whether Chrome should send stop request to a session manager.
bool g_send_stop_request_to_session_manager = false;

}  // namespace

void AttemptRelaunch() {
  GetPowerManagerClient()->RequestRestart(power_manager::REQUEST_RESTART_OTHER,
                                          "Chrome relaunch");
}

void RelaunchIgnoreUnloadHandlers() {
  AttemptRelaunch();
}

void RelaunchForUpdate() {
  DCHECK(UpdatePending());
  GetUpdateEngineClient()->RebootAfterUpdate();
}

bool UpdatePending() {
  if (!ash::DBusThreadManager::IsInitialized())
    return false;

  return GetUpdateEngineClient()->GetLastStatus().current_operation() ==
         update_engine::UPDATED_NEED_REBOOT;
}

bool SetLocaleForNextStart(PrefService* local_state) {
  // If a policy mandates the login screen locale, use it.
  ash::CrosSettings* cros_settings = ash::CrosSettings::Get();
  const base::Value::List* login_screen_locales = nullptr;
  if (cros_settings->GetList(ash::kDeviceLoginScreenLocales,
                             &login_screen_locales) &&
      !login_screen_locales->empty() &&
      login_screen_locales->front().is_string()) {
    std::string login_screen_locale = login_screen_locales->front().GetString();
    local_state->SetString(language::prefs::kApplicationLocale,
                           login_screen_locale);
    return true;
  }

  // Login screen should show up in owner's locale.
  std::string owner_locale = local_state->GetString(prefs::kOwnerLocale);
  std::string pref_locale =
      local_state->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&pref_locale);
  if (!owner_locale.empty() && pref_locale != owner_locale &&
      !local_state->IsManagedPreference(language::prefs::kApplicationLocale)) {
    local_state->SetString(language::prefs::kApplicationLocale, owner_locale);
    return true;
  }

  return false;
}

bool IsSendingStopRequestToSessionManager() {
  return g_send_stop_request_to_session_manager;
}

void SetSendStopRequestToSessionManager(bool should_send_request) {
  g_send_stop_request_to_session_manager = should_send_request;
}

}  // namespace chrome
