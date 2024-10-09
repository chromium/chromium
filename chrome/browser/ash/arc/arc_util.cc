// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_util.h"

#include <linux/magic.h>
#include <sys/statfs.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Status of ARC based on device affiliation.
enum class DeviceAffiliationBasedArcStatus {
  // ARC allowed on affiliated device
  kAllowedOnAffiliatedDevice,

  // ARC allowed on unaffiliated device
  kAllowedOnUnaffiliatedDevice,

  // ARC not allowed on unaffiliated device
  kDisallowedOnUnaffiliatedDevice,

  kMaxValue = kDisallowedOnUnaffiliatedDevice,
};

enum class ArcStatus {
  // ARC disallowed for testing
  kDisallowedForTesting,

  // ARC is not available
  kNotAvailable,

  // Non-primary users are not supported in ARC
  kNonPrimaryUsersNotSupported,

  // ARC is disabled by flag for managed user
  kDisabledByFlagForManagedUser,

  // ARC is not allowed for the user
  kDisallowedForUser,

  // Device admin disallowed ARC for unaffiliated user
  kDisallowedByDevicePolicyRestriction,

  // ARC disallowed for unaffiliated users
  kDisallowedByUserPolicyRestriction,

  // ARC allowed on affiliated device
  kAllowedOnAffiliatedDevice,

  // ARC allowed on unaffilated device
  kAllowedOnUnaffiliatedDevice,

  // ARC allowed
  kAllowed,
};

// Contains map of profile to check result of ARC allowed. Contains true if ARC
// allowed check was performed and ARC is allowed. If map does not contain
// a value then this means that check has not been performed yet.
base::LazyInstance<std::map<const Profile*, bool>>::DestructorAtExit
    g_profile_status_check = LAZY_INSTANCE_INITIALIZER;

// Let IsAllowedForProfile() return "false" for any profile.
bool g_disallow_for_testing = false;

// Let IsArcBlockedDueToIncompatibleFileSystem() return the specified value
// during test runs. Doesn't affect public session.
bool g_arc_blocked_due_to_incompatible_filesystem_for_testing = false;

// TODO(kinaba): Temporary workaround for crbug.com/729034.
//
// Some type of accounts don't have user prefs. As a short-term workaround,
// store the compatibility info from them on memory, ignoring the defect that
// it cannot survive browser crash and restart.
//
// This will be removed once the forced migration for ARC Kiosk user is
// implemented. After it's done such types of accounts cannot even sign-in
// with incompatible filesystem. Hence it'll be safe to always regard compatible
// for them then.
base::LazyInstance<std::set<AccountId>>::DestructorAtExit
    g_known_compatible_users = LAZY_INSTANCE_INITIALIZER;

// Returns whether ARC can run on the filesystem mounted at |path|.
// This function should run only on threads where IO operations are allowed.
bool IsArcCompatibleFilesystem(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // If it can be verified it is not on ecryptfs, then it is ok.
  struct statfs statfs_buf;
  if (statfs(path.value().c_str(), &statfs_buf) < 0) {
    VPLOG(1) << "statfs failed";
    return false;
  }
  return statfs_buf.f_type != ECRYPTFS_SUPER_MAGIC;
}

FileSystemCompatibilityState GetFileSystemCompatibilityPref(
    const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (auto pref = known_user.FindIntPath(account_id,
                                         prefs::kArcCompatibleFilesystemChosen);
      pref) {
    return static_cast<FileSystemCompatibilityState>(pref.value());
  }
  VLOG(1) << "arc.compatible_filesystem.chosen not set. Assuming incompatible.";
  return kFileSystemIncompatible;
}

// Similar to GetFileSystemCompatibilityPref, but when the pref is not found,
// it optimistically assumes that the file system is compatible. This is for
// the mitigation of issues like b/327969092 that unexpectedly clears the pref
// during the initial sign-in.
FileSystemCompatibilityState GetFileSystemCompatibilityPrefOptimistic(
    const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (auto pref = known_user.FindIntPath(account_id,
                                         prefs::kArcCompatibleFilesystemChosen);
      pref) {
    return static_cast<FileSystemCompatibilityState>(pref.value());
  }
  VLOG(1) << "arc.compatible_filesystem.chosen not set. Assuming compatible.";
  return kFileSystemCompatible;
}

