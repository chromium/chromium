// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/component_updater/component_updater_service.h"
#include "components/exo/shell_surface_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

using ash::standalone_browser::IsGoogleInternal;
using ash::standalone_browser::LacrosAvailability;
using user_manager::User;
using user_manager::UserManager;
using version_info::Channel;

namespace crosapi::browser_util {
namespace {

// At session start the value for LacrosAvailability logic is applied and the
// result is stored in this variable which is used after that as a cache.
absl::optional<LacrosAvailability> g_lacros_availability_cache;

// At session start the value for LacrosDataBackwardMigrationMode logic is
// applied and the result is stored in this variable which is used after that as
// a cache.
absl::optional<LacrosDataBackwardMigrationMode>
    g_lacros_data_backward_migration_mode;

// At session start the value for LacrosSelection logic is applied and the
// result is stored in this variable which is used after that as a cache.
absl::optional<LacrosSelectionPolicy> g_lacros_selection_cache;

// The rootfs lacros-chrome metadata keys.
constexpr char kLacrosMetadataContentKey[] = "content";
constexpr char kLacrosMetadataVersionKey[] = "version";

// The conversion map for LacrosDataBackwardMigrationMode policy data. The
// values must match the ones from LacrosDataBackwardMigrationMode.yaml.
constexpr auto kLacrosDataBackwardMigrationModeMap =
    base::MakeFixedFlatMap<base::StringPiece, LacrosDataBackwardMigrationMode>({
        {kLacrosDataBackwardMigrationModePolicyNone,
         LacrosDataBackwardMigrationMode::kNone},
        {kLacrosDataBackwardMigrationModePolicyKeepNone,
         LacrosDataBackwardMigrationMode::kKeepNone},
        {kLacrosDataBackwardMigrationModePolicyKeepSafeData,
         LacrosDataBackwardMigrationMode::kKeepSafeData},
        {kLacrosDataBackwardMigrationModePolicyKeepAll,
         LacrosDataBackwardMigrationMode::kKeepAll},
    });

// The conversion map for LacrosSelection policy data. The values must match
// the ones from LacrosSelection.yaml.
constexpr auto kLacrosSelectionPolicyMap =
    base::MakeFixedFlatMap<base::StringPiece, LacrosSelectionPolicy>({
        {"user_choice", LacrosSelectionPolicy::kUserChoice},
        {"rootfs", LacrosSelectionPolicy::kRootfs},
    });

// Returns primary user's User instance.
const user_manager::User* GetPrimaryUser() {
  // TODO(crbug.com/1185813): TaskManagerImplTest is not ready to run with
  // Lacros enabled.
  // UserManager is not initialized for unit tests by default, unless a fake
  // user manager is constructed.
  if (!UserManager::IsInitialized()) {
    return nullptr;
  }

  // GetPrimaryUser works only after user session is started.
  // May return nullptr, if this is called beforehand.
  return UserManager::Get()->GetPrimaryUser();
}

// Returns the lacros integration suggested by the policy lacros-availability.
// There are several reasons why we might choose to ignore the
// lacros-availability policy.
// 1. The user has set a command line or chrome://flag for
//    kLacrosAvailabilityIgnore.
// 2. The user is a Googler and they are not opted into the
//    kLacrosGooglePolicyRollout trial and they did not have the
//    kLacrosDisallowed policy.
LacrosAvailability GetCachedLacrosAvailability() {
  // TODO(crbug.com/1286340): add DCHECK for production use to avoid the
  // same inconsistency for the future.
  if (g_lacros_availability_cache.has_value())
    return g_lacros_availability_cache.value();
  // It could happen in some browser tests that value is not cached. Return
  // default in that case.
  return LacrosAvailability::kUserChoice;
}

// Returns appropriate LacrosAvailability.
LacrosAvailability GetLacrosAvailability(const user_manager::User* user,
                                         PolicyInitState policy_init_state) {
  switch (policy_init_state) {
    case PolicyInitState::kBeforeInit:
      // If the value is needed before policy initialization, actually,
      // this should be the case where ash process was restarted, and so
      // the calculated value in the previous session should be carried
      // via command line flag.
      // See also LacrosAvailabilityPolicyObserver how it will be propergated.
      return ash::standalone_browser::
          DetermineLacrosAvailabilityFromPolicyValue(
              user,
              base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                  ash::standalone_browser::kLacrosAvailabilityPolicySwitch));

    case PolicyInitState::kAfterInit:
      // If policy initialization is done, the calculated value should be
      // cached.
      return GetCachedLacrosAvailability();
  }
}

// Returns whether the lacros is enabled currently.
bool IsLacrosEnabledInternal(const User* user,
                             LacrosAvailability lacros_availability,
                             bool check_migration_status) {
  if (!ash::standalone_browser::BrowserSupport::IsAllowedInternal(
          user, lacros_availability)) {
    return false;
  }

  DCHECK(user);

  // If profile migration is enabled, the completion of it is necessary for
  // Lacros to be enabled.
  if (check_migration_status && IsProfileMigrationEnabled()) {
    PrefService* local_state = g_browser_process->local_state();
    // Note that local_state can be nullptr in tests.
    if (local_state && !ash::standalone_browser::migrator_util::
                           IsProfileMigrationCompletedForUser(
                               local_state, user->username_hash())) {
      // If migration has not been completed, do not enable lacros.
      return false;
    }
  }

  switch (lacros_availability) {
    case LacrosAvailability::kUserChoice:
      break;
    case LacrosAvailability::kLacrosDisallowed:
      NOTREACHED();  // Guarded by IsLacrosAllowedInternal.
      return false;
    case LacrosAvailability::kLacrosOnly:
      return true;
  }

  if (base::FeatureList::IsEnabled(
          ash::standalone_browser::features::kLacrosOnly)) {
    return true;
  }

  return false;
}

// Returns the string value for the kLacrosStabilitySwitch if present.
absl::optional<std::string> GetLacrosStabilitySwitchValue() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(browser_util::kLacrosStabilitySwitch)
             ? absl::optional<std::string>(cmdline->GetSwitchValueASCII(
                   browser_util::kLacrosStabilitySwitch))
             : absl::nullopt;
}

