// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_mode/app_launch_utils.h"

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// The list of prefs that are reset on the start of each kiosk session.
const char* const kPrefsToReset[] = {"settings.accessibility",  // ChromeVox
                                     "settings.a11y", "ash.docked_magnifier",
                                     "settings.tts"};

const char* const kOneTimeAutoLaunchWebKioskAccountId =
    "one_time_auto_launch.web_app_account_id";
const char* const kOneTimeAutoLaunchChromeAppAccountId =
    "one_time_auto_launch.chrome_app_account_id";
const char* const kOneTimeAutoLaunchChromeAppId =
    "one_time_auto_launch.chrome_app_id";

// This vector is used in tests when they want to replace `kPrefsToReset` with
// their own list.
std::vector<std::string>* test_prefs_to_reset = nullptr;

AccountId ToAccountId(const std::string* account_id_string) {
  auto account_id = AccountId::Deserialize(CHECK_DEREF(account_id_string));
  CHECK(account_id.has_value());
  return *account_id;
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
                                        : kPrefsToReset[pref_id];
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
         KioskAppLaunchError::Get() == KioskAppLaunchError::Error::kNone &&
         // IsOobeCompleted() is needed to prevent kiosk session start in case
         // of enterprise rollback, when keeping the enrollment, policy, not
         // clearing TPM, but wiping stateful partition.
         StartupUtils::IsOobeCompleted();
}

bool ShouldOneTimeAutoLaunchKioskApp(const base::CommandLine& command_line,
                                     const PrefService& local_state) {
  return command_line.HasSwitch(switches::kLoginManager) &&
         GetOneTimeAutoLaunchKioskAppId(local_state).has_value();
}

std::optional<KioskAppId> GetOneTimeAutoLaunchKioskAppId(
    const PrefService& local_state) {
  const base::Value::Dict& dict =
      local_state.GetDict(KioskChromeAppManager::kKioskDictionaryName);

  if (dict.contains(kOneTimeAutoLaunchWebKioskAccountId)) {
    return KioskAppId::ForWebApp(
        ToAccountId(dict.FindString(kOneTimeAutoLaunchWebKioskAccountId)));
  } else if (dict.contains(kOneTimeAutoLaunchChromeAppAccountId)) {
    return KioskAppId::ForChromeApp(
        CHECK_DEREF(dict.FindString(kOneTimeAutoLaunchChromeAppId)),
        ToAccountId(dict.FindString(kOneTimeAutoLaunchChromeAppAccountId)));
  }

  return std::nullopt;
}

void ClearOneTimeAutoLaunchKioskAppId(PrefService& local_state) {
  ScopedDictPrefUpdate dict_update(&local_state,
                                   KioskChromeAppManager::kKioskDictionaryName);

  dict_update->Remove(kOneTimeAutoLaunchWebKioskAccountId);
  dict_update->Remove(kOneTimeAutoLaunchChromeAppAccountId);
  dict_update->Remove(kOneTimeAutoLaunchChromeAppId);
  local_state.CommitPendingWrite();
}

KioskAppId ExtractOneTimeAutoLaunchKioskAppId(PrefService& local_state) {
  auto result = GetOneTimeAutoLaunchKioskAppId(local_state);
  ClearOneTimeAutoLaunchKioskAppId(local_state);
  return result.value();
}

void SetOneTimeAutoLaunchKioskAppId(PrefService& local_state,
                                    const KioskAppId& kiosk_app_id) {
  ScopedDictPrefUpdate dict_update(&local_state,
                                   KioskChromeAppManager::kKioskDictionaryName);

  switch (kiosk_app_id.type) {
    case KioskAppType::kChromeApp:
      dict_update->Set(kOneTimeAutoLaunchChromeAppAccountId,
                       kiosk_app_id.account_id.Serialize());
      dict_update->Set(kOneTimeAutoLaunchChromeAppId,
                       kiosk_app_id.app_id.value());
      local_state.CommitPendingWrite();
      return;
    case KioskAppType::kWebApp:
      dict_update->Set(kOneTimeAutoLaunchWebKioskAccountId,
                       kiosk_app_id.account_id.Serialize());
      local_state.CommitPendingWrite();
      return;
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/361016399): implement Kiosk IWA autolaunch.
      NOTIMPLEMENTED();
      return;
  }
  NOTREACHED();
}

}  // namespace ash