// Stores the result of IsArcCompatibleFilesystem posted back from the blocking
// task runner.
void StoreCompatibilityCheckResult(const AccountId& account_id,
                                   base::OnceClosure callback,
                                   bool is_compatible) {
  if (is_compatible) {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetIntegerPref(account_id, prefs::kArcCompatibleFilesystemChosen,
                              kFileSystemCompatible);

    // TODO(kinaba): Remove this code for accounts without user prefs.
    // See the comment for |g_known_compatible_users| for the detail.
    if (GetFileSystemCompatibilityPref(account_id) != kFileSystemCompatible) {
      g_known_compatible_users.Get().insert(account_id);
    }
  }
  std::move(callback).Run();
}

bool IsUnaffiliatedArcAllowed() {
  bool arc_allowed;
  ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager) {
    switch (arc_session_manager->state()) {
      case ArcSessionManager::State::NOT_INITIALIZED:
      case ArcSessionManager::State::STOPPED:
        // Apply logic below
        break;
      case ArcSessionManager::State::CHECKING_REQUIREMENTS:
      case ArcSessionManager::State::REMOVING_DATA_DIR:
      case ArcSessionManager::State::CHECKING_DATA_MIGRATION_NECESSITY:
      case ArcSessionManager::State::READY:
      case ArcSessionManager::State::ACTIVE:
      case ArcSessionManager::State::STOPPING:
        // Never forbid unaffiliated ARC while ARC is running
        return true;
    }
  }
  if (ash::CrosSettings::Get()->GetBoolean(ash::kUnaffiliatedArcAllowed,
                                           &arc_allowed)) {
    return arc_allowed;
  }
  // If device policy is not set, allow ARC.
  return true;
}

ArcStatus GetArcStatusForProfile(const Profile* profile,
                                 bool should_report_reason) {
  if (g_disallow_for_testing) {
    VLOG_IF(1, should_report_reason) << "ARC is disallowed for testing.";
    return ArcStatus::kDisallowedForTesting;
  }

  if (!IsArcAvailable()) {
    VLOG_IF(1, should_report_reason) << "ARC is not available.";
    return ArcStatus::kNotAvailable;
  }

  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG_IF(1, should_report_reason)
        << "Non-primary users are not supported in ARC.";
    return ArcStatus::kNonPrimaryUsersNotSupported;
  }

  if (policy_util::IsArcDisabledForEnterprise() &&
      policy_util::IsAccountManaged(profile)) {
    VLOG_IF(1, should_report_reason)
        << "ARC is disabled by flag for managed users.";
    return ArcStatus::kDisabledByFlagForManagedUser;
  }

  // Play Store requires an appropriate application install mechanism. Normal
  // users do this through GAIA, but Kiosk users use a different application
  // install mechanism. ARC is not allowed otherwise (e.g. in public sessions,
  // as described in crbug.com/605545).
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsArcAllowedForUser(user)) {
    VLOG_IF(1, should_report_reason) << "ARC is not allowed for the user.";
    return ArcStatus::kDisallowedForUser;
  }

  if (!user->IsAffiliated() && !IsUnaffiliatedArcAllowed()) {
    VLOG_IF(1, should_report_reason)
        << "Device admin disallowed ARC for unaffiliated users.";
    return ArcStatus::kDisallowedByDevicePolicyRestriction;
  }

  if (!user->IsAffiliated() &&
      !profile->GetPrefs()->GetBoolean(prefs::kUnaffiliatedDeviceArcAllowed) &&
      policy_util::IsAccountManaged(profile)) {
    VLOG_IF(1, should_report_reason) << "ARC disallowed for unaffiliated users";
    return arc::ArcStatus::kDisallowedByUserPolicyRestriction;
  }

  // Please add any condition that disallows ARC above this check.
  const bool is_arc_allowed_on_unaffiliated_devices =
      profile->GetPrefs()->GetBoolean(prefs::kUnaffiliatedDeviceArcAllowed);
  if (user->IsAffiliated() && !is_arc_allowed_on_unaffiliated_devices) {
    return ArcStatus::kAllowedOnAffiliatedDevice;
  }
  if (!user->IsAffiliated() && is_arc_allowed_on_unaffiliated_devices) {
    return ArcStatus::kAllowedOnUnaffiliatedDevice;
  }

  return ArcStatus::kAllowed;
}

bool IsArcAllowedForProfileInternal(const Profile* profile,
                                    bool should_report_reason) {
  const ArcStatus status =
      GetArcStatusForProfile(profile, should_report_reason);
  return status == ArcStatus::kAllowed ||
         status == ArcStatus::kAllowedOnAffiliatedDevice ||
         status == ArcStatus::kAllowedOnUnaffiliatedDevice;
}