// Resolves the Lacros stateful channel in the following order:
//   1. From the kLacrosStabilitySwitch command line flag if present.
//   2. From the current ash channel.
Channel GetStatefulLacrosChannel() {
  static const auto kStabilitySwitchToChannelMap =
      base::MakeFixedFlatMap<base::StringPiece, Channel>({
          {browser_util::kLacrosStabilityChannelCanary, Channel::CANARY},
          {browser_util::kLacrosStabilityChannelDev, Channel::DEV},
          {browser_util::kLacrosStabilityChannelBeta, Channel::BETA},
          {browser_util::kLacrosStabilityChannelStable, Channel::STABLE},
      });
  auto stability_switch_value = GetLacrosStabilitySwitchValue();
  return stability_switch_value && base::Contains(kStabilitySwitchToChannelMap,
                                                  *stability_switch_value)
             ? kStabilitySwitchToChannelMap.at(*stability_switch_value)
             : chrome::GetChannel();
}

}  // namespace

// NOTE: If you change the lacros component names, you must also update
// chrome/browser/component_updater/cros_component_installer_chromeos.cc
const ComponentInfo kLacrosDogfoodCanaryInfo = {
    "lacros-dogfood-canary", "hkifppleldbgkdlijbdfkdpedggaopda"};
const ComponentInfo kLacrosDogfoodDevInfo = {
    "lacros-dogfood-dev", "ldobopbhiamakmncndpkeelenhdmgfhk"};
const ComponentInfo kLacrosDogfoodBetaInfo = {
    "lacros-dogfood-beta", "hnfmbeciphpghlfgpjfbcdifbknombnk"};
const ComponentInfo kLacrosDogfoodStableInfo = {
    "lacros-dogfood-stable", "ehpjbaiafkpkmhjocnenjbbhmecnfcjb"};

const Channel kLacrosDefaultChannel = Channel::DEV;

