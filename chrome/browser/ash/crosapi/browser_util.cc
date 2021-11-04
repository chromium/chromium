// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"
#include "chrome/browser/ash/crosapi/resource_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/holding_space_service.mojom.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using MojoOptionalBool = crosapi::mojom::DeviceSettings::OptionalBool;
using user_manager::User;
using version_info::Channel;

namespace crosapi {
namespace browser_util {
namespace {

bool g_lacros_enabled_for_test = false;

bool g_profile_migration_completed_for_test = false;

absl::optional<bool> g_lacros_primary_browser_for_test;

// At session start the value for LacrosLaunchSwitch logic is applied and the
// result is stored in this value which is used after that as a cache.
absl::optional<LacrosLaunchSwitch> g_lacros_launch_switch_cache;

// The rootfs lacros-chrome metadata keys.
constexpr char kLacrosMetadataContentKey[] = "content";
constexpr char kLacrosMetadataVersionKey[] = "version";

// The conversion map for LacrosAvailability policy data. The values must match
// the ones from policy_templates.json.
const auto policy_value_to_enum =
    base::MakeFixedFlatMap<std::string, LacrosLaunchSwitch>({
        {"user_choice", LacrosLaunchSwitch::kUserChoice},
        {"lacros_disallowed", LacrosLaunchSwitch::kLacrosDisallowed},
        {"side_by_side", LacrosLaunchSwitch::kSideBySide},
        {"lacros_primary", LacrosLaunchSwitch::kLacrosPrimary},
        {"lacros_only", LacrosLaunchSwitch::kLacrosOnly},
    });

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const User* user) {
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

// Returns true if the main profile is associated with a google internal
// account.
bool IsGoogleInternal() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user = user_manager->GetPrimaryUser();
  if (!user)
    return false;
  return gaia::IsGoogleInternalAccountEmail(
      user->GetAccountId().GetUserEmail());
}

// Returns the lacros integration suggested by the policy lacros-availability.
// There are several reasons why we might choose to ignore the
// lacros-availability policy.
// 1. The user has set a command line or chrome://flag for
//    kLacrosAvailabilityIgnore.
// 2. The user is a Googler and they are not opted into the
//    kLacrosGooglePolicyRollout trial and they did not have the
//    kLacrosDisallowed policy.
LacrosLaunchSwitch GetLaunchSwitch() {
  if (g_lacros_launch_switch_cache.has_value())
    return g_lacros_launch_switch_cache.value();
  // It could happen in some browser tests that value is not cached. Return
  // default in that case.
  return LacrosLaunchSwitch::kUserChoice;
}

// Gets called from IsLacrosAllowedToBeEnabled with primary user or from
// IsLacrosEnabledForMigration with the user that the
// IsLacrosEnabledForMigration was passed.
bool IsLacrosAllowedToBeEnabledWithUser(const User* user, Channel channel) {
  if (g_lacros_enabled_for_test)
    return true;

  if (!IsUserTypeAllowed(user)) {
    return false;
  }

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
absl::optional<std::vector<uint8_t>> GetDeviceAccountPolicy(
    EnvironmentProvider* environment_provider) {
  if (!user_manager::UserManager::IsInitialized()) {
    LOG(ERROR) << "User not initialized.";
    return absl::nullopt;
  }
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    LOG(ERROR) << "No primary user.";
    return absl::nullopt;
  }
  std::string policy_data = environment_provider->GetDeviceAccountPolicy();
  return std::vector<uint8_t>(policy_data.begin(), policy_data.end());
}

// Returns the device specific data needed for Lacros.
mojom::DevicePropertiesPtr GetDeviceProperties() {
  mojom::DevicePropertiesPtr result = mojom::DeviceProperties::New();
  result->device_dm_token = "";

  if (ash::DeviceSettingsService::IsInitialized()) {
    const enterprise_management::PolicyData* policy_data =
        ash::DeviceSettingsService::Get()->policy_data();

    if (policy_data && policy_data->has_request_token())
      result->device_dm_token = policy_data->request_token();

    if (policy_data && !policy_data->device_affiliation_ids().empty()) {
      const auto& ids = policy_data->device_affiliation_ids();
      result->device_affiliation_ids = {ids.begin(), ids.end()};
    }
  }

  return result;
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
    MakeInterfaceVersionEntry<chromeos::cdm::mojom::BrowserCdmFactory>(),
    MakeInterfaceVersionEntry<chromeos::sensors::mojom::SensorHalClient>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Authentication>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Automation>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AccountManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppPublisher>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppServiceProxy>(),
    MakeInterfaceVersionEntry<crosapi::mojom::AppWindowTracker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserAppInstanceRegistry>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserServiceHost>(),
    MakeInterfaceVersionEntry<crosapi::mojom::BrowserVersionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::CertDatabase>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Clipboard>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ClipboardHistory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ContentProtection>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Crosapi>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DeviceSettingsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DownloadController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::DriveIntegrationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Feedback>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FieldTrialService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::FileManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ForceInstalledTracker>(),
    MakeInterfaceVersionEntry<crosapi::mojom::GeolocationService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::HoldingSpaceService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdentityManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::IdleService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ImageWriter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KeystoreService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::KioskSessionService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LocalPrinter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::LoginState>(),
    MakeInterfaceVersionEntry<
        chromeos::machine_learning::mojom::MachineLearningService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MessageCenter>(),
    MakeInterfaceVersionEntry<crosapi::mojom::MetricsReporting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NativeThemeService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkingAttributes>(),
    MakeInterfaceVersionEntry<crosapi::mojom::NetworkSettingsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::PolicyService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Power>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Prefs>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Remoting>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ResourceManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::ScreenManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::StructuredMetricsService>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SnapshotCapturer>(),
    MakeInterfaceVersionEntry<crosapi::mojom::SystemDisplay>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TaskManager>(),
    MakeInterfaceVersionEntry<crosapi::mojom::TestController>(),
    MakeInterfaceVersionEntry<crosapi::mojom::Tts>(),
    MakeInterfaceVersionEntry<crosapi::mojom::UrlHandler>(),
    MakeInterfaceVersionEntry<crosapi::mojom::VideoCaptureDeviceFactory>(),
    MakeInterfaceVersionEntry<crosapi::mojom::WebPageInfoFactory>(),
    MakeInterfaceVersionEntry<device::mojom::HidConnection>(),
    MakeInterfaceVersionEntry<device::mojom::HidManager>(),
    MakeInterfaceVersionEntry<
        media::stable::mojom::StableVideoDecoderFactory>(),
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

// Called from `IsDataWipeRequired()` or `IsDataWipeRequiredForTesting()`.
// data_version` is the version of last data wipe. `current_version` is the
// version of ash-chrome. `required_version` is the version that introduces some
// breaking change. `data_version` needs to be greater or equal to
// `required_version`. If `required_version` is newer than `current_version`,
// data wipe is not required.
bool IsDataWipeRequiredInternal(base::Version data_version,
                                const base::Version& current_version,
                                const base::Version& required_version) {
  // `data_version` is invalid if any wipe has not been recorded yet. In
  // such a case, assume that the last data wipe happened significantly long
  // time ago.
  if (!data_version.IsValid())
    data_version = base::Version("0");

  if (current_version < required_version) {
    // If `current_version` is smaller than the `required_version`, that means
    // that the data wipe doesn't need to happen yet.
    return false;
  }

  if (data_version >= required_version) {
    // If `data_version` is greater or equal to `required_version`, this means
    // data wipe has already happened and that user data is compatible with the
    // current lacros.
    return false;
  }

  return true;
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

static_assert(
    crosapi::mojom::Crosapi::Version_ == 58,
    "if you add a new crosapi, please add it to kInterfaceVersionEntries");
static_assert(!HasDuplicatedUuid(),
              "Each Crosapi Mojom interface should have unique UUID.");

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

// When this feature is enabled, Lacros will be available on stable channel.
const base::Feature kLacrosAllowOnStableChannel{
    "LacrosAllowOnStableChannel", base::FEATURE_ENABLED_BY_DEFAULT};

// A kill switch for lacros chrome apps.
const base::Feature kLacrosDisableChromeApps{"LacrosDisableChromeApps",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When this feature is enabled, Lacros is allowed to roll out by policy to
// Googlers.
const base::Feature kLacrosGooglePolicyRollout{
    "LacrosGooglePolicyRollout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable this to turn on profile migration for non-googlers. Currently the
// feature is only limited to googlers only.
const base::Feature kLacrosProfileMigrationForAnyUser{
    "LacrosProfileMigrationForAnyUser", base::FEATURE_DISABLED_BY_DEFAULT};

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
const char kClearUserDataDir1Pref[] = "lacros.clear_user_data_dir_1";
const char kDataVerPref[] = "lacros.data_version";
const char kRequiredDataVersion[] = "92.0.0.0";
const char kProfileMigrationCompletedForUserPref[] =
    "lacros.profile_migration_completed_for_user";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
  registry->RegisterBooleanPref(kClearUserDataDir1Pref,
                                /*default_value=*/false);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDataVerPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletedForUserPref,
                                   base::DictionaryValue());
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

  // If profile migration is enabled for the user, then make profile migration a
  // requirement to enable lacros.
  if (IsProfileMigrationEnabled(
          user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId())) {
    PrefService* local_state = g_browser_process->local_state();
    // Note that local_state can be nullptr in tests.
    if (local_state && !IsProfileMigrationCompletedForUser(
                           local_state, user_manager::UserManager::Get()
                                            ->GetPrimaryUser()
                                            ->username_hash())) {
      // If migration has not been completed, do not enable lacros.
      return false;
    }
  }

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

bool IsProfileMigrationEnabled(const AccountId& account_id) {
  //  Currently we turn on profile migration only for Googlers.
  //  `kLacrosProfileMigrationForAnyUser` can be enabled to allow testing with
  //  non-googler accounts.
  if (gaia::IsGoogleInternalAccountEmail(account_id.GetUserEmail()) ||
      base::FeatureList::IsEnabled(kLacrosProfileMigrationForAnyUser))
    return true;

  return false;
}

bool IsLacrosEnabledForMigration(const User* user) {
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

bool IsLacrosSupportFlagAllowed(Channel channel) {
  return IsLacrosAllowedToBeEnabled(channel) &&
         (GetLaunchSwitch() == LacrosLaunchSwitch::kUserChoice);
}

void SetLacrosEnabledForTest(bool force_enabled) {
  g_lacros_enabled_for_test = force_enabled;
}

bool IsAshWebBrowserEnabled() {
  return IsAshWebBrowserEnabled(chrome::GetChannel());
}

bool IsAshWebBrowserEnabled(Channel channel) {
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

  // Lacros-chrome will always be the primary browser if Lacros is enabled in
  // web Kiosk session.
  if (user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp())
    return true;

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

void SetLacrosPrimaryBrowserForTest(absl::optional<bool> value) {
  g_lacros_primary_browser_for_test = value;
}

bool IsLacrosPrimaryBrowserAllowed(Channel channel) {
  if (!IsLacrosAllowedToBeEnabled(channel))
    return false;

  switch (GetLaunchSwitch()) {
    case LacrosLaunchSwitch::kLacrosDisallowed:
      DCHECK_EQ(channel, Channel::UNKNOWN);
      return false;
    case LacrosLaunchSwitch::kLacrosPrimary:
    case LacrosLaunchSwitch::kLacrosOnly:
      // Forcibly allow to use Lacros as a Primary respecting the policy.
      return true;
    default:
      // Fallback others.
      break;
  }

  return true;
}

bool IsLacrosPrimaryFlagAllowed(Channel channel) {
  return IsLacrosPrimaryBrowserAllowed(channel) &&
         (GetLaunchSwitch() == LacrosLaunchSwitch::kUserChoice);
}

bool IsLacrosAllowedToLaunch() {
  return user_manager::UserManager::Get()->GetLoggedInUsers().size() <= 1;
}

bool IsLacrosChromeAppsEnabled() {
  if (base::FeatureList::IsEnabled(kLacrosDisableChromeApps))
    return false;

  if (!IsLacrosPrimaryBrowser())
    return false;

  return true;
}

bool IsLacrosEnabledInWebKioskSession() {
  return user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp() &&
         base::FeatureList::IsEnabled(features::kWebKioskEnableLacros) &&
         IsLacrosEnabled();
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
    InitialBrowserAction initial_browser_action,
    bool is_keep_alive_enabled) {
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
  params->use_new_account_manager =
      environment_provider->GetUseNewAccountManager();

  params->device_account_gaia_id =
      environment_provider->GetDeviceAccountGaiaId();
  const absl::optional<account_manager::Account> maybe_device_account =
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
  params->native_theme_info = NativeThemeServiceAsh::GetNativeThemeInfo();

  params->is_incognito_deprecated =
      initial_browser_action.action ==
      crosapi::mojom::InitialBrowserAction::kOpenIncognitoWindow;
  params->restore_last_session_deprecated =
      initial_browser_action.action ==
      crosapi::mojom::InitialBrowserAction::kRestoreLastSession;
  params->initial_browser_action = initial_browser_action.action;
  if (initial_browser_action.action ==
      crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls) {
    params->startup_urls = std::move(initial_browser_action.urls);
  }

  params->web_apps_enabled = web_app::IsWebAppsCrosapiEnabled();
  params->standalone_browser_is_primary = IsLacrosPrimaryBrowser();
  params->device_properties = GetDeviceProperties();
  params->device_settings = GetDeviceSettings();
  // |metrics_service| could be nullptr in tests.
  if (auto* metrics_service = g_browser_process->metrics_service()) {
    // Send metrics service client id to Lacros if it's present.
    std::string client_id = metrics_service->GetClientId();
    if (!client_id.empty())
      params->metrics_service_client_id = client_id;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kOndeviceHandwritingSwitch)) {
    const auto handwriting_switch =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ash::switches::kOndeviceHandwritingSwitch);

    // TODO(https://crbug.com/1168978): Query mlservice instead of using
    // hard-coded values.
    if (handwriting_switch == "use_rootfs") {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUseRootfs;
    } else if (handwriting_switch == "use_dlc") {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUseDlc;
    } else {
      params->ondevice_handwriting_support =
          crosapi::mojom::OndeviceHandwritingSupport::kUnsupported;
    }
  }

  // Add any BUILDFLAGs we use to pass our per-platform/ build configuration to
  // lacros for runtime handling instead.
  std::vector<crosapi::mojom::BuildFlag> build_flags;
#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
  build_flags.emplace_back(
      crosapi::mojom::BuildFlag::kEnablePlatformEncryptedHevc);
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  build_flags.emplace_back(crosapi::mojom::BuildFlag::kEnablePlatformHevc);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  build_flags.emplace_back(
      crosapi::mojom::BuildFlag::kUseChromeosProtectedMedia);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
  build_flags.emplace_back(crosapi::mojom::BuildFlag::kUseChromeosProtectedAv1);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
  params->build_flags = std::move(build_flags);

  params->standalone_browser_is_only_browser = !IsAshWebBrowserEnabled();
  params->publish_chrome_apps = browser_util::IsLacrosChromeAppsEnabled();

  // Keep-alive mojom API is now used by the current ash-chrome.
  params->initial_keep_alive =
      is_keep_alive_enabled
          ? crosapi::mojom::BrowserInitParams::InitialKeepAlive::kEnabled
          : crosapi::mojom::BrowserInitParams::InitialKeepAlive::kDisabled;
  return params;
}

InitialBrowserAction::InitialBrowserAction(
    crosapi::mojom::InitialBrowserAction action)
    : action(action) {
  // kOpnWindowWIthUrls should take the argument, so the ctor below should be
  // used.
  DCHECK_NE(action, crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls);
}

InitialBrowserAction::InitialBrowserAction(
    crosapi::mojom::InitialBrowserAction action,
    std::vector<GURL> urls)
    : action(action), urls(std::move(urls)) {
  // Currently, only kOpenWindowWithUrls can take the URLs as its argument.
  DCHECK_EQ(action, crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls);
}

InitialBrowserAction::InitialBrowserAction(InitialBrowserAction&&) = default;
InitialBrowserAction& InitialBrowserAction::operator=(InitialBrowserAction&&) =
    default;

InitialBrowserAction::~InitialBrowserAction() = default;

base::ScopedFD CreateStartupData(EnvironmentProvider* environment_provider,
                                 InitialBrowserAction initial_browser_action,
                                 bool is_keep_alive_enabled) {
  auto data = GetBrowserInitParams(environment_provider,
                                   std::move(initial_browser_action),
                                   is_keep_alive_enabled);
  std::vector<uint8_t> serialized =
      crosapi::mojom::BrowserInitParams::Serialize(&data);

  base::ScopedFD fd(memfd_create("startup_data", 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory backed file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(fd.get(), serialized)) {
    LOG(ERROR) << "Failed to dump the serialized startup data";
    return base::ScopedFD();
  }

  if (lseek(fd.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the FD position";
    return base::ScopedFD();
  }

  return fd;
}

base::Version GetDataVer(PrefService* local_state,
                         const std::string& user_id_hash) {
  const base::DictionaryValue* data_versions =
      local_state->GetDictionary(kDataVerPref);
  const std::string* data_version_str =
      data_versions->FindStringPath(user_id_hash);

  if (!data_version_str)
    return base::Version();

  return base::Version(*data_version_str);
}

void RecordDataVer(PrefService* local_state,
                   const std::string& user_id_hash,
                   const base::Version& version) {
  DCHECK(version.IsValid());
  DictionaryPrefUpdate update(local_state, kDataVerPref);
  base::DictionaryValue* dict = update.Get();
  dict->SetString(user_id_hash, version.GetString());
}

bool IsDataWipeRequired(const std::string& user_id_hash) {
  base::Version data_version =
      GetDataVer(g_browser_process->local_state(), user_id_hash);
  base::Version current_version = version_info::GetVersion();
  base::Version required_version =
      base::Version(base::StringPiece(kRequiredDataVersion));

  return IsDataWipeRequiredInternal(data_version, current_version,
                                    required_version);
}

bool IsDataWipeRequiredForTesting(base::Version data_version,
                                  const base::Version& current_version,
                                  const base::Version& required_version) {
  return IsDataWipeRequiredInternal(data_version, current_version,
                                    required_version);
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

  const base::Value* content = v->FindKey(kLacrosMetadataContentKey);
  if (!content || !content->is_dict()) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata content key.";
    return {};
  }

  const base::Value* version = content->FindKey(kLacrosMetadataVersionKey);
  if (!version || !version->is_string()) {
    LOG(WARNING)
        << "Failed to parse rootfs lacros-chrome metadata version key.";
    return {};
  }

  return base::Version{version->GetString()};
}

bool IsSigninProfileOrBelongsToAffiliatedUser(Profile* profile) {
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return true;

  if (profile->IsOffTheRecord())
    return false;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;
  return user->IsAffiliated();
}

void CacheLacrosLaunchSwitch(const policy::PolicyMap& map) {
  if (g_lacros_launch_switch_cache.has_value()) {
    // Some browser tests might call this multiple times.
    LOG(ERROR) << "Trying to cache LacrosLaunchSwitch and the value was set";
    return;
  }
  // Users can set this switch in chrome://flags to disable the effect of the
  // lacros-availability policy.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kLacrosAvailabilityIgnore)) {
    g_lacros_launch_switch_cache = LacrosLaunchSwitch::kUserChoice;
    return;
  }

  const base::Value* value = map.GetValue(policy::key::kLacrosAvailability);
  if (!value) {
    // Some tests call IsLacrosAllowedToBeEnabled but don't have the value set.
    g_lacros_launch_switch_cache = LacrosLaunchSwitch::kUserChoice;
    return;
  }

  auto* map_entry = policy_value_to_enum.find(value->GetString());
  if (map_entry == policy_value_to_enum.end()) {
    LOG(ERROR) << "Invalid LacrosLaunchSwitch policy value: "
               << value->GetString();
    g_lacros_launch_switch_cache = LacrosLaunchSwitch::kUserChoice;
    return;
  }
  auto result = map_entry->second;
  if (IsGoogleInternal() &&
      !base::FeatureList::IsEnabled(kLacrosGooglePolicyRollout) &&
      result != LacrosLaunchSwitch::kLacrosDisallowed) {
    g_lacros_launch_switch_cache = LacrosLaunchSwitch::kUserChoice;
    return;
  }

  g_lacros_launch_switch_cache = result;
}

ComponentInfo GetLacrosComponentInfo() {
  // We default to the Dev component for UNKNOWN channels.
  static const auto kChannelToComponentInfoMap =
      base::MakeFixedFlatMap<Channel, const ComponentInfo*>({
          {Channel::UNKNOWN, &kLacrosDogfoodDevInfo},
          {Channel::CANARY, &kLacrosDogfoodCanaryInfo},
          {Channel::DEV, &kLacrosDogfoodDevInfo},
          {Channel::BETA, &kLacrosDogfoodBetaInfo},
          {Channel::STABLE, &kLacrosDogfoodStableInfo},
      });
  return *kChannelToComponentInfoMap.at(GetStatefulLacrosChannel());
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
  }
}

// Returns the device policy data needed for Lacros.
mojom::DeviceSettingsPtr GetDeviceSettings() {
  mojom::DeviceSettingsPtr result = mojom::DeviceSettings::New();

  result->attestation_for_content_protection_enabled = MojoOptionalBool::kUnset;
  if (ash::CrosSettings::IsInitialized()) {
    // It's expected that the CrosSettings values are trusted. The only
    // theoretical exception is when device ownership is taken on consumer
    // device. Then there's no settings to be passed to Lacros anyway.
    auto trusted_result =
        ash::CrosSettings::Get()->PrepareTrustedValues(base::DoNothing());
    if (trusted_result == ash::CrosSettingsProvider::TRUSTED) {
      const auto* cros_settings = ash::CrosSettings::Get();
      bool attestation_enabled = false;
      if (cros_settings->GetBoolean(
              ash::kAttestationForContentProtectionEnabled,
              &attestation_enabled)) {
        result->attestation_for_content_protection_enabled =
            attestation_enabled ? MojoOptionalBool::kTrue
                                : MojoOptionalBool::kFalse;
      }

      const base::ListValue* usb_detachable_allow_list;
      if (cros_settings->GetList(ash::kUsbDetachableAllowlist,
                                 &usb_detachable_allow_list)) {
        mojom::UsbDetachableAllowlistPtr allow_list =
            mojom::UsbDetachableAllowlist::New();
        for (const auto& entry : usb_detachable_allow_list->GetList()) {
          mojom::UsbDeviceIdPtr usb_device_id = mojom::UsbDeviceId::New();
          absl::optional<int> vid =
              entry.FindIntKey(ash::kUsbDetachableAllowlistKeyVid);
          if (vid) {
            usb_device_id->has_vendor_id = true;
            usb_device_id->vendor_id = vid.value();
          }
          absl::optional<int> pid =
              entry.FindIntKey(ash::kUsbDetachableAllowlistKeyPid);
          if (pid) {
            usb_device_id->has_product_id = true;
            usb_device_id->product_id = pid.value();
          }
          allow_list->usb_device_ids.push_back(std::move(usb_device_id));
        }
        result->usb_detachable_allow_list = std::move(allow_list);
      }
    } else {
      LOG(WARNING) << "Unexpected crossettings trusted values status: "
                   << trusted_result;
    }
  }

  result->device_system_wide_tracing_enabled = MojoOptionalBool::kUnset;
  auto* local_state = g_browser_process->local_state();
  if (local_state) {
    auto* pref = local_state->FindPreference(
        ash::prefs::kDeviceSystemWideTracingEnabled);
    if (pref && pref->IsManaged()) {
      result->device_system_wide_tracing_enabled =
          pref->GetValue()->GetBool() ? MojoOptionalBool::kTrue
                                      : MojoOptionalBool::kFalse;
    }
  }

  return result;
}

LacrosLaunchSwitch GetLaunchSwitchForTesting() {
  return GetLaunchSwitch();
}

void ClearLacrosLaunchSwitchCacheForTest() {
  g_lacros_launch_switch_cache.reset();
}

bool IsProfileMigrationCompletedForUser(PrefService* local_state,
                                        const std::string& user_id_hash) {
  // Allows tests to avoid marking profile migration as completed by getting
  // user_id_hash of the logged in user and updating
  // g_browser_process->local_state() etc.
  if (g_profile_migration_completed_for_test)
    return true;

  if (base::FeatureList::IsEnabled(
          ash::features::kForceProfileMigrationCompletion)) {
    return true;
  }

  const auto* pref =
      local_state->FindPreference(kProfileMigrationCompletedForUserPref);
  // Return if the pref is not registered. This can happen in browsertests. In
  // such a case, assume that migration was completed.
  if (!pref)
    return true;

  const base::Value* value = pref->GetValue();
  DCHECK(value->is_dict());
  absl::optional<bool> is_completed = value->FindBoolKey(user_id_hash);

  // If migration was skipped or failed, disable lacros.
  return is_completed.value_or(false);
}

void SetProfileMigrationCompletedForUser(PrefService* local_state,
                                         const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state,
                              kProfileMigrationCompletedForUserPref);
  base::DictionaryValue* dict = update.Get();
  dict->SetBoolKey(user_id_hash, true);
}

void ClearProfileMigrationCompletedForUser(PrefService* local_state,
                                           const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state,
                              kProfileMigrationCompletedForUserPref);
  base::DictionaryValue* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

void SetProfileMigrationCompletedForTest(bool is_completed) {
  g_profile_migration_completed_for_test = is_completed;
}

}  // namespace browser_util
}  // namespace crosapi
