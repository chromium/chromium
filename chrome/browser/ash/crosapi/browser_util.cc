// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {
namespace browser_util {
namespace {

bool g_lacros_enabled_for_test = false;

base::Optional<bool> g_lacros_primary_browser_for_test;

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const User* user) {
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED_DEPRECATED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

LacrosLaunchSwitch GetLaunchSwitch() {
  if (!g_browser_process->local_state() ||
      !g_browser_process->local_state()->FindPreference(
          prefs::kLacrosLaunchSwitch)) {
    // Some tests call IsLacrosAllowedToBeEnabled but don't have local_state.
    // Some tests use fake local_state without registered preference.
    return LacrosLaunchSwitch::kUserChoice;
  }

  return static_cast<LacrosLaunchSwitch>(
      g_browser_process->local_state()->GetInteger(prefs::kLacrosLaunchSwitch));
}

// Gets called from IsLacrosAllowedToBeEnabled with primary user or from
// IsLacrosEnabledWithUser with the user that the IsLacrosEnabledWithUser was
// passed.
bool IsLacrosAllowedToBeEnabledWithUser(const User* user, Channel channel) {
  if (g_lacros_enabled_for_test)
    return true;

  if (!IsUserTypeAllowed(user)) {
    return false;
  }

  // TODO(https://crbug.com/1135494): Remove the free ticket for
  // Channel::UNKNOWN after the policy is set on server side for developers.
  if (channel == Channel::UNKNOWN)
    return true;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kUserChoice:
      break;
    case LacrosLaunchSwitch::kLacrosDisallowed:
      return false;
    case LacrosLaunchSwitch::kSideBySide:
    case LacrosLaunchSwitch::kLacrosPrimary:
    case LacrosLaunchSwitch::kLacrosOnly:
      return true;
  }

  // Some unit tests call IsLacrosAllowedToBeEnabled but don't have local_state
  // or use fake one without expected preferences.
  // Only channel check above prevents crash. If chaneel check is removed, there
  // should be check if local_state is nullptr or does not have registered
  // preference.
  DCHECK(g_browser_process->local_state());
  DCHECK(
      g_browser_process->local_state()->FindPreference(prefs::kLacrosAllowed));
  if (!g_browser_process->local_state()->GetBoolean(prefs::kLacrosAllowed)) {
    return false;
  }

  switch (channel) {
    case Channel::UNKNOWN:
    case Channel::CANARY:
    case Channel::DEV:
    case Channel::BETA:
      // Canary/dev/beta builds can use Lacros.
      // Developer builds can use lacros.
      return true;
    case Channel::STABLE:
      return base::FeatureList::IsEnabled(kLacrosAllowOnStableChannel);
  }
}

// Returns the vector containing policy data of the device account. In case of
// an error, returns nullopt.
base::Optional<std::vector<uint8_t>> GetDeviceAccountPolicy(
    EnvironmentProvider* environment_provider) {
  if (!user_manager::UserManager::IsInitialized()) {
    LOG(ERROR) << "User not initialized.";
    return base::nullopt;
  }
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    LOG(ERROR) << "No primary user.";
    return base::nullopt;
  }
  std::string policy_data = environment_provider->GetDeviceAccountPolicy();
  return std::vector<uint8_t>(policy_data.begin(), policy_data.end());
}

struct InterfaceVersionEntry {
  base::Token uuid;
  uint32_t version;
};

template <typename T>
constexpr InterfaceVersionEntry MakeInterfaceVersionEntry() {
  return {T::Uuid_, T::Version_};
}

constexpr InterfaceVersionEntry kInterfaceVersionEntries[] = {
    MakeInterfaceVersionEntry<chromeos::sensors::mojom::SensorHalClient>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Automation>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AccountManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserServiceHost>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CertDatabase>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Clipboard>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Crosapi>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Feedback>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FileManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdleService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KeystoreService>(),
    MakeInterfaceVersionEntry<
        chromeos::machine_learning::mojom::MachineLearningService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MessageCenter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MetricsReporting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Prefs>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ScreenManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SnapshotCapturer>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TaskManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TestController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::UrlHandler>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VideoCaptureDeviceFactory>(),
    MakeInterfaceVersionEntry<device::mojom::HidConnection>(),
    MakeInterfaceVersionEntry<device::mojom::HidManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::MediaControllerManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManager>(),
    MakeInterfaceVersionEntry<media_session::mojom::AudioFocusManagerDebug>(),
};

constexpr bool HasDuplicatedUuid() {
  // We assume the number of entries are small enough so that simple
  // O(N^2) check works.
  const size_t size = base::size(kInterfaceVersionEntries);
  for (size_t i = 0; i < size; ++i) {
    for (size_t j = i + 1; j < size; ++j) {
      if (kInterfaceVersionEntries[i].uuid == kInterfaceVersionEntries[j].uuid)
        return true;
    }
  }
  return false;
}