const char kLacrosStabilitySwitch[] = "lacros-stability";
const char kLacrosStabilityChannelCanary[] = "canary";
const char kLacrosStabilityChannelDev[] = "dev";
const char kLacrosStabilityChannelBeta[] = "beta";
const char kLacrosStabilityChannelStable[] = "stable";

const char kLacrosSelectionSwitch[] = "lacros-selection";
const char kLacrosSelectionRootfs[] = "rootfs";
const char kLacrosSelectionStateful[] = "stateful";

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";
// Marks the Chrome version at which profile migration was completed.
const char kDataVerPref[] = "lacros.data_version";
const char kProfileDataBackwardMigrationCompletedForUserPref[] =
    "lacros.profile_data_backward_migration_completed_for_user";
// This pref is to record whether the user clicks "Go to files" button
// on error page of the data migration.
const char kGotoFilesPref[] = "lacros.goto_files";
const char kProfileMigrationCompletionTimeForUserPref[] =
    "lacros.profile_migration_completion_time_for_user";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDataVerPref);
  registry->RegisterDictionaryPref(
      kProfileDataBackwardMigrationCompletedForUserPref);
  registry->RegisterListPref(kGotoFilesPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletionTimeForUserPref);
}

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(crosapi::kLacrosUserDataPath);
  }
  // For developers on Linux desktop, put the directory under the developer's
  // specified --user-data-dir.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  return base_path.Append("lacros");
}

bool IsLacrosAllowedToBeEnabled() {
  if (!ash::standalone_browser::BrowserSupport::IsInitializedForPrimaryUser()) {
    // This function must be called only after user session starts.
    base::debug::DumpWithoutCrashing();
    // Returning false for compatibility.
    // TODO(crbug.com/1494005): replace this logic by CHECK/DCHECK.
    return false;
  }
  return ash::standalone_browser::BrowserSupport::GetForPrimaryUser()
      ->IsAllowed();
}

bool IsLacrosEnabled() {
  return IsLacrosEnabledInternal(GetPrimaryUser(),
                                 GetCachedLacrosAvailability(),
                                 /*check_migration_status=*/true);
}

bool IsLacrosEnabledForMigration(const User* user,
                                 PolicyInitState policy_init_state) {
  return IsLacrosEnabledInternal(user,
                                 GetLacrosAvailability(user, policy_init_state),
                                 /*check_migration_status=*/false);
}

bool IsProfileMigrationEnabled() {
  return IsProfileMigrationEnabledWithUserAndPolicyInitState(
      GetPrimaryUser(), PolicyInitState::kAfterInit);
}

bool IsProfileMigrationEnabledWithUserAndPolicyInitState(
    const user_manager::User* user,
    PolicyInitState policy_init_state) {
  return !base::FeatureList::IsEnabled(ash::standalone_browser::features::
                                           kLacrosProfileMigrationForceOff) &&
         !IsAshWebBrowserEnabledForMigration(user, policy_init_state);
}

bool IsProfileMigrationAvailable() {
  if (!IsProfileMigrationEnabled()) {
    return false;
  }

  // If migration is already completed, it is not necessary to run again.
  if (ash::standalone_browser::migrator_util::
          IsProfileMigrationCompletedForUser(
              UserManager::Get()->GetLocalState(),
              UserManager::Get()->GetPrimaryUser()->username_hash())) {
    return false;
  }

  return true;
}

bool IsAshWebBrowserEnabled() {
  return !IsLacrosEnabled();
}

bool IsAshWebBrowserEnabledForMigration(const user_manager::User* user,
                                        PolicyInitState policy_init_state) {
  return !IsLacrosEnabledForMigration(user, policy_init_state);
}

bool IsLacrosOnlyBrowserAllowed() {
  if (!ash::standalone_browser::BrowserSupport::IsInitializedForPrimaryUser()) {
    // This function must be called only after user session starts.
    base::debug::DumpWithoutCrashing();
    // Returning false for compatibility.
    // TODO(crbug.com/1494005): replace this logic by CHECK/DCHECK.
    return false;
  }
  return ash::standalone_browser::BrowserSupport::GetForPrimaryUser()
      ->IsAllowed();
}