void ShowContactAdminDialog() {
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_CONTACT_ADMIN_TITLE),
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_CONTACT_ADMIN_CONTEXT));
}

void SharePathIfRequired(ConvertToContentUrlsAndShareCallback callback,
                         const std::vector<GURL>& content_urls,
                         const std::vector<base::FilePath>& paths_to_share) {
  DCHECK(arc::IsArcVmEnabled() || paths_to_share.empty());
  std::vector<base::FilePath> path_list;
  Profile* const profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  for (const auto& path : paths_to_share) {
    if (!guest_os::GuestOsSharePathFactory::GetForProfile(profile)
             ->IsPathShared(kArcVmName, path)) {
      path_list.push_back(path);
    }
  }
  if (path_list.empty()) {
    std::move(callback).Run(content_urls);
    return;
  }

  const auto& vm_info =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)->GetVmInfo(
          kArcVmName);
  if (!vm_info) {
    LOG(WARNING) << "ARCVM not running, cannot share paths";
    std::move(callback).Run(std::vector<GURL>());
    return;
  }
  guest_os::GuestOsSharePathFactory::GetForProfile(profile)->SharePaths(
      kArcVmName, vm_info->seneschal_server_handle(), path_list,
      base::BindOnce(
          [](ConvertToContentUrlsAndShareCallback callback,
             const std::vector<GURL>& content_urls, bool success,
             const std::string& failure_reason) {
            if (success) {
              std::move(callback).Run(content_urls);
            } else {
              LOG(ERROR) << "Error sharing ARC content URLs: "
                         << failure_reason;
              std::move(callback).Run(std::vector<GURL>());
            }
          },
          std::move(callback), content_urls));
}

}  // namespace

void RecordArcStatusBasedOnDeviceAffiliationUMA(Profile* profile) {
  if (!policy_util::IsAccountManaged(profile) ||
      !profile->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    return;
  }

  switch (GetArcStatusForProfile(profile, /*should_report_reason=*/false)) {
    case arc::ArcStatus::kAllowedOnAffiliatedDevice:
      base::UmaHistogramEnumeration(
          "Arc.Provisioning.DeviceAffiliationAction",
          arc::DeviceAffiliationBasedArcStatus::kAllowedOnAffiliatedDevice);
      break;
    case arc::ArcStatus::kAllowedOnUnaffiliatedDevice:
      base::UmaHistogramEnumeration(
          "Arc.Provisioning.DeviceAffiliationAction",
          arc::DeviceAffiliationBasedArcStatus::kAllowedOnUnaffiliatedDevice);
      break;
    case arc::ArcStatus::kDisallowedByUserPolicyRestriction:
      base::UmaHistogramEnumeration("Arc.Provisioning.DeviceAffiliationAction",
                                    arc::DeviceAffiliationBasedArcStatus::
                                        kDisallowedOnUnaffiliatedDevice);
      break;
    default:
      break;
  }
}

bool IsRealUserProfile(const Profile* profile) {
  // Return false for signin, lock screen and incognito profiles.
  return profile && ash::ProfileHelper::IsUserProfile(profile) &&
         !profile->IsOffTheRecord();
}

bool IsArcAllowedForProfile(const Profile* profile) {
  if (!IsRealUserProfile(profile)) {
    return false;
  }

  auto it = g_profile_status_check.Get().find(profile);

  const bool first_check = it == g_profile_status_check.Get().end();
  const bool result =
      IsArcAllowedForProfileInternal(profile, first_check /* report_reason */);

  if (first_check) {
    g_profile_status_check.Get()[profile] = result;
    return result;
  }

  // This is next check. We should be persistent and report the same result.
  if (result != it->second) {
    NOTREACHED_IN_MIGRATION()
        << "ARC allowed was changed for the current user session "
        << "and profile " << profile->GetPath().MaybeAsASCII()
        << ". This may lead to unexpected behavior. ARC allowed is"
        << " forced to " << it->second;
  }
  return it->second;
}

bool IsArcProvisioned(const Profile* profile) {
  return profile && profile->GetPrefs()->HasPrefPath(prefs::kArcSignedIn) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcSignedIn);
}

void ResetArcAllowedCheckForTesting(const Profile* profile) {
  g_profile_status_check.Get().erase(profile);
}

void ClearArcAllowedCheckForTesting() {
  g_profile_status_check.Get().clear();
}