static_assert(
    crosapi::mojom::Crosapi::Version_ == 21,
    "if you add a new crosapi, please add it to the version map here");
static_assert(!HasDuplicatedUuid(),
              "Each Crosapi Mojom interface should have unique UUID.");

}  // namespace

// When this feature is enabled, Lacros will be available on stable channel.
const base::Feature kLacrosAllowOnStableChannel{
    "LacrosAllowOnStableChannel", base::FEATURE_ENABLED_BY_DEFAULT};

const char kLacrosStabilitySwitch[] = "lacros-stability";
const char kLacrosStabilityLessStable[] = "less-stable";
const char kLacrosStabilityMoreStable[] = "more-stable";

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";
const char kClearUserDataDir1Pref[] = "lacros.clear_user_data_dir_1";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
  registry->RegisterBooleanPref(kClearUserDataDir1Pref,
                                /*default_value=*/false);
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

bool IsLacrosAllowedToBeEnabled(Channel channel) {
  // Allows tests to avoid enabling the flag, constructing a fake user manager,
  // creating g_browser_process->local_state(), etc.
  if (g_lacros_enabled_for_test)
    return true;

  // TODO(crbug.com/1185813): TaskManagerImplTest is not ready to run with
  // Lacros enabled.
  // UserManager is not initialized for unit tests by default, unless a fake
  // user manager is constructed.
  if (!user_manager::UserManager::IsInitialized())
    return false;

  // GetPrimaryUser works only after user session is started.
  const User* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    return false;
  }

  return IsLacrosAllowedToBeEnabledWithUser(user, channel);
}

bool IsLacrosEnabled() {
  return IsLacrosEnabled(chrome::GetChannel());
}

bool IsLacrosEnabled(Channel channel) {
  // Allows tests to avoid enabling the flag, constructing a fake user manager,
  // creating g_browser_process->local_state(), etc.
  if (g_lacros_enabled_for_test)
    return true;

  if (!IsLacrosAllowedToBeEnabled(channel))
    return false;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kUserChoice:
      break;
    case LacrosLaunchSwitch::kLacrosDisallowed:
      DCHECK_EQ(channel, Channel::UNKNOWN);
      return false;
    case LacrosLaunchSwitch::kSideBySide:
    case LacrosLaunchSwitch::kLacrosPrimary:
    case LacrosLaunchSwitch::kLacrosOnly:
      return true;
  }

  return base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

bool IsLacrosEnabledWithUser(const User* user) {
  if (g_lacros_enabled_for_test)
    return true;

  if (!IsLacrosAllowedToBeEnabledWithUser(user, chrome::GetChannel()))
    return false;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kUserChoice:
      break;
    case LacrosLaunchSwitch::kLacrosDisallowed:
      return false;
    case LacrosLaunchSwitch::kSideBySide:
    case LacrosLaunchSwitch::kLacrosPrimary:
    case LacrosLaunchSwitch::kLacrosOnly:
      return true;
  }

  return base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport);
}

bool IsLacrosSupportFlagAllowed(version_info::Channel channel) {
  return IsLacrosAllowedToBeEnabled(channel) &&
         (GetLaunchSwitch() == LacrosLaunchSwitch::kUserChoice);
}

void SetLacrosEnabledForTest(bool force_enabled) {
  g_lacros_enabled_for_test = force_enabled;
}

bool IsAshWebBrowserEnabled() {
  return IsAshWebBrowserEnabled(chrome::GetChannel());
}

bool IsAshWebBrowserEnabled(version_info::Channel channel) {
  // If Lacros is not allowed or is not enabled, Ash browser is always enabled.
  if (!IsLacrosEnabled(channel))
    return true;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kUserChoice:
      break;
    case LacrosLaunchSwitch::kLacrosDisallowed:
    case LacrosLaunchSwitch::kSideBySide:
    case LacrosLaunchSwitch::kLacrosPrimary:
      return true;
    case LacrosLaunchSwitch::kLacrosOnly:
      return false;
  }

  return true;
}

bool IsLacrosPrimaryBrowser() {
  return IsLacrosPrimaryBrowser(chrome::GetChannel());
}

bool IsLacrosPrimaryBrowser(Channel channel) {
  if (g_lacros_primary_browser_for_test.has_value())
    return g_lacros_primary_browser_for_test.value();

  if (!IsLacrosEnabled(channel))
    return false;

  if (!IsLacrosPrimaryBrowserAllowed(channel))
    return false;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kUserChoice:
      break;
    case LacrosLaunchSwitch::kLacrosDisallowed:
      NOTREACHED();
      return false;
    case LacrosLaunchSwitch::kSideBySide:
      return false;
    case LacrosLaunchSwitch::kLacrosPrimary:
    case LacrosLaunchSwitch::kLacrosOnly:
      return true;
  }

  return base::FeatureList::IsEnabled(chromeos::features::kLacrosPrimary);
}

