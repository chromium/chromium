// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/startup_utils.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/login_constants.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/onboarding_user_activity_counter.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_metrics_helper.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_token_provider.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_common.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

// Saves boolean "Local State" preference and forces its persistence to disk.
void SaveBoolPreferenceForced(PrefService& local_state,
                              const char* pref_name,
                              bool value) {
  local_state.SetBoolean(pref_name, value);
  local_state.CommitPendingWrite();
}

// Saves integer "Local State" preference and forces its persistence to disk.
void SaveIntegerPreferenceForced(PrefService& local_state,
                                 const char* pref_name,
                                 int value) {
  local_state.SetInteger(pref_name, value);
  local_state.CommitPendingWrite();
}

// Saves string "Local State" preference and forces its persistence to disk.
void SaveStringPreferenceForced(PrefService& local_state,
                                const char* pref_name,
                                const std::string& value) {
  local_state.SetString(pref_name, value);
  local_state.CommitPendingWrite();
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
  registry->RegisterTimePref(prefs::kOobeStartTime, base::Time());
  registry->RegisterIntegerPref(ash::prefs::kDeviceRegistered, -1);
  registry->RegisterBooleanPref(ash::prefs::kEnrollmentRecoveryRequired, false);
  registry->RegisterStringPref(ash::prefs::kInitialLocale, "en-US");
  registry->RegisterBooleanPref(prefs::kOobeGuestMetricsEnabled, false);
  registry->RegisterBooleanPref(prefs::kOobeCriticalUpdateCompleted, false);
  registry->RegisterBooleanPref(prefs::kOobeIsConsumerSegment, false);
  registry->RegisterBooleanPref(prefs::kOobeConsumerUpdateCompleted, false);
  registry->RegisterStringPref(prefs::kOobeScreenAfterConsumerUpdate, "");
  if (switches::IsRevenBranding()) {
    registry->RegisterBooleanPref(prefs::kOobeRevenUpdatedToFlex, false);
  }
  registry->RegisterBooleanPref(prefs::kOobeLocaleChangedOnWelcomeScreen,
                                false);
  registry->RegisterStringPref(prefs::kUrlParameterToAutofillSAMLUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kOobeMetricsClientIdAtOobeStart,
                               std::string());
  registry->RegisterBooleanPref(prefs::kOobeMetricsReportedAsEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kOobeStatsReportingControllerReportedReset, false);

  registry->RegisterBooleanPref(
      ash::quick_start::prefs::kShouldResumeQuickStartAfterReboot, false);
  registry->RegisterDictionaryPref(
      ash::quick_start::prefs::kResumeQuickStartAfterRebootInfo);

  registry->RegisterIntegerPref(
      prefs::kAuthenticationFlowAutoReloadInterval,
      constants::kDefaultAuthenticationFlowAutoReloadInterval);

  registry->RegisterBooleanPref(prefs::kAutoEnrollmentCheckExited, false);
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
  if (switches::IsRevenBranding()) {
    registry->RegisterBooleanPref(prefs::kRevenOobeConsolidatedConsentAccepted,
                                  false);
  }

  if (features::IsOobeChoobeEnabled()) {
    registry->RegisterListPref(prefs::kChoobeSelectedScreens);
    registry->RegisterListPref(prefs::kChoobeCompletedScreens);
  }

  if (drive::util::IsOobeDrivePinningScreenEnabled()) {
    registry->RegisterBooleanPref(prefs::kOobeDrivePinningEnabledDeferred,
                                  false);
  }

  if (features::IsOobePersonalizedOnboardingEnabled()) {
    registry->RegisterListPref(prefs::kOobeCategoriesSelected);
  }

  if (features::IsOobePerksDiscoveryEnabled()) {
    registry->RegisterBooleanPref(prefs::kOobePerksDiscoveryGamgeeShown, false);
  }

  if (features::IsOobeDisplaySizeEnabled()) {
    registry->RegisterDoublePref(prefs::kOobeDisplaySizeFactorDeferred, 1.0);
  }

  OnboardingUserActivityCounter::RegisterProfilePrefs(registry);
}

// static
bool StartupUtils::IsEulaAccepted(const PrefService& local_state) {
  return local_state.GetBoolean(::prefs::kEulaAccepted);
}

// static
bool StartupUtils::IsOobeCompleted(const PrefService& local_state) {
  return local_state.GetBoolean(prefs::kOobeComplete);
}