bool IsArcBlockedDueToIncompatibleFileSystem(const Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);

  // Do not block ARC for public accounts as they only have ext4.
  // Without this check it fails to start after browser crash as compatibility
  // info is stored in RAM.
  if (user && user->GetType() == user_manager::UserType::kPublicAccount) {
    return false;
  }

  // Test runs on Linux workstation does not have expected /etc/lsb-release
  // field nor profile creation step. Hence it returns a dummy test value.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return g_arc_blocked_due_to_incompatible_filesystem_for_testing;
  }

  // Conducts the actual check, only when running on a real Chrome OS device.
  return !IsArcCompatibleFileSystemUsedForUser(user);
}

void SetArcBlockedDueToIncompatibleFileSystemForTesting(bool block) {
  g_arc_blocked_due_to_incompatible_filesystem_for_testing = block;
}

bool IsArcCompatibleFileSystemUsedForUser(const user_manager::User* user) {
  // Returns false for profiles not associated with users (like sign-in profile)
  if (!user) {
    return false;
  }

  // ash::UserSessionManager does the actual file system check and stores
  // the result to prefs, so that it survives crash-restart.
  FileSystemCompatibilityState filesystem_compatibility =
      GetFileSystemCompatibilityPrefOptimistic(user->GetAccountId());
  const bool is_filesystem_compatible =
      filesystem_compatibility != kFileSystemIncompatible ||
      g_known_compatible_users.Get().count(user->GetAccountId()) != 0;

  // To run ARC we want to make sure the underlying file system is compatible
  // with ARC.
  if (!is_filesystem_compatible) {
    VLOG(1)
        << "ARC is not supported since the user hasn't migrated to dircrypto.";
    return false;
  }

  return true;
}

void DisallowArcForTesting() {
  g_disallow_for_testing = true;
  g_profile_status_check.Get().clear();
}

bool IsArcPlayStoreEnabledForProfile(const Profile* profile) {
  return IsArcAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

bool IsArcPlayStoreEnabledPreferenceManagedForProfile(const Profile* profile) {
  if (!IsArcAllowedForProfile(profile)) {
    LOG(DFATAL) << "ARC is not allowed for profile";
    return false;
  }
  return profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled);
}

bool SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled) {
  DCHECK(IsArcAllowedForProfile(profile));
  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    if (enabled && !IsArcPlayStoreEnabledForProfile(profile)) {
      LOG(WARNING) << "Attempt to enable disabled by policy ARC.";
      if (ash::switches::IsTabletFormFactor()) {
        VLOG(1) << "Showing contact admin dialog managed user of tablet form "
                   "factor devices.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ShowContactAdminDialog));
      }
      return false;
    }
    VLOG(1) << "Google-Play-Store-enabled pref is managed. Request to "
            << (enabled ? "enable" : "disable") << " Play Store is not stored";
    // Need update ARC session manager manually for managed case in order to
    // keep its state up to date, otherwise it may stuck with enabling
    // request.
    // TODO(khmel): Consider finding the better way handling this.
    ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
    // |arc_session_manager| can be nullptr in unit_tests.
    if (!arc_session_manager) {
      return false;
    }
    if (enabled) {
      arc_session_manager->AllowActivation(
          ArcSessionManager::AllowActivationReason::kUserEnableAction);
      arc_session_manager->RequestEnable();
    } else {
      arc_session_manager->RequestDisableWithArcDataRemoval();
    }

    return true;
  }
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, enabled);
  return true;
}

bool AreArcAllOptInPreferencesIgnorableForProfile(const Profile* profile) {
  // The preferences are ignorable iff both backup&restore and location services
  // are set by policy.
  const PrefService* prefs = profile->GetPrefs();

  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    // When PH is enabled, location toggle is no longer ARC specific (applies to
    // entire ChromeOS);
    return prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled);
  } else {
    return prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled) &&
           prefs->IsManagedPreference(prefs::kArcLocationServiceEnabled);
  }
}

bool IsArcOobeOptInActive() {
  // No OOBE is expected in case Play Store is not available.
  if (!IsPlayStoreAvailable()) {
    return false;
  }

  // Check if Chrome OS OOBE flow is currently showing.
  // TODO(b/65861628): Redesign the OptIn flow since there is no longer reason
  // to have two different OptIn flows.
  if (!ash::LoginDisplayHost::default_host()) {
    return false;
  }

  // ARC OOBE opt-in will only be active if the user did not complete the
  // onboarding flow yet. The OnboardingCompletedVersion preference will only be
  // saved after the onboarding flow is completed.
  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  user_manager::KnownUser known_user(g_browser_process->local_state());
  return !known_user.GetOnboardingCompletedVersion(account_id).has_value();
}