void SetLacrosPrimaryBrowserForTest(base::Optional<bool> value) {
  g_lacros_primary_browser_for_test = value;
}

bool IsLacrosPrimaryBrowserAllowed(Channel channel) {
  if (!IsLacrosAllowedToBeEnabled(channel))
    return false;

  if (GetLaunchSwitch() == LacrosLaunchSwitch::kLacrosDisallowed) {
    DCHECK_EQ(channel, Channel::UNKNOWN);
    return false;
  }

  switch (channel) {
    case Channel::UNKNOWN:
      // Currently, developer build is only a way to enable Lacros as a Primary
      // web browser.
      return true;
    case Channel::CANARY:
    case Channel::DEV:
    case Channel::BETA:
    case Channel::STABLE:
      // Canary/dev/beta/stable builds cannot use Lacros as a primary
      // browser, yet.
      return false;
  }
}

bool IsLacrosPrimaryFlagAllowed(version_info::Channel channel) {
  return IsLacrosPrimaryBrowserAllowed(channel) &&
         (GetLaunchSwitch() == LacrosLaunchSwitch::kUserChoice);
}

bool IsLacrosAllowedToLaunch() {
  return user_manager::UserManager::Get()->GetLoggedInUsers().size() <= 1;
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

  base::Value* version = metadata->FindPath("content.version");
  if (!version || !version->is_string())
    return false;

  std::string version_str = version->GetString();
  std::vector<std::string> versions_str = base::SplitString(
      version_str, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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

base::flat_map<base::Token, uint32_t> GetInterfaceVersions() {
  base::flat_map<base::Token, uint32_t> versions;
  for (const auto& entry : kInterfaceVersionEntries)
    versions.emplace(entry.uuid, entry.version);
  return versions;
}

mojom::BrowserInitParamsPtr GetBrowserInitParams(
    EnvironmentProvider* environment_provider,
    crosapi::mojom::InitialBrowserAction initial_browser_action) {
  auto params = mojom::BrowserInitParams::New();
  params->crosapi_version = crosapi::mojom::Crosapi::Version_;
  params->deprecated_ash_metrics_enabled_has_value = true;
  PrefService* local_state = g_browser_process->local_state();
  params->ash_metrics_enabled =
      local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
  params->ash_metrics_managed =
      local_state->IsManagedPreference(metrics::prefs::kMetricsReportingEnabled)
          ? mojom::MetricsReportingManaged::kManaged
          : mojom::MetricsReportingManaged::kNotManaged;

  params->session_type = environment_provider->GetSessionType();
  params->device_mode = environment_provider->GetDeviceMode();
  params->interface_versions = GetInterfaceVersions();
  params->default_paths = environment_provider->GetDefaultPaths();

  params->device_account_gaia_id =
      environment_provider->GetDeviceAccountGaiaId();
  const base::Optional<account_manager::Account> maybe_device_account =
      environment_provider->GetDeviceAccount();
  if (maybe_device_account) {
    params->device_account =
        account_manager::ToMojoAccount(maybe_device_account.value());
  }

  // TODO(crbug.com/1093194): This should be updated to a new value when
  // the long term fix is made in ash-chrome, atomically.
  params->exo_ime_support =
      crosapi::mojom::ExoImeSupport::kConsumedByImeWorkaround;
  params->cros_user_id_hash = chromeos::ProfileHelper::GetUserIdHashFromProfile(
      ProfileManager::GetPrimaryUserProfile());
  params->device_account_policy = GetDeviceAccountPolicy(environment_provider);
  params->idle_info = IdleServiceAsh::ReadIdleInfoFromSystem();

  params->is_incognito_deprecated =
      initial_browser_action ==
      crosapi::mojom::InitialBrowserAction::kOpenIncognitoWindow;
  params->restore_last_session_deprecated =
      initial_browser_action ==
      crosapi::mojom::InitialBrowserAction::kRestoreLastSession;
  params->initial_browser_action = initial_browser_action;

  return params;
}

base::ScopedFD CreateStartupData(
    EnvironmentProvider* environment_provider,
    crosapi::mojom::InitialBrowserAction initial_browser_action) {
  auto data =
      GetBrowserInitParams(environment_provider, initial_browser_action);
  std::vector<uint8_t> serialized =
      crosapi::mojom::BrowserInitParams::Serialize(&data);

  base::ScopedFD fd(memfd_create("startup_data", 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory backed file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(
          fd.get(), reinterpret_cast<const char*>(serialized.data()),
          serialized.size())) {
    LOG(ERROR) << "Failed to dump the serialized startup data";
    return base::ScopedFD();
  }

  if (lseek(fd.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the FD position";
    return base::ScopedFD();
  }

  return fd;
}

}  // namespace browser_util
}  // namespace crosapi
