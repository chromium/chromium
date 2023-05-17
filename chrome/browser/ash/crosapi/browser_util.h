// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

#include <string>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

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

// Represents the policy indicating which Lacros browser to launch, named
// LacrosSelection. The values shall be consistent with the controlling
// policy. Unlike `LacrosSelection` representing which lacros to select,
// `LacrosSelectionPolicy` represents how to decide which lacros to select.
// Stateful option from `LacrosSelection` is omitted due to a breakage risks in
// case of version skew (e.g. when the latest stateful Lacros available in omaha
// is older than the rootfs Lacros on the device).
enum class LacrosSelectionPolicy {
  // Indicates that the user decides which Lacros browser to launch: rootfs or
  // stateful.
  kUserChoice = 0,
  // Indicates that rootfs Lacros will always be launched.
  kRootfs = 1,
};

// Represents the different options available for lacros selection.
enum class LacrosSelection {
  kRootfs = 0,
  kStateful = 1,
  kDeployedLocally = 2,
  kMaxValue = kDeployedLocally,
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

// Specifies the mode of migration. The values correspond to `MigratorDelegate`
// either being `CopyMigrator` or `MoveMigrator`.
enum class MigrationMode {
  kCopy = 0,  // Migrate using `CopyMigrator`.
  kMove = 1,  // Migrate using `MoveMigrator`.
};

// Specifies the mode Lacros is currently running.
// This enum is different from LacrosAvailability in the way that
// it describes the mode Lacros is running at a given point in time
// which can be influenced by multiple factors such as flags,
// while LacrosAvailability describes the policy that dictates how
// Lacros is supposed to be launched.
enum class LacrosMode {
  // Indicates that Lacros is disabled. Ash is the only browser.
  kDisabled = 0,
  // Indicates that Lacros is enabled but Ash browser is the
  // primary browser.
  kSideBySide = 1,
  // Indicates that Lacros is the primary browser. Ash is also available.
  kPrimary = 2,
  // Indicates that Lacros is the only available browser.
  kOnly = 3,
};

extern const ComponentInfo kLacrosDogfoodCanaryInfo;
extern const ComponentInfo kLacrosDogfoodDevInfo;
extern const ComponentInfo kLacrosDogfoodBetaInfo;
extern const ComponentInfo kLacrosDogfoodStableInfo;

BASE_DECLARE_FEATURE(kLacrosForSupervisedUsers);

// The default update channel to leverage for Lacros when the channel is
// unknown.
extern const version_info::Channel kLacrosDefaultChannel;

// A command-line switch that can also be set from chrome://flags for selecting
// the channel for Lacros updates.
extern const char kLacrosStabilitySwitch[];
extern const char kLacrosStabilityChannelCanary[];
extern const char kLacrosStabilityChannelDev[];
extern const char kLacrosStabilityChannelBeta[];
extern const char kLacrosStabilityChannelStable[];

// A command-line switch that can also be set from chrome://flags that chooses
// which selection of Lacros to use.
extern const char kLacrosSelectionSwitch[];
extern const char kLacrosSelectionRootfs[];
extern const char kLacrosSelectionStateful[];

// A command-line switch that is converted and set via the feature flag.
extern const char kLacrosAvailabilityPolicyInternalName[];
extern const char kLacrosAvailabilityPolicySwitch[];
extern const char kLacrosAvailabilityPolicyUserChoice[];
extern const char kLacrosAvailabilityPolicyLacrosDisabled[];
extern const char kLacrosAvailabilityPolicySideBySide[];
extern const char kLacrosAvailabilityPolicyLacrosPrimary[];
extern const char kLacrosAvailabilityPolicyLacrosOnly[];

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

// A boolean preference that records whether the user data dir has been cleared.
// We intentionally number this as we anticipate we might need to clear the user
// data dir multiple times. This preference tracks the breaking change
// introduced by account_manager in M91/M92 timeframe.
extern const char kClearUserDataDir1Pref[];

// A dictionary local state pref that records the last data version of
// lacros-chrome.
extern const char kDataVerPref[];

// Lacros' user data is backward compatible up until this version.
extern const char kRequiredDataVersion[];

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

// Represents whether the function is being called before the Policy is
// initialized or not.
enum class PolicyInitState {
  kBeforeInit,
  kAfterInit,
};

// Similar to `IsLacrosEnabled()` but does not check if profile migration has
// been completed. This is to be used inside `BrowserDataMigrator`. Unlike
// `IsLacrosEnabled()` it can be called before the primary user profile is
// created.
// TODO(crbug.com/1265800): Refactor `IsLacrosEnabled()` and
// `IsLacrosEnabledForMigration()` to reduce duplicated code.
bool IsLacrosEnabledForMigration(const user_manager::User* user,
                                 PolicyInitState policy_init_state);

// Returns true if `ash::features::kLacrosSupport` flag is allowed.
bool IsLacrosSupportFlagAllowed();

// Returns true if Ash browser is enabled. Returns false iff Lacros is
// enabled and is the only browser.
bool IsAshWebBrowserEnabled();

// Similar to `IsAshWebBrowserEnabled()` but it is calleable even before primary
// profile and policy are initialized.
// TODO(crbug.com/1265800): Refactor to reduce code duplication with
// `IsAshWebBrowserEnabled()`.
bool IsAshWebBrowserEnabledForMigration(const user_manager::User* user,
                                        PolicyInitState policy_init_state);

// Returns true if the lacros should be used as a primary browser.
bool IsLacrosPrimaryBrowser();

// Similar to `IsLacrosPrimaryBrowser()` but is calleable even before primary
// profile and policy are initialized.
// TODO(crbug.com/1265800): Refactor to reduce code duplication with
// `IsLacrosPrimaryBrowser()`.
bool IsLacrosPrimaryBrowserForMigration(const user_manager::User* user,
                                        PolicyInitState policy_init_state);

// Returns the current mode Lacros is running.
// See LacrosMode definition for a full list of modes.
LacrosMode GetLacrosMode();

// Returns true if the lacros can be used as a primary browser
// for the current session.
// Note that IsLacrosPrimaryBrowser may return false, even if this returns
// true, specifically, the feature is disabled by user/policy.
bool IsLacrosPrimaryBrowserAllowed();

// Similar to `IsLacrosPrimaryBrowserAllowed()` but is calleable even before
// primary profile and policy are initialized.
// TODO(crbug.com/1265800): Refactor to reduce code duplication with
// `IsLacrosPrimaryBrowserAllowed()`.
bool IsLacrosPrimaryBrowserAllowedForMigration(
    const user_manager::User* user,
    ash::standalone_browser::LacrosAvailability lacros_availability);

// Returns true if `ash::features::kLacrosPrimary` flag is allowed.
bool IsLacrosPrimaryFlagAllowed();

// Returns true if the lacros can be used as a only browser
// for the current session.
bool IsLacrosOnlyBrowserAllowed();

// Returns true if `ash::features::kLacrosOnly` flag is allowed.
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

// Reads `kDataVerPref` and gets corresponding data version for `user_id_hash`.
// If no such version is registered yet, returns `Version` that is invalid.
// Should only be called on UI thread since it reads from `LocalState`.
base::Version GetDataVer(PrefService* local_state,
                         const std::string& user_id_hash);

// Records data version for `user_id_hash` in `LocalState`. Should only be
// called on UI thread since it reads from `LocalState`.
void RecordDataVer(PrefService* local_state,
                   const std::string& user_id_hash,
                   const base::Version& version);

// Checks if lacros' data directory needs to be wiped for backward incompatible
// data.
bool IsDataWipeRequired(PrefService* local_state,
                        const std::string& user_id_hash);

// Exposed for testing. The arguments are passed to
// `IsDataWipeRequiredInternal()`.
bool IsDataWipeRequiredForTesting(base::Version data_version,
                                  const base::Version& current_version,
                                  const base::Version& required_version);

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

// To be called at primary user login, to cache the policy value for
// LacrosSelection policy. The effective value of the policy does not
// change for the duration of the user session, so cached value shall be
// checked.
void CacheLacrosSelection(const policy::PolicyMap& map);

// Returns cached value of LacrosSelection policy. See `CacheLacrosSelection`
// for details.
LacrosSelectionPolicy GetCachedLacrosSelectionPolicy();

// Returns lacros selection option according to LarcrosSelectionPolicy and
// lacros-selection flag. Returns nullopt if there is no preference.
absl::optional<LacrosSelection> DetermineLacrosSelection();

// Returns the lacros ComponentInfo for a given channel.
ComponentInfo GetLacrosComponentInfoForChannel(version_info::Channel channel);

// Returns the ComponentInfo associated with the stateful lacros instance.
ComponentInfo GetLacrosComponentInfo();

// Returns the update channel associated with the given loaded lacros selection.
version_info::Channel GetLacrosSelectionUpdateChannel(
    LacrosSelection selection);

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

// Clears the cached value for LacrosSelection policy.
void ClearLacrosSelectionCacheForTest();

// Returns true if profile migraiton is enabled. If profile migration is
// enabled, the completion of it is required to enable Lacros.
bool IsProfileMigrationEnabled();

bool IsProfileMigrationEnabledWithUserAndPolicyInitState(
    const user_manager::User* user,
    PolicyInitState policy_init_state);

// Returns true if the profile migration is enabled, but not yet completed.
bool IsProfileMigrationAvailable();

// Returns `MigrationMode::kMove` if LacrosOnly or `kLacrosMoveProfileMigration`
// is enabled and `MigrationMode::kCopy` otherwise.
MigrationMode GetMigrationMode(const user_manager::User* user,
                               PolicyInitState policy_init_state);

// Returns true if either copy or move migration is completed. Used as a wrapper
// over `IsProfileMigrationCompletedForUser()`.
// TODO(crbug.com/1340438): This function is introduced to prevent running
// profile move migration for users who have already completed copy migration.
bool IsCopyOrMoveProfileMigrationCompletedForUser(
    PrefService* local_state,
    const std::string& user_id_hash);

// Checks if profile migration has been completed. This is reset if profile
// migration is initiated for example due to lacros data directory being wiped.
bool IsProfileMigrationCompletedForUser(PrefService* local_state,
                                        const std::string& user_id_hash,
                                        MigrationMode mode);

// Sets the value of `kProfileMigrationCompletedForUserPref` or
// `kProfileMoveMigrationCompletedForUserPref` to be true for the user
// identified by `user_id_hash`, depending on `mode`.
void SetProfileMigrationCompletedForUser(PrefService* local_state,
                                         const std::string& user_id_hash,
                                         MigrationMode mode);

// Clears the values of `kProfileMigrationCompletedForUserPref` and
// `kProfileMoveMigrationCompletedForUserPref` prefs for user identified by
// `user_id_hash`:
void ClearProfileMigrationCompletedForUser(PrefService* local_state,
                                           const std::string& user_id_hash);

// Sets the value of `kProfileMigrationCompletionTimeForUserPref` for the user
// identified by `user_id_hash` to the current time.
void SetProfileMigrationCompletionTimeForUser(PrefService* local_state,
                                              const std::string& user_id_hash);

// Gets the value of `kProfileMigrationCompletionTimeForUserPref` for the user
// identified by `user_id_hash`.
absl::optional<base::Time> GetProfileMigrationCompletionTimeForUser(
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

// Makes `IsProfileMigrationCompletedForUser()` return true without actually
// updating Local State. It allows tests to avoid marking profile migration as
// completed by getting user_id_hash of the logged in user and updating
// g_browser_process->local_state() etc.
void SetProfileMigrationCompletedForTest(bool is_completed);

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

// Parses the string representation of LacrosSelection policy value into the
// enum value. Returns nullopt on unknown value.
absl::optional<LacrosSelectionPolicy> ParseLacrosSelectionPolicy(
    base::StringPiece value);

// Parses the string representation of LacrosDataBackwardMigrationMode policy
// value into the enum value. Returns nullopt on unknown value.
absl::optional<LacrosDataBackwardMigrationMode>
ParseLacrosDataBackwardMigrationMode(base::StringPiece value);

// Returns the policy string representation from the given enum value.
base::StringPiece GetLacrosDataBackwardMigrationModeName(
    LacrosDataBackwardMigrationMode value);

// Returns the LacrosSelection policy value name from the given value. Returned
// StringPiece is guaranteed to never be invalidated.
base::StringPiece GetLacrosSelectionPolicyName(LacrosSelectionPolicy value);

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

// Forces IsLacrosPrimaryBrowser() to return true or false for testing.
// Reset upon destruction of returned |base::AutoReset| object.
base::AutoReset<absl::optional<bool>> SetLacrosPrimaryBrowserForTest(
    absl::optional<bool> value);

}  // namespace crosapi::browser_util

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