bool IsArcOobeOptInConfigurationBased() {
  // Ignore if not applicable.
  if (!IsArcOobeOptInActive()) {
    return false;
  }
  // Check that configuration exist.
  auto* oobe_configuration = ash::OobeConfiguration::Get();
  if (!oobe_configuration) {
    return false;
  }
  if (!oobe_configuration->CheckCompleted()) {
    return false;
  }
  // Check configuration value that triggers automatic ARC TOS acceptance.
  auto& configuration = oobe_configuration->configuration();
  auto auto_accept =
      configuration.FindBool(ash::configuration::kArcTosAutoAccept);
  if (!auto_accept) {
    return false;
  }
  return *auto_accept;
}

bool IsArcTermsOfServiceNegotiationNeeded(const Profile* profile) {
  DCHECK(profile);
  // Don't show in session ARC OptIn dialog for managed user.
  // For more info see crbug/950013.
  // Skip to show UI asking users to set up ARC OptIn preferences, if all of
  // them are managed by the admin policy. Note that the ToS agreement is anyway
  // not shown in the case of the managed ARC.
  if (ShouldStartArcSilentlyForManagedProfile(profile) &&
      !ShouldShowOptInForTesting()) {
    VLOG(1) << "Skip ARC Terms of Service negotiation for managed user. "
            << "Don't record B&R and GLS if admin leave it as user to decide "
            << "and user skips the opt-in dialog.";
    return false;
  }

  // If it is marked that the Terms of service is accepted already,
  // just skip the negotiation with user, and start Android management
  // check directly.
  // This happens, e.g., when a user accepted the Terms of service on Opt-in
  // flow, but logged out before ARC sign in procedure was done. Then, logs
  // in again.
  if (profile->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
    VLOG(1) << "The user already accepts ARC Terms of Service.";
    return false;
  }

  return true;
}

bool IsArcTermsOfServiceOobeNegotiationNeeded() {
  if (!IsPlayStoreAvailable()) {
    VLOG(1) << "Skip ARC Terms of Service screen because Play Store is not "
               "available on the device.";
    return false;
  }

  // Demo mode setup flow runs before user is created, therefore this condition
  // needs to be checked before any user related ones.
  if (IsArcDemoModeSetupFlow()) {
    return true;
  }

  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    VLOG(1) << "Skip ARC Terms of Service screen because user is not "
            << "logged in.";
    return false;
  }

  const Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!IsArcAllowedForProfile(profile)) {
    VLOG(1) << "Skip ARC Terms of Service screen because ARC is not allowed.";
    return false;
  }

  if (profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled) &&
      !profile->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    VLOG(1) << "Skip ARC Terms of Service screen because ARC is disabled.";
    return false;
  }

  if (!IsArcTermsOfServiceNegotiationNeeded(profile)) {
    VLOG(1) << "Skip ARC Terms of Service screen because it is already "
               "accepted or fully controlled by policy.";
    return false;
  }

  return true;
}

bool IsArcStatsReportingEnabled() {
  // Managed guest session users never saw the consent for stats reporting even
  // if the admin forced the pref by a policy.
  if (chromeos::IsManagedGuestSession()) {
    return false;
  }

  bool pref = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kStatsReportingPref, &pref);
  return pref;
}

bool IsArcDemoModeSetupFlow() {
  return ash::DemoSetupController::IsOobeDemoSetupFlowInProgress();
}

void UpdateArcFileSystemCompatibilityPrefIfNeeded(
    const AccountId& account_id,
    const base::FilePath& profile_path,
    base::OnceClosure callback) {
  DCHECK(callback);

  // If ARC is not available, skip the check.
  // This shortcut is just for marginally improving the log-in performance on
  // old devices without ARC. We can always safely remove the following 4 lines
  // without changing any functionality when, say, the code clarity becomes
  // more important in the future.
  if (!IsArcAvailable()) {
    std::move(callback).Run();
    return;
  }

  // If the compatibility has been already confirmed, skip the check.
  if (GetFileSystemCompatibilityPref(account_id) != kFileSystemIncompatible) {
    std::move(callback).Run();
    return;
  }

  // Otherwise, check the underlying filesystem.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&IsArcCompatibleFilesystem, profile_path),
      base::BindOnce(&StoreCompatibilityCheckResult, account_id,
                     std::move(callback)));
}