bool IsLacrosOnlyFlagAllowed() {
  return IsLacrosOnlyBrowserAllowed() &&
         (GetCachedLacrosAvailability() == LacrosAvailability::kUserChoice);
}

bool IsLacrosAllowedToLaunch() {
  return UserManager::Get()->GetLoggedInUsers().size() == 1;
}

bool IsLacrosChromeAppsEnabled() {
  return !base::FeatureList::IsEnabled(
             ash::standalone_browser::features::kLacrosDisableChromeApps) &&
         IsLacrosEnabled();
}

bool IsLacrosEnabledInWebKioskSession() {
  return UserManager::Get()->IsLoggedInAsWebKioskApp() && IsLacrosEnabled();
}

bool IsLacrosEnabledInChromeKioskSession() {
  return UserManager::Get()->IsLoggedInAsKioskApp() && IsLacrosEnabled();
}

bool IsLacrosWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return base::StartsWith(*app_id, kLacrosAppIdPrefix);
}

// Assuming the metadata exists, parse the version and check if it contains the
// non-backwards-compatible account_manager change.
// A typical format for metadata is:
// {
//   "content": {
//     "version": "91.0.4469.5"
//   },
//   "metadata_version": 1
// }
bool DoesMetadataSupportNewAccountManager(base::Value* metadata) {
  if (!metadata)
    return false;

  std::string* version_str =
      metadata->GetDict().FindStringByDottedPath("content.version");
  if (!version_str) {
    return false;
  }

  std::vector<std::string> versions_str = base::SplitString(
      *version_str, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (versions_str.size() != 4)
    return false;

  int major_version = 0;
  int minor_version = 0;
  if (!base::StringToInt(versions_str[0], &major_version))
    return false;
  if (!base::StringToInt(versions_str[2], &minor_version))
    return false;

  // TODO(https://crbug.com/1197220): Come up with more appropriate major/minor
  // version numbers.
  return major_version >= 1000 && minor_version >= 0;
}

base::Version GetDataVer(PrefService* local_state,
                         const std::string& user_id_hash) {
  const base::Value::Dict& data_versions = local_state->GetDict(kDataVerPref);
  const std::string* data_version_str = data_versions.FindString(user_id_hash);

  if (!data_version_str)
    return base::Version();

  return base::Version(*data_version_str);
}

void RecordDataVer(PrefService* local_state,
                   const std::string& user_id_hash,
                   const base::Version& version) {
  DCHECK(version.IsValid());
  ScopedDictPrefUpdate update(local_state, kDataVerPref);
  base::Value::Dict& dict = update.Get();
  dict.Set(user_id_hash, version.GetString());
}

base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path) {
  if (!base::PathExists(version_file_path)) {
    LOG(WARNING) << "The rootfs lacros-chrome metadata is missing.";
    return {};
  }

  std::string metadata;
  if (!base::ReadFileToString(version_file_path, &metadata)) {
    PLOG(WARNING) << "Failed to read rootfs lacros-chrome metadata.";
    return {};
  }

  absl::optional<base::Value> v = base::JSONReader::Read(metadata);
  if (!v || !v->is_dict()) {
    LOG(WARNING) << "Failed to parse rootfs lacros-chrome metadata.";
    return {};
  }

  const base::Value::Dict& dict = v->GetDict();
  const base::Value::Dict* content = dict.FindDict(kLacrosMetadataContentKey);
  if (!content) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata content key.";
    return {};
  }

  const std::string* version = content->FindString(kLacrosMetadataVersionKey);
  if (!version) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata version key.";
    return {};
  }

  return base::Version{*version};
}

void CacheLacrosAvailability(const policy::PolicyMap& map) {
  if (g_lacros_availability_cache.has_value()) {
    // Some browser tests might call this multiple times.
    LOG(ERROR) << "Trying to cache LacrosAvailability and the value was set";
    return;
  }

  const base::Value* value =
      map.GetValue(policy::key::kLacrosAvailability, base::Value::Type::STRING);
  g_lacros_availability_cache =
      ash::standalone_browser::DetermineLacrosAvailabilityFromPolicyValue(
          GetPrimaryUser(), value ? value->GetString() : base::StringPiece());
}

