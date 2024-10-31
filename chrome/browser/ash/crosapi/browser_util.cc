// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <string>
#include <string_view>

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
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
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
#include "components/user_manager/known_user.h"
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
std::optional<LacrosAvailability> g_lacros_availability_cache;

// The rootfs lacros-chrome metadata keys.
constexpr char kLacrosMetadataContentKey[] = "content";
constexpr char kLacrosMetadataVersionKey[] = "version";

// Returns primary user's User instance.
const user_manager::User* GetPrimaryUser() {
  // TODO(crbug.com/40753373): TaskManagerImplTest is not ready to run with
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
  // TODO(crbug.com/40210811): add DCHECK for production use to avoid the
  // same inconsistency for the future.
  if (g_lacros_availability_cache.has_value())
    return g_lacros_availability_cache.value();
  // It could happen in some browser tests that value is not cached. Return
  // default in that case.
  return LacrosAvailability::kUserChoice;
}

}  // namespace

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
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
    // TODO(crbug.com/40286020): replace this logic by CHECK/DCHECK.
    return false;
  }
  return ash::standalone_browser::BrowserSupport::GetForPrimaryUser()
      ->IsAllowed();
}

bool IsLacrosEnabled() {
  return false;
}

bool IsAshWebBrowserEnabled() {
  return true;
}

bool IsLacrosOnlyBrowserAllowed() {
  if (!ash::standalone_browser::BrowserSupport::IsInitializedForPrimaryUser()) {
    // This function must be called only after user session starts.
    base::debug::DumpWithoutCrashing();
    // Returning false for compatibility.
    // TODO(crbug.com/40286020): replace this logic by CHECK/DCHECK.
    return false;
  }
  return ash::standalone_browser::BrowserSupport::GetForPrimaryUser()
      ->IsAllowed();
}

bool IsLacrosOnlyFlagAllowed() {
  return IsLacrosOnlyBrowserAllowed() &&
         // Hide lacros_only flag for guest sessions as they do always start
         // with a fresh and anonymous profile, hence ignoring this setting.
         !UserManager::Get()->IsLoggedInAsGuest() &&
         (GetCachedLacrosAvailability() == LacrosAvailability::kUserChoice);
}

bool IsLacrosChromeAppsEnabled() {
  return false;
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

  // TODO(crbug.com/40176822): Come up with more appropriate major/minor
  // version numbers.
  return major_version >= 1000 && minor_version >= 0;
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

  std::optional<base::Value> v = base::JSONReader::Read(metadata);
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
          GetPrimaryUser(), value ? value->GetString() : std::string_view());
}

LacrosAvailability GetCachedLacrosAvailabilityForTesting() {
  return GetCachedLacrosAvailability();
}

void SetLacrosLaunchSwitchSourceForTest(LacrosAvailability test_value) {
  g_lacros_availability_cache = test_value;
}

void ClearLacrosAvailabilityCacheForTest() {
  g_lacros_availability_cache.reset();
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

}  // namespace crosapi::browser_util
