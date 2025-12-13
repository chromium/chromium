// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_launch_utils.h"

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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

// Refers to `KioskAppLaunchError` to find if the previous Kiosk launch ended
// with an error. The logic is as follows:
//
// 1. The user cancelled the previous launch => block auto-launch.
// 2. The previous launch succeeded => allow auto-launch.
// 3. The previous launch failed with `kChromeAppDeprecated` or
// `kIsolatedAppNotAllowed` => allow auto-launch.
// 4. The previous launch failed with any other error => block auto-launch.
//
// If there was a launch error we generally block auto-launch to prevent a
// launch-error-loop, and Kiosk displays a toast in the login screen. The
// exception is (3) where a decision was made to display these errors in the
// splash screen instead. For this reason auto-launch is allowed only in those
// errors.
bool ShouldAutoLaunchAfterLastError(const PrefService& local_state) {
  if (KioskAppLaunchError::DidUserCancelLaunch(local_state)) {
    return false;
  }
  auto error = KioskAppLaunchError::Get(local_state);
  return error == KioskAppLaunchError::Error::kNone ||
         error == KioskAppLaunchError::Error::kChromeAppDeprecated ||
         error == KioskAppLaunchError::Error::kIsolatedAppNotAllowed;
}

}  // namespace

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
                                        : UNSAFE_TODO(kPrefsToReset[pref_id]);
    prefs->ClearPrefsWithPrefixSilently(branch_path);
  }
}

void SetEphemeralKioskPreferencesListForTesting(
    std::vector<std::string>* prefs) {
  test_prefs_to_reset = prefs;
}

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line,
                              const PrefService& local_state) {
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
  if (local_state.GetBoolean(prefs::kFactoryResetRequested)) {
    return false;
  }

  return command_line.HasSwitch(switches::kLoginManager) &&
         KioskController::Get().GetAutoLaunchApp().has_value() &&
         ShouldAutoLaunchAfterLastError(local_state) &&
         // IsOobeCompleted() is needed to prevent kiosk session start in case
         // of enterprise rollback, when keeping the enrollment, policy, not
         // clearing TPM, but wiping stateful partition.
         StartupUtils::IsOobeCompleted();
}

}  // namespace ash