void CacheLacrosDataBackwardMigrationMode(const policy::PolicyMap& map) {
  if (g_lacros_data_backward_migration_mode.has_value()) {
    // Some browser tests might call this multiple times.
    LOG(ERROR) << "Trying to cache LacrosDataBackwardMigrationMode and the "
                  "value was set";
    return;
  }

  const base::Value* value = map.GetValue(
      policy::key::kLacrosDataBackwardMigrationMode, base::Value::Type::STRING);
  g_lacros_data_backward_migration_mode = ParseLacrosDataBackwardMigrationMode(
      value ? value->GetString() : base::StringPiece());
}

void CacheLacrosSelection(const policy::PolicyMap& map) {
  if (g_lacros_selection_cache.has_value()) {
    // Some browser tests might call this multiple times.
    LOG(ERROR) << "Trying to cache LacrosSelection and the value was set";
    return;
  }

  // Users can set this switch in chrome://flags to disable the effect of the
  // lacros-selection policy. This should only be allows for googlers.
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(ash::switches::kLacrosSelectionPolicyIgnore) &&
      IsGoogleInternal(UserManager::Get()->GetPrimaryUser())) {
    LOG(WARNING) << "LacrosSelection policy is ignored due to the ignore flag";
    return;
  }

  const base::Value* value =
      map.GetValue(policy::key::kLacrosSelection, base::Value::Type::STRING);
  g_lacros_selection_cache = ParseLacrosSelectionPolicy(
      value ? value->GetString() : base::StringPiece());
}

LacrosSelectionPolicy GetCachedLacrosSelectionPolicy() {
  return g_lacros_selection_cache.value_or(LacrosSelectionPolicy::kUserChoice);
}

absl::optional<LacrosSelection> DetermineLacrosSelection() {
  switch (GetCachedLacrosSelectionPolicy()) {
    case LacrosSelectionPolicy::kRootfs:
      return LacrosSelection::kRootfs;
    case LacrosSelectionPolicy::kUserChoice:
      break;
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  if (!cmdline->HasSwitch(browser_util::kLacrosSelectionSwitch)) {
    return absl::nullopt;
  }

  auto value =
      cmdline->GetSwitchValueASCII(browser_util::kLacrosSelectionSwitch);
  if (value == browser_util::kLacrosSelectionRootfs) {
    return LacrosSelection::kRootfs;
  }
  if (value == browser_util::kLacrosSelectionStateful) {
    return LacrosSelection::kStateful;
  }

  return absl::nullopt;
}

ComponentInfo GetLacrosComponentInfoForChannel(version_info::Channel channel) {
  // We default to the Dev component for UNKNOWN channels.
  static const auto kChannelToComponentInfoMap =
      base::MakeFixedFlatMap<Channel, const ComponentInfo*>({
          {Channel::UNKNOWN, &kLacrosDogfoodDevInfo},
          {Channel::CANARY, &kLacrosDogfoodCanaryInfo},
          {Channel::DEV, &kLacrosDogfoodDevInfo},
          {Channel::BETA, &kLacrosDogfoodBetaInfo},
          {Channel::STABLE, &kLacrosDogfoodStableInfo},
      });
  return *kChannelToComponentInfoMap.at(channel);
}

ComponentInfo GetLacrosComponentInfo() {
  return GetLacrosComponentInfoForChannel(GetStatefulLacrosChannel());
}

Channel GetLacrosSelectionUpdateChannel(LacrosSelection selection) {
  switch (selection) {
    case LacrosSelection::kRootfs:
      // For 'rootfs' Lacros use the same channel as ash/OS. Obtained from
      // the LSB's release track property.
      return chrome::GetChannel();
    case LacrosSelection::kStateful:
      // For 'stateful' Lacros directly check the channel of stateful-lacros
      // that the user is on.
      return GetStatefulLacrosChannel();
    case LacrosSelection::kDeployedLocally:
      // For locally deployed Lacros there is no channel so return unknown.
      return Channel::UNKNOWN;
  }
}

base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service) {
  DCHECK(component_update_service);

  const std::vector<component_updater::ComponentInfo>& components =
      component_update_service->GetComponents();
  const std::string& lacros_component_id = GetLacrosComponentInfo().crx_id;

  LOG(WARNING) << "Looking for lacros-chrome component with id: "
               << lacros_component_id;
  auto it =
      std::find_if(components.begin(), components.end(),
                   [&](const component_updater::ComponentInfo& component_info) {
                     return component_info.id == lacros_component_id;
                   });

  return it == components.end() ? base::Version() : it->version;
}