ArcManagementTransition GetManagementTransition(const Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->GetPrefs());

  const ArcManagementTransition management_transition =
      static_cast<ArcManagementTransition>(
          profile->GetPrefs()->GetInteger(prefs::kArcManagementTransition));
  const bool is_unmanaged_to_managed_enabled =
      base::FeatureList::IsEnabled(kEnableUnmanagedToManagedTransitionFeature);
  if (management_transition == ArcManagementTransition::UNMANAGED_TO_MANAGED &&
      !is_unmanaged_to_managed_enabled) {
    return ArcManagementTransition::NO_TRANSITION;
  }
  return management_transition;
}

bool IsPlayStoreAvailable() {
  if (ShouldArcAlwaysStartWithNoPlayStore()) {
    return false;
  }

  if (!IsRobotOrOfflineDemoAccountMode()) {
    return true;
  }

  // Demo Mode is the only public session scenario that can launch Play.
  if (!ash::DemoSession::IsDeviceInDemoMode()) {
    return false;
  }

  return ash::features::ShouldShowPlayStoreInDemoMode();
}

bool ShouldStartArcSilentlyForManagedProfile(const Profile* profile) {
  return IsArcPlayStoreEnabledPreferenceManagedForProfile(profile) &&
         (AreArcAllOptInPreferencesIgnorableForProfile(profile) ||
          !IsArcOobeOptInActive());
}

aura::Window* GetArcWindow(int32_t task_id) {
  for (auto* window : ChromeShelfController::instance()->GetArcWindows()) {
    if (arc::GetWindowTaskId(window) == task_id) {
      return window;
    }
  }

  return nullptr;
}

std::unique_ptr<content::WebContents> CreateArcCustomTabWebContents(
    Profile* profile,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<content::SiteInstance> site_instance =
      tab_util::GetSiteInstanceForNewTab(profile, url);
  content::WebContents::CreateParams create_params(profile, site_instance);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  // Use the same version number as browser_commands.cc
  // TODO(hashimoto): Get the actual Android version from the container;
  // also for |structured_ua.platform_version| below.
  constexpr char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";
  // Override the user agent to request mobile version web sites.
  const std::string product = embedder_support::GetProductAndVersion();
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = content::BuildUserAgentFromOSAndProduct(
      kOsOverrideForTabletSite, product);

  ua_override.ua_metadata_override =
      embedder_support::GetUserAgentMetadata(g_browser_process->local_state());
  ua_override.ua_metadata_override->platform = "Android";
  ua_override.ua_metadata_override->platform_version = "9";
  ua_override.ua_metadata_override->model = "Chrome tablet";
  ua_override.ua_metadata_override->mobile = false;

  web_contents->SetUserAgentOverride(ua_override,
                                     false /*override_in_new_tabs=*/);

  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.source_site_instance = site_instance;
  load_url_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;
  web_contents->GetController().LoadURLWithParams(load_url_params);

  // Add a flag to remember this tab originated in the ARC context.
  web_contents->SetUserData(
      &arc::ArcWebContentsData::kArcTransitionFlag,
      std::make_unique<arc::ArcWebContentsData>(web_contents.get()));

  return web_contents;
}

std::string GetHistogramNameByUserType(const std::string& base_name,
                                       const Profile* profile) {
  if (profile == nullptr) {
    profile = ProfileManager::GetPrimaryUserProfile();
  }
  if (IsRobotOrOfflineDemoAccountMode()) {
    auto* demo_session = ash::DemoSession::Get();
    if (demo_session && demo_session->started()) {
      return base_name + ".DemoMode";
    }
    return base_name + ".RobotAccount";
  }
  if (profile->IsChild()) {
    return base_name + ".Child";
  }

  return base_name +
         (policy_util::IsAccountManaged(profile) ? ".Managed" : ".Unmanaged");
}

std::string GetHistogramNameByUserTypeForPrimaryProfile(
    const std::string& base_name) {
  return GetHistogramNameByUserType(base_name, /*profile=*/nullptr);
}

void ConvertToContentUrlsAndShare(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsAndShareCallback callback) {
  file_manager::util::ConvertToContentUrls(
      profile, std::move(file_system_urls),
      base::BindOnce(&SharePathIfRequired, std::move(callback)));
}

}  // namespace arc
