// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/startup_utils.h"

#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/onboarding_user_activity_counter.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr char kDisableHIDDetectionScreenForTests[] =
    "oobe.disable_hid_detection_screen_for_tests";

// Saves boolean "Local State" preference and forces its persistence to disk.
void SaveBoolPreferenceForced(const char* pref_name, bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(pref_name, value);
  prefs->CommitPendingWrite();
}

// Saves integer "Local State" preference and forces its persistence to disk.
void SaveIntegerPreferenceForced(const char* pref_name, int value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(pref_name, value);
  prefs->CommitPendingWrite();
}

// Saves string "Local State" preference and forces its persistence to disk.
void SaveStringPreferenceForced(const char* pref_name,
                                const std::string& value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(pref_name, value);
  prefs->CommitPendingWrite();
}

// Returns the path to flag file indicating that both parts of OOBE were
// completed.
// On chrome device, returns /home/chronos/.oobe_completed.
// On Linux desktop, returns {DIR_USER_DATA}/.oobe_completed.
base::FilePath GetOobeCompleteFlagPath() {
  // The constant is defined here so it won't be referenced directly.
  // If you change this path make sure to also change the corresponding rollback
  // constant in Chrome OS: src/platform2/oobe_config/rollback_constants.cc
  const char kOobeCompleteFlagFilePath[] = "/home/chronos/.oobe_completed";

  if (base::SysInfo::IsRunningOnChromeOS()) {
    return base::FilePath(kOobeCompleteFlagFilePath);
  } else {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    return user_data_dir.AppendASCII(".oobe_completed");
  }
}

void CreateOobeCompleteFlagFile() {
  // Create flag file for boot-time init scripts.
  const base::FilePath oobe_complete_flag_path = GetOobeCompleteFlagPath();
  if (!base::PathExists(oobe_complete_flag_path)) {
    FILE* oobe_flag_file = base::OpenFile(oobe_complete_flag_path, "w+b");
    if (oobe_flag_file == nullptr)
      DLOG(WARNING) << oobe_complete_flag_path.value() << " doesn't exist.";
    else
      base::CloseFile(oobe_flag_file);
  }
}

}  // namespace

// static
void StartupUtils::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOobeComplete, false);
  registry->RegisterStringPref(prefs::kOobeScreenPending, "");
  registry->RegisterIntegerPref(::prefs::kDeviceRegistered, -1);
  registry->RegisterBooleanPref(::prefs::kEnrollmentRecoveryRequired, false);
  registry->RegisterStringPref(::prefs::kInitialLocale, "en-US");
  registry->RegisterBooleanPref(kDisableHIDDetectionScreenForTests, false);
  registry->RegisterBooleanPref(prefs::kOobeGuestMetricsEnabled, false);
  registry->RegisterBooleanPref(prefs::kOobeGuestAcceptedTos, false);
  if (switches::IsRevenBranding()) {
    registry->RegisterBooleanPref(prefs::kOobeRevenUpdatedToFlex, false);
  }
  registry->RegisterBooleanPref(prefs::kOobeLocaleChangedOnWelcomeScreen,
                                false);
  registry->RegisterStringPref(prefs::kUrlParameterToAutofillSAMLUsername,
                               std::string());
}

// static
void StartupUtils::RegisterOobeProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kOobeMarketingOptInScreenFinished, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOobeMarketingOptInChoice, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(prefs::kLastLoginInputMethod, std::string());
  registry->RegisterTimePref(prefs::kOobeOnboardingTime, base::Time());
  // The `Arc.PlayStoreLaunchWithinAWeek` metric can only be recorded if
  // `kOobeOnboardingTime` has been set. Therefore,
  // `kArcPlayStoreLaunchMetricCanBeRecorded` should be registered and
  // initialized along with `kOobeOnboardingTime`.
  registry->RegisterBooleanPref(
      arc::prefs::kArcPlayStoreLaunchMetricCanBeRecorded, false);
  if (switches::IsRevenBranding() &&
      features::IsOobeConsolidatedConsentEnabled()) {
    registry->RegisterBooleanPref(prefs::kRevenOobeConsolidatedConsentAccepted,
                                  false);
  }

  if (features::IsOobeChoobeEnabled()) {
    registry->RegisterListPref(prefs::kChoobeSelectedScreens);
  }
  OnboardingUserActivityCounter::RegisterProfilePrefs(registry);
}