LacrosAvailability GetCachedLacrosAvailabilityForTesting() {
  return GetCachedLacrosAvailability();
}

// Returns the cached value of the LacrosDataBackwardMigrationMode policy.
LacrosDataBackwardMigrationMode GetCachedLacrosDataBackwardMigrationMode() {
  if (g_lacros_data_backward_migration_mode.has_value())
    return g_lacros_data_backward_migration_mode.value();

  // By default migration should be disabled.
  return LacrosDataBackwardMigrationMode::kNone;
}

void SetLacrosLaunchSwitchSourceForTest(LacrosAvailability test_value) {
  g_lacros_availability_cache = test_value;
}

void ClearLacrosAvailabilityCacheForTest() {
  g_lacros_availability_cache.reset();
}

void ClearLacrosDataBackwardMigrationModeCacheForTest() {
  g_lacros_data_backward_migration_mode.reset();
}

void ClearLacrosSelectionCacheForTest() {
  g_lacros_selection_cache.reset();
}

void RecordMigrationStatus() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    // This can happen in tests.
    CHECK_IS_TEST();
    return;
  }

  const auto* user = GetPrimaryUser();
  if (!user) {
    // The function is intended to be run after primary user is initialized.
    // The function might be run in tests without primary user being set.
    CHECK_IS_TEST();
    return;
  }

  const MigrationStatus status = GetMigrationStatus(local_state, user);

  UMA_HISTOGRAM_ENUMERATION(kLacrosMigrationStatus, status);
}

MigrationStatus GetMigrationStatus(PrefService* local_state,
                                   const user_manager::User* user) {
  if (!crosapi::browser_util::IsLacrosEnabledForMigration(
          user, crosapi::browser_util::PolicyInitState::kAfterInit)) {
    return MigrationStatus::kLacrosNotEnabled;
  }

  absl::optional<ash::standalone_browser::migrator_util::MigrationMode> mode =
      ash::standalone_browser::migrator_util::GetCompletedMigrationMode(
          local_state, user->username_hash());

  if (!mode.has_value()) {
    if (ash::standalone_browser::migrator_util::
            IsMigrationAttemptLimitReachedForUser(local_state,
                                                  user->username_hash())) {
      return MigrationStatus::kMaxAttemptReached;
    }

    return MigrationStatus::kUncompleted;
  }

  switch (mode.value()) {
    case ash::standalone_browser::migrator_util::MigrationMode::kCopy:
      return MigrationStatus::kCopyCompleted;
    case ash::standalone_browser::migrator_util::MigrationMode::kMove:
      return MigrationStatus::kMoveCompleted;
    case ash::standalone_browser::migrator_util::MigrationMode::kSkipForNewUser:
      return MigrationStatus::kSkippedForNewUser;
  }
}

void SetProfileMigrationCompletionTimeForUser(PrefService* local_state,
                                              const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(local_state,
                              kProfileMigrationCompletionTimeForUserPref);
  update->Set(user_id_hash, base::TimeToValue(base::Time::Now()));
}

absl::optional<base::Time> GetProfileMigrationCompletionTimeForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  const auto* pref =
      local_state->FindPreference(kProfileMigrationCompletionTimeForUserPref);

  if (!pref) {
    return absl::nullopt;
  }

  const base::Value* value = pref->GetValue();
  DCHECK(value->is_dict());

  return base::ValueToTime(value->GetDict().Find(user_id_hash));
}

void ClearProfileMigrationCompletionTimeForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(local_state,
                              kProfileMigrationCompletionTimeForUserPref);
  base::Value::Dict& dict = update.Get();
  dict.Remove(user_id_hash);
}

void SetProfileDataBackwardMigrationCompletedForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(
      local_state, kProfileDataBackwardMigrationCompletedForUserPref);
  update->Set(user_id_hash, true);
}

void ClearProfileDataBackwardMigrationCompletedForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(
      local_state, kProfileDataBackwardMigrationCompletedForUserPref);
  base::Value::Dict& dict = update.Get();
  dict.Remove(user_id_hash);
}

LacrosLaunchSwitchSource GetLacrosLaunchSwitchSource() {
  if (!g_lacros_availability_cache.has_value())
    return LacrosLaunchSwitchSource::kUnknown;

  // Note: this check needs to be consistent with the one in
  // DetermineLacrosAvailabilityFromPolicyValue.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kLacrosAvailabilityIgnore) &&
      IsGoogleInternal(UserManager::Get()->GetPrimaryUser())) {
    return LacrosLaunchSwitchSource::kForcedByUser;
  }

  return GetCachedLacrosAvailability() == LacrosAvailability::kUserChoice
             ? LacrosLaunchSwitchSource::kPossiblySetByUser
             : LacrosLaunchSwitchSource::kForcedByPolicy;
}

absl::optional<LacrosSelectionPolicy> ParseLacrosSelectionPolicy(
    base::StringPiece value) {
  auto* it = kLacrosSelectionPolicyMap.find(value);
  if (it != kLacrosSelectionPolicyMap.end())
    return it->second;

  LOG(ERROR) << "Unknown LacrosSelection policy value is passed: " << value;
  return absl::nullopt;
}

absl::optional<LacrosDataBackwardMigrationMode>
ParseLacrosDataBackwardMigrationMode(base::StringPiece value) {
  auto* it = kLacrosDataBackwardMigrationModeMap.find(value);
  if (it != kLacrosDataBackwardMigrationModeMap.end())
    return it->second;

  if (!value.empty()) {
    LOG(ERROR) << "Unknown LacrosDataBackwardMigrationMode policy value: "
               << value;
  }
  return absl::nullopt;
}

base::StringPiece GetLacrosDataBackwardMigrationModeName(
    LacrosDataBackwardMigrationMode value) {
  for (const auto& entry : kLacrosDataBackwardMigrationModeMap) {
    if (entry.second == value)
      return entry.first;
  }

  NOTREACHED();
  return base::StringPiece();
}

base::StringPiece GetLacrosSelectionPolicyName(LacrosSelectionPolicy value) {
  for (const auto& entry : kLacrosSelectionPolicyMap) {
    if (entry.second == value) {
      return entry.first;
    }
  }

  NOTREACHED();
  return base::StringPiece();
}

bool IsAshBrowserSyncEnabled() {
  // Turn off sync from Ash if Lacros is enabled and Ash web browser is
  // disabled.
  if (IsLacrosEnabled() && !IsAshWebBrowserEnabled())
    return false;

  return true;
}

void SetGotoFilesClicked(PrefService* local_state,
                         const std::string& user_id_hash) {
  ScopedListPrefUpdate update(local_state, kGotoFilesPref);
  base::Value::List& list = update.Get();
  base::Value user_id_hash_value(user_id_hash);
  if (!base::Contains(list, user_id_hash_value))
    list.Append(std::move(user_id_hash_value));
}

void ClearGotoFilesClicked(PrefService* local_state,
                           const std::string& user_id_hash) {
  ScopedListPrefUpdate update(local_state, kGotoFilesPref);
  update->EraseValue(base::Value(user_id_hash));
}

bool WasGotoFilesClicked(PrefService* local_state,
                         const std::string& user_id_hash) {
  const base::Value::List& list = local_state->GetList(kGotoFilesPref);
  return base::Contains(list, base::Value(user_id_hash));
}

bool ShouldEnforceAshExtensionKeepList() {
  return IsLacrosEnabled() && base::FeatureList::IsEnabled(
                                  ash::features::kEnforceAshExtensionKeeplist);
}

bool IsAshDevToolEnabled() {
  return IsAshWebBrowserEnabled() ||
         base::FeatureList::IsEnabled(ash::features::kAllowDevtoolsInSystemUI);
}

}  // namespace crosapi::browser_util
