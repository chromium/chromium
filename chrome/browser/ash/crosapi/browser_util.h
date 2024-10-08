// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::standalone_browser::migrator_util {
enum class PolicyInitState;
}  // namespace ash::standalone_browser::migrator_util

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
class Value;
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace policy {
class PolicyMap;
}  // namespace policy

namespace user_manager {
class User;
}  // namespace user_manager

// These methods are used by ash-chrome.
namespace crosapi::browser_util {

// Indicates how the decision for the usage of Lacros has been made.
enum class LacrosLaunchSwitchSource {
  // It is unknown yet if and how Lacros will be used.
  kUnknown = 0,
  // Either there were no policies, or the system had a special condition in
  // which the policy got ignored and the user could have set the mode.
  kPossiblySetByUser = 1,
  // The Lacros usage was enforced by the user via #lacros-availability-ignore
  // flag override.
  kForcedByUser = 2,
  // The Lacros usage was enforced using the policy. Note that in this case
  // the policy might still not be used, but it is programmatically overridden
  // and not by the user (e.g. special Googler user case).
  kForcedByPolicy = 3
};

// Represents the values of the LacrosDataBackwardMigrationMode string enum
// policy. It controls what happens when we switch from Lacros back to Ash.
// The values shall be consistent with the policy description.
enum class LacrosDataBackwardMigrationMode {
  // Indicates data backward migration is not performed. The Lacros folder is
  // removed and Ash uses its existing state.
  kNone,
  // Not yet implemented.
  kKeepNone,
  // Not yet implemented.
  kKeepSafeData,
  // All data is migrated back from Lacros to Ash.
  kKeepAll,
};

struct ComponentInfo {
  // The client-side component name.
  const char* const name;
  // The CRX "extension" ID for component updater.
  // Must match the Omaha console.
  const char* const crx_id;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// This enum corresponds to LacrosMigrationStatus* in histograms.xml
// and enums.xml.
enum class MigrationStatus {
  kLacrosNotEnabled = 0,  // Lacros is not enabled.
  kUncompleted = 1,  // Lacros is enabled but migration has not been completed.
  kSkippedForNewUser = 2,  // Migration is skipped for new users.
  kCopyCompleted = 3,      // Migration was completed with `CopyMigratior`.
  kMoveCompleted = 4,      // Migration was completed with `MoveMigrator`.
  kMaxAttemptReached = 5,  // Migration failed or skipped more than
                           // `kMaxMigrationAttemptCount` times.
  kMaxValue = kMaxAttemptReached,
};

// The internal name in about_flags.cc for the `LacrosDataBackwardMigrationMode`
// policy.
inline constexpr const char
    kLacrosDataBackwardMigrationModePolicyInternalName[] =
        "lacros-data-backward-migration-policy";

// The commandline flag name of `LacrosDataBackwardMigrationMode` policy.
// The value should be the policy value as defined just below.
inline constexpr const char kLacrosDataBackwardMigrationModePolicySwitch[] =
    "lacros-data-backward-migration-policy";

// The values for LacrosDataBackwardMigrationMode, they must match the ones
// from LacrosDataBackwardMigrationMode.yaml.
inline constexpr const char kLacrosDataBackwardMigrationModePolicyNone[] =
    "none";
inline constexpr const char kLacrosDataBackwardMigrationModePolicyKeepNone[] =
    "keep_none";
inline constexpr const char
    kLacrosDataBackwardMigrationModePolicyKeepSafeData[] = "keep_safe_data";
inline constexpr const char kLacrosDataBackwardMigrationModePolicyKeepAll[] =
    "keep_all";

// Boolean preference. Whether to launch lacros-chrome on login.
extern const char kLaunchOnLoginPref[];

// Registers user profile preferences related to the lacros-chrome binary.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Registers prefs used via local state PrefService.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if the Lacros feature is allowed to be enabled for primary user.
// This checks user type, chrome channel and enterprise policy.
bool IsLacrosAllowedToBeEnabled();

// Returns true if the Lacros feature is enabled for the primary user.
bool IsLacrosEnabled();

// Similar to `IsLacrosEnabled()` but does not check if profile migration has
// been completed. This is to be used inside `BrowserDataMigrator`. Unlike
// `IsLacrosEnabled()` it can be called before the primary user profile is
// created.
bool IsLacrosEnabledForMigration(
    const user_manager::User* user,
    ash::standalone_browser::migrator_util::PolicyInitState policy_init_state);

// Returns true if Ash browser is enabled. Returns false iff Lacros is
// enabled and is the only browser.
// DEPRECATED. Please use !IsLacrosEnabled().
bool IsAshWebBrowserEnabled();

// Returns true if Lacros can be used as the only browser
// for the current session.
// Note that IsLacrosEnabled may return false, even if this returns
// true, specifically, if the feature is disabled by user/policy.
bool IsLacrosOnlyBrowserAllowed();

// Returns true if `ash::standalone_browser::features::kLacrosOnly` flag is
// allowed to be configured on about:flags page.
bool IsLacrosOnlyFlagAllowed();

// Returns true if Lacros is allowed to launch and show a window. This can
// return false if the user is using multi-signin, which is mutually exclusive
// with Lacros.
bool IsLacrosAllowedToLaunch();

// Returns true if chrome apps should be routed through Lacros instead of ash.
bool IsLacrosChromeAppsEnabled();

// Returns true if Lacros is used in the web Kiosk session.
bool IsLacrosEnabledInWebKioskSession();

// Returns true if Lacros is used in the Chrome App Kiosk session.
bool IsLacrosEnabledInChromeKioskSession();

// Returns true if |window| is an exo ShellSurface window representing a Lacros
// browser.
bool IsLacrosWindow(const aura::Window* window);

// Returns true if |metadata| is appropriately formatted, contains a lacros
// version, and that lacros versions supports the new backwards-incompatible
// account_manager logic.
bool DoesMetadataSupportNewAccountManager(base::Value* metadata);

// Gets the version of the rootfs lacros-chrome. By reading the metadata json
// file in the correct format.
base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path);

// To be called at primary user login, to cache the policy value for lacros
// availability.
void CacheLacrosAvailability(const policy::PolicyMap& map);

// To be called at primary user login, to cache the policy value for the
// LacrosDataBackwardMigrationMode policy.
void CacheLacrosDataBackwardMigrationMode(const policy::PolicyMap& map);

// Returns the lacros ComponentInfo for a given channel.
ComponentInfo GetLacrosComponentInfoForChannel(version_info::Channel channel);

// Returns the ComponentInfo associated with the stateful lacros instance.
ComponentInfo GetLacrosComponentInfo();

// Returns the update channel associated with the given loaded lacros selection.
version_info::Channel GetLacrosSelectionUpdateChannel(
    ash::standalone_browser::LacrosSelection selection);

// Returns the currently installed version of lacros-chrome managed by the
// component updater. Will return an empty / invalid version if no lacros
// component is found.
base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service);