// static
bool StartupUtils::IsEulaAccepted() {
  return g_browser_process->local_state()->GetBoolean(::prefs::kEulaAccepted);
}

// static
bool StartupUtils::IsOobeCompleted() {
  return g_browser_process->local_state()->GetBoolean(prefs::kOobeComplete);
}

// static
void StartupUtils::MarkEulaAccepted() {
  SaveBoolPreferenceForced(::prefs::kEulaAccepted, true);
}

// static
void StartupUtils::MarkOobeCompleted() {
  // Forcing the second pref will force this one as well. Even if this one
  // doesn't end up synced it is only going to eat up a couple of bytes with no
  // side-effects.
  g_browser_process->local_state()->ClearPref(prefs::kOobeScreenPending);
  SaveBoolPreferenceForced(prefs::kOobeComplete, true);

  // Successful enrollment implies that recovery is not required.
  SaveBoolPreferenceForced(::prefs::kEnrollmentRecoveryRequired, false);
}

// static
void StartupUtils::SaveOobePendingScreen(const std::string& screen) {
  SaveStringPreferenceForced(prefs::kOobeScreenPending, screen);
}

// static
base::TimeDelta StartupUtils::GetTimeSinceOobeFlagFileCreation() {
  const base::FilePath oobe_complete_flag_path = GetOobeCompleteFlagPath();
  base::File::Info file_info;
  if (base::GetFileInfo(oobe_complete_flag_path, &file_info))
    return base::Time::Now() - file_info.creation_time;
  return base::TimeDelta();
}

// static
bool StartupUtils::IsDeviceRegistered() {
  int value =
      g_browser_process->local_state()->GetInteger(::prefs::kDeviceRegistered);
  if (value > 0) {
    // Recreate flag file in case it was lost.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&CreateOobeCompleteFlagFile));
    return true;
  } else if (value == 0) {
    return false;
  } else {
    // Pref is not set. For compatibility check flag file. It causes blocking
    // IO on UI thread. But it's required for update from old versions.
    base::ScopedAllowBlocking allow_blocking;
    const base::FilePath oobe_complete_flag_path = GetOobeCompleteFlagPath();
    bool file_exists = base::PathExists(oobe_complete_flag_path);
    SaveIntegerPreferenceForced(::prefs::kDeviceRegistered,
                                file_exists ? 1 : 0);
    return file_exists;
  }
}

// static
void StartupUtils::MarkDeviceRegistered(base::OnceClosure done_callback) {
  SaveIntegerPreferenceForced(::prefs::kDeviceRegistered, 1);
  if (done_callback.is_null()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&CreateOobeCompleteFlagFile));
  } else {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&CreateOobeCompleteFlagFile), std::move(done_callback));
  }
}

// static
void StartupUtils::MarkEnrollmentRecoveryRequired() {
  SaveBoolPreferenceForced(::prefs::kEnrollmentRecoveryRequired, true);
}

// static
void StartupUtils::DisableHIDDetectionScreenForTests() {
  SaveBoolPreferenceForced(kDisableHIDDetectionScreenForTests, true);
}

// static
bool StartupUtils::IsHIDDetectionScreenDisabledForTests() {
  return g_browser_process->local_state()->GetBoolean(
      kDisableHIDDetectionScreenForTests);
}

// static
std::string StartupUtils::GetInitialLocale() {
  std::string locale =
      g_browser_process->local_state()->GetString(::prefs::kInitialLocale);
  if (!l10n_util::IsValidLocaleSyntax(locale))
    locale = "en-US";
  return locale;
}

// static
void StartupUtils::SetInitialLocale(const std::string& locale) {
  if (l10n_util::IsValidLocaleSyntax(locale))
    SaveStringPreferenceForced(::prefs::kInitialLocale, locale);
  else
    NOTREACHED();
}

// static
bool StartupUtils::IsDeviceOwned() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return !user_manager::UserManager::Get()->GetUsers().empty() ||
         connector->IsDeviceEnterpriseManaged();
}

}  // namespace ash
