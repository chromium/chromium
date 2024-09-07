// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

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

void ReportSessionUMAMetrics() {
  // GetProfileByUser() will crash in tests if profile_manager() from
  // g_browser_process is not initialized.
  if (!user_manager::UserManager::IsInitialized() ||
      !g_browser_process->profile_manager()) {
    return;
  }

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    return;
  }

  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  // Could be nullptr in tests.
  if (!profile) {
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }

  base::Time session_start_time =
      prefs->GetTime(ash::prefs::kAshLoginSessionStartedTime);
  if (!session_start_time.is_null()) {
    base::TimeDelta duration = base::Time::Now() - session_start_time;
    // Use CustomCounts histogram instead of CustomTimes because the latter
    // allows 24 days maximum (data size limit) but we need 1 month.
    if (prefs->GetBoolean(ash::prefs::kAshLoginSessionStartedIsFirstSession)) {
      // Report 1 minute ... 30 days in minutes.
      base::UmaHistogramCustomCounts("Ash.Login.TotalFirstSessionDuration",
                                     duration.InMinutes(), 1,
                                     base::Days(30) / base::Minutes(1), 100);
    } else {
      // Report 1 minute ... 30 days in minutes.
      base::UmaHistogramCustomCounts("Ash.Login.TotalSessionDuration",
                                     duration.InMinutes(), 1,
                                     base::Days(30) / base::Minutes(1), 100);
    }
  }
  prefs->ClearPref(ash::prefs::kAshLoginSessionStartedTime);
  prefs->ClearPref(ash::prefs::kAshLoginSessionStartedIsFirstSession);
}

}  // namespace

void AttemptUserExit() {
  VLOG(1) << "AttemptUserExit";
  ash::BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutStarted", false);

  ReportSessionUMAMetrics();

  PrefService* state = g_browser_process->local_state();
  if (state) {
    ash::BootTimesRecorder::Get()->OnLogoutStarted(state);

    if (SetLocaleForNextStart(state)) {
      TRACE_EVENT0("shutdown", "CommitPendingWrite");
      state->CommitPendingWrite();
    }
  }
  SetSendStopRequestToSessionManager();
  // On ChromeOS, always terminate the browser, regardless of the result of
  // AreAllBrowsersCloseable(). See crbug.com/123107.
  browser_shutdown::NotifyAppTerminating();
  StopSession();
}

void AttemptRelaunch() {
  GetPowerManagerClient()->RequestRestart(power_manager::REQUEST_RESTART_OTHER,
                                          "Chrome relaunch");
}

void AttemptExit() {
  AttemptUserExit();
}

void RelaunchIgnoreUnloadHandlers() {
  AttemptRelaunch();
}

void RelaunchForUpdate() {
  DCHECK(UpdatePending());
  GetUpdateEngineClient()->RebootAfterUpdate();
}

bool UpdatePending() {
  if (!ash::DBusThreadManager::IsInitialized()) {
    return false;
  }

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

void StopSession() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only call this function once.
  static bool notified = false;
  if (notified) {
    return;
  }
  notified = true;

  if (chromeos::PowerPolicyController::IsInitialized()) {
    chromeos::PowerPolicyController::Get()->NotifyChromeIsExiting();
  }

  if (chrome::UpdatePending()) {
    chrome::RelaunchForUpdate();
    return;
  }

  // Signal session manager to stop the session if Chrome has initiated an
  // attempt to do so.
  if (chrome::IsSendingStopRequestToSessionManager() &&
      ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->StopSession(
        login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);
  }
}

void LogMarkAsCleanShutdown() {
// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1
  VLOG(1) << "Set the current session exit type as clean.";
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL -1
}

}  // namespace chrome