// Exposed for testing. Sets lacros-availability cache for testing.
void SetCachedLacrosAvailabilityForTesting(
    ash::standalone_browser::LacrosAvailability lacros_availability);

// Exposed for testing. Returns the lacros integration suggested by the policy
// lacros-availability, modified by Finch flags and user flags as appropriate.
ash::standalone_browser::LacrosAvailability
GetCachedLacrosAvailabilityForTesting();

// GetCachedLacrosDataBackwardMigrationMode returns the cached value of the
// LacrosDataBackwardMigrationMode policy.
LacrosDataBackwardMigrationMode GetCachedLacrosDataBackwardMigrationMode();

// Clears the cached values for lacros availability policy.
void ClearLacrosAvailabilityCacheForTest();

// Clears the cached value for LacrosDataBackwardMigrationMode.
void ClearLacrosDataBackwardMigrationModeCacheForTest();

// Returns true if profile migraiton is enabled. If profile migration is
// enabled, the completion of it is required to enable Lacros.
bool IsProfileMigrationEnabled(
    const user_manager::User* user,
    ash::standalone_browser::migrator_util::PolicyInitState policy_init_state);

// Returns true if the profile migration is enabled, but not yet completed.
bool IsProfileMigrationAvailable();

// Returns migration status for the primary user. Returns nullopt if the primary
// user is not yet set, which should only happen in tests.
std::optional<MigrationStatus> GetMigrationStatus();
MigrationStatus GetMigrationStatusForUser(PrefService* local_state,
                                          const user_manager::User* user);

