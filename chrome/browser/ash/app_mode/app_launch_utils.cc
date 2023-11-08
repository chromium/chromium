// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_launch_utils.h"

#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "chrome/browser/ash/app_mode/crash_recovery_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// The list of prefs that are reset on the start of each kiosk session.
const char* const kPrefsToReset[] = {"settings.accessibility",  // ChromeVox
                                     "settings.a11y", "ash.docked_magnifier",
                                     "settings.tts"};

// This vector is used in tests when they want to replace `kPrefsToReset` with
// their own list.
std::vector<std::string>* test_prefs_to_reset = nullptr;

}  // namespace

void LaunchAppOrDie(Profile* profile, const KioskAppId& kiosk_app_id) {
  auto* launcher =
      new CrashRecoveryLauncher(CHECK_DEREF(profile), kiosk_app_id);
  launcher->Start(base::BindOnce(
      [](CrashRecoveryLauncher* launcher, const KioskAppId& kiosk_app_id,
         Profile* profile, bool success,
         const absl::optional<std::string>& app_name) {
        delete launcher;
        if (success) {
          CreateKioskSystemSession(kiosk_app_id, profile, app_name);
        } else {
          chrome::AttemptUserExit();
        }
      },
      launcher, kiosk_app_id, profile));
}

void ResetEphemeralKioskPreferences(PrefService* prefs) {
  CHECK(prefs);
  CHECK(user_manager::UserManager::IsInitialized() &&
        user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
  for (size_t pref_id = 0;
       pref_id < (test_prefs_to_reset ? test_prefs_to_reset->size()
                                      : std::size(kPrefsToReset));
       pref_id++) {
    const std::string branch_path = test_prefs_to_reset
                                        ? (*test_prefs_to_reset)[pref_id]
                                        : kPrefsToReset[pref_id];
    prefs->ClearPrefsWithPrefixSilently(branch_path);
  }
}

void SetEphemeralKioskPreferencesListForTesting(
    std::vector<std::string>* prefs) {
  test_prefs_to_reset = prefs;
}

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line,
                              PrefService* local_state) {
  // We shouldn't auto launch kiosk app if a designated command line switch was
  // used.
  //
  // For example, in Tast tests command line switch is used to prevent kiosk
  // autolaunch configured by policy from a previous test. This way ChromeOS
  // will stay on the login screen and Tast can perform policies cleanup.
  if (command_line.HasSwitch(switches::kPreventKioskAutolaunchForTesting)) {
    return false;
  }

  // We shouldn't auto launch kiosk app if powerwash screen should be shown.
  if (local_state->GetBoolean(prefs::kFactoryResetRequested)) {
    return false;
  }

  return command_line.HasSwitch(switches::kLoginManager) &&
         KioskController::Get().GetAutoLaunchApp().has_value() &&
         KioskAppLaunchError::Get() == KioskAppLaunchError::Error::kNone &&
         // IsOobeCompleted() is needed to prevent kiosk session start in case
         // of enterprise rollback, when keeping the enrollment, policy, not
         // clearing TPM, but wiping stateful partition.
         StartupUtils::IsOobeCompleted();
}

void CreateKioskSystemSession(const KioskAppId& kiosk_app_id,
                              Profile* profile,
                              const absl::optional<std::string>& app_name) {
  switch (kiosk_app_id.type) {
    case KioskAppType::kWebApp:
      WebKioskAppManager::Get()->InitKioskSystemSession(profile, kiosk_app_id,
                                                        app_name);
      return;
    case KioskAppType::kChromeApp:
      KioskChromeAppManager::Get()->InitKioskSystemSession(profile,
                                                           kiosk_app_id);
      return;
    case KioskAppType::kArcApp:
      // Do not create a `KioskBrowserSession` for ARC kiosk
      return;
  }
}

}  // namespace ash
