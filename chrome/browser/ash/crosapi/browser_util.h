// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

#include <string>

#include "base/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;
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
namespace crosapi {
namespace browser_util {

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

// Represents different options for how to launch Lacros browser. The values
// shall be consistent with the controlling policy.
enum class LacrosLaunchSwitch {
  // Indicates that the user decides whether to enable Lacros (if allowed) and
  // make it the primary browser.
  kUserChoice = 0,
  // Indicates that Lacros is not allowed to be enabled.
  kLacrosDisallowed = 1,
  // Indicates that Lacros will be enabled (if allowed). Ash browser is the
  // primary browser.
  kSideBySide = 2,
  // Similar to kSideBySide but Lacros is the primary browser.
  kLacrosPrimary = 3,
  // Indicates that Lacros (if allowed) is the only available browser. The value
  // is preserved for future use and is not supported yet.
  kLacrosOnly = 4
};

// Represents the different options available for lacros selection.
enum class LacrosSelection {
  kRootfs = 0,
  kStateful = 1,
  kMaxValue = kStateful,
};

struct ComponentInfo {
  // The client-side component name.
  const char* const name;
  // The CRX "extension" ID for component updater.
  // Must match the Omaha console.
  const char* const crx_id;
};

extern const ComponentInfo kLacrosDogfoodCanaryInfo;
extern const ComponentInfo kLacrosDogfoodDevInfo;
extern const ComponentInfo kLacrosDogfoodBetaInfo;
extern const ComponentInfo kLacrosDogfoodStableInfo;

extern const base::Feature kLacrosAllowOnStableChannel;
extern const base::Feature kLacrosGooglePolicyRollout;

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
bool IsLacrosAllowedToBeEnabled(version_info::Channel channel);

// Returns true if the Lacros feature is enabled for the primary user.
bool IsLacrosEnabled();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosEnabled(version_info::Channel channel);

// Similar to `IsLacrosEnabled()` but does not check if profile migration has
// been completed. This is to be used inside `BrowserDataMigrator`. Unlike
// `IsLacrosEnabled()` it can be called before the primary user profile is
// created.
// TODO(crbug.com/1265800): Refactor `IsLacrosEnabled()` and
// `IsLacrosEnabledForMigration()` to reduce duplicated code.
bool IsLacrosEnabledForMigration(const user_manager::User* user);

// Returns true if |chromeos::features::kLacrosSupport| flag is allowed.
bool IsLacrosSupportFlagAllowed(version_info::Channel channel);

// Forces IsLacrosEnabled() to return true for testing.
void SetLacrosEnabledForTest(bool force_enabled);

// Returns true if Ash browser is enabled. Returns false iff Lacros is
// enabled and is the only browser.
bool IsAshWebBrowserEnabled();

// As above, but takes a channel. Exposed for testing.
bool IsAshWebBrowserEnabled(version_info::Channel channel);

// Returns true if the lacros should be used as a primary browser.
bool IsLacrosPrimaryBrowser();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosPrimaryBrowser(version_info::Channel channel);

// Forces IsLacrosPrimaryBrowser() to return true or false for testing.
// Passing absl::nullopt will reset the state.
void SetLacrosPrimaryBrowserForTest(absl::optional<bool> value);

// Returns true if the lacros can be used as a primary browser
// for the current session.
// Note that IsLacrosPrimaryBrowser may return false, even if this returns
// true, specifically, the feature is disabled by user/policy.
bool IsLacrosPrimaryBrowserAllowed(version_info::Channel channel);

// Returns true if |chromeos::features::kLacrosPrimary| flag is allowed.
bool IsLacrosPrimaryFlagAllowed(version_info::Channel channel);

// Returns true if Lacros is allowed to launch and show a window. This can
// return false if the user is using multi-signin, which is mutually exclusive
// with Lacros.
bool IsLacrosAllowedToLaunch();

// Returns true if chrome apps should be routed through Lacros instead of ash.
bool IsLacrosChromeAppsEnabled();

// Returns true if Lacros is used in the web Kiosk session.
bool IsLacrosEnabledInWebKioskSession();

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
bool IsDataWipeRequired(const std::string& user_id_hash);

// Exposed for testing. The arguments are passed to
// `IsDataWipeRequiredInternal()`.
bool IsDataWipeRequiredForTesting(base::Version data_version,
                                  const base::Version& current_version,
                                  const base::Version& required_version);

// Gets the version of the rootfs lacros-chrome. By reading the metadata json
// file in the correct format.
base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path);

// To be called at primary user login, to cache the policy value for launch
// switch.
void CacheLacrosLaunchSwitch(const policy::PolicyMap& map);

// Returns the ComponentInfo associated with the stateful lacros instance.
ComponentInfo GetLacrosComponentInfo();

// Returns the update channel associated with the given loaded lacros selection.
version_info::Channel GetLacrosSelectionUpdateChannel(
    LacrosSelection selection);

// Exposed for testing. Returns the lacros integration suggested by the policy
// lacros-availability, modified by Finch flags and user flags as appropriate.
LacrosLaunchSwitch GetLaunchSwitchForTesting();

// Clears the cached values for policy data.
void ClearLacrosLaunchSwitchCacheForTest();

bool IsProfileMigrationEnabled(const AccountId& account_id);

// Checks if profile migration has been completed. This is reset if profile
// migration is initiated for example due to lacros data directory being wiped.
bool IsProfileMigrationCompletedForUser(PrefService* local_state,
                                        const std::string& user_id_hash);

// Sets the value of `kProfileMigrationCompletedForUser1Pref` to be true
// for the user identified by `user_id_hash`.
void SetProfileMigrationCompletedForUser(PrefService* local_state,
                                         const std::string& user_id_hash);

// Clears the value of `kProfileMigrationCompletedForUser1Pref` for user
// identified by `user_id_hash`.
void ClearProfileMigrationCompletedForUser(PrefService* local_state,
                                           const std::string& user_id_hash);

// Makes `IsProfileMigrationCompletedForUser()` return true without actually
// updating Local State. It allows tests to avoid marking profile migration as
// completed by getting user_id_hash of the logged in user and updating
// g_browser_process->local_state() etc.
void SetProfileMigrationCompletedForTest(bool is_completed);

// Returns who decided how Lacros should be used - or not: The User, the policy
// or another edge case.
LacrosLaunchSwitchSource GetLacrosLaunchSwitchSource();

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