// Sets the value of `kProfileMigrationCompletionTimeForUserPref` for the user
// identified by `user_id_hash` to the current time.
void SetProfileMigrationCompletionTimeForUser(PrefService* local_state,
                                              const std::string& user_id_hash);

// Gets the value of `kProfileMigrationCompletionTimeForUserPref` for the user
// identified by `user_id_hash`.
std::optional<base::Time> GetProfileMigrationCompletionTimeForUser(
    PrefService* local_state,
    const std::string& user_id_hash);

// Clears the value of `kProfileMigrationCompletionTimeForUserPref` for the user
// identified by `user_id_hash`.
void ClearProfileMigrationCompletionTimeForUser(
    PrefService* local_state,
    const std::string& user_id_hash);

// Sets the value of `kProfileDataBackwardMigrationCompletedForUserPref` for the
// user identified by `user_id_hash`.
void SetProfileDataBackwardMigrationCompletedForUser(
    PrefService* local_state,
    const std::string& user_id_hash);

// Clears the value of `kProfileDataBackwardMigrationCompletedForUserPref` for
// the user identified by `user_id_hash`.
void ClearProfileDataBackwardMigrationCompletedForUser(
    PrefService* local_state,
    const std::string& user_id_hash);

// Indicate whether sync on Ash should be enabled for browser data. Sync should
// stop syncing browser items from Ash if Lacros is enabled and once browser
// data is migrated to Lacros making it safe to turn off web browser on
// Ash and sync for browser data. Only use after the primary user profile is set
// on UserManager since it calls `IsLacrosEnabled()`.
bool IsAshBrowserSyncEnabled();

// Returns who decided how Lacros should be used - or not: The User, the policy
// or another edge case.
LacrosLaunchSwitchSource GetLacrosLaunchSwitchSource();

// Allow unit tests to simulate that the readout of policies has taken place
// so that later DCHECKs do not fail.
void SetLacrosLaunchSwitchSourceForTest(
    ash::standalone_browser::LacrosAvailability test_value);

// Parses the string representation of LacrosDataBackwardMigrationMode policy
// value into the enum value. Returns nullopt on unknown value.
std::optional<LacrosDataBackwardMigrationMode>
ParseLacrosDataBackwardMigrationMode(std::string_view value);

// Returns the policy string representation from the given enum value.
std::string_view GetLacrosDataBackwardMigrationModeName(
    LacrosDataBackwardMigrationMode value);

// Stores that "Go to files button" on the migration error screen is clicked.
void SetGotoFilesClicked(PrefService* local_state,
                         const std::string& user_id_hash);

// Forgets that "Go to files button" on the migration error screen was clicked.
void ClearGotoFilesClicked(PrefService* local_state,
                           const std::string& user_id_hash);

// Returns true if "Go to files button" on the migration error screen was
// clicked.
bool WasGotoFilesClicked(PrefService* local_state,
                         const std::string& user_id_hash);

// Returns true if ash 1st party extension keep list should be enforced.
bool ShouldEnforceAshExtensionKeepList();

// Indicates whether user can open DevTools in Ash.
bool IsAshDevToolEnabled();

}  // namespace crosapi::browser_util

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