// static
void StartupUtils::MarkEulaAccepted(PrefService& local_state) {
  SaveBoolPreferenceForced(local_state, ::prefs::kEulaAccepted, true);
}

// static
void StartupUtils::MarkOobeCompleted(PrefService& local_state) {
  // Forcing the second pref will force this one as well. Even if this one
  // doesn't end up synced it is only going to eat up a couple of bytes with no
  // side-effects.
  SaveBoolPreferenceForced(local_state, prefs::kOobeComplete, true);

  // Successful enrollment implies that recovery is not required.
  SaveBoolPreferenceForced(local_state, ash::prefs::kEnrollmentRecoveryRequired,
                           false);

  // If `kOobeComplete` is already true, the `kAutoEnrollmentCheckExited` pref
  // is no longer needed as its purpose is to potentially block OOBE completion.
  local_state.ClearPref(prefs::kAutoEnrollmentCheckExited);
}

// static
void StartupUtils::SaveOobePendingScreen(PrefService& local_state,
                                         const std::string& screen) {
  SaveStringPreferenceForced(local_state, prefs::kOobeScreenPending, screen);
}

// static
void StartupUtils::SaveScreenAfterConsumerUpdate(PrefService& local_state,
                                                 const std::string& screen) {
  SaveStringPreferenceForced(local_state, prefs::kOobeScreenAfterConsumerUpdate,
                             screen);
}

// static
base::Time StartupUtils::GetTimeOfOobeFlagFileCreation() {
  const base::FilePath oobe_complete_flag_path = GetOobeCompleteFlagPath();
  base::File::Info file_info;
  if (base::GetFileInfo(oobe_complete_flag_path, &file_info)) {
    return file_info.creation_time;
  }
  return base::Time();
}

// static
base::TimeDelta StartupUtils::GetTimeSinceOobeFlagFileCreation() {
  base::Time creation_time = GetTimeOfOobeFlagFileCreation();
  return !creation_time.is_null() ? base::Time::Now() - creation_time
                                  : base::TimeDelta();
}

// static
bool StartupUtils::IsDeviceRegistered(PrefService& local_state) {
  int value = local_state.GetInteger(ash::prefs::kDeviceRegistered);
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
    SaveIntegerPreferenceForced(local_state, ash::prefs::kDeviceRegistered,
                                file_exists ? 1 : 0);
    return file_exists;
  }
}

// static
void StartupUtils::MarkDeviceRegistered(PrefService& local_state,
                                        base::OnceClosure done_callback) {
  SaveIntegerPreferenceForced(local_state, ash::prefs::kDeviceRegistered, 1);

  auto* host = LoginDisplayHost::default_host();
  if (host) {
    host->GetOobeMetricsHelper()->RecordDeviceRegistered();
  }

  // clear specific oobe preference from Local state.
  local_state.ClearPref(prefs::kOobeScreenPending);
  local_state.ClearPref(prefs::kOobeIsConsumerSegment);
  local_state.ClearPref(prefs::kOobeConsumerUpdateCompleted);
  local_state.ClearPref(prefs::kOobeScreenAfterConsumerUpdate);
  local_state.ClearPref(prefs::kOobeCriticalUpdateCompleted);

  if (policy::GetEnrollmentToken(OobeConfiguration::Get()).has_value()) {
    VLOG(0) << "Clearing Flex OOBE config after enrollment.";
    OobeConfigurationClient::Get()->DeleteFlexOobeConfig();
  }

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
void StartupUtils::MarkEnrollmentRecoveryRequired(PrefService& local_state) {
  SaveBoolPreferenceForced(local_state, ash::prefs::kEnrollmentRecoveryRequired,
                           true);
}

// static
std::string StartupUtils::GetInitialLocale(const PrefService& local_state) {
  std::string locale = local_state.GetString(ash::prefs::kInitialLocale);
  if (!l10n_util::IsValidLocaleSyntax(locale))
    locale = "en-US";
  return locale;
}

// static
void StartupUtils::SetInitialLocale(PrefService& local_state,
                                    const std::string& locale) {
  if (l10n_util::IsValidLocaleSyntax(locale)) {
    SaveStringPreferenceForced(local_state, ash::prefs::kInitialLocale, locale);
  } else {
    NOTREACHED();
  }
}

// static
bool StartupUtils::IsDeviceOwned() {
  return !user_manager::UserManager::Get()->GetPersistedUsers().empty() ||
         ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

}  // namespace ash
