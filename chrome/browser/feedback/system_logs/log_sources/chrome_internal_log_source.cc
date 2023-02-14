// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_internals_util.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/ash_interfaces.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/metrics/enrollment_status.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/version/version_loader.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/google/google_update_win.h"
#endif
#include "ui/base/win/hidden_window.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace system_logs {

namespace {

constexpr char kSyncDataKey[] = "about_sync_data";
constexpr char kExtensionsListKey[] = "extensions";
constexpr char kPowerApiListKey[] = "chrome.power extensions";
constexpr char kChromeVersionTag[] = "CHROME VERSION";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kLacrosChromeVersionPrefix[] = "Lacros ";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kAshChromeVersionPrefix[] = "Ash ";
constexpr char kArcPolicyComplianceReportKey[] =
    "CHROMEOS_ARC_POLICY_COMPLIANCE_REPORT";
constexpr char kArcDpcVersionKey[] = "CHROMEOS_ARC_DPC_VERSION";
constexpr char kArcPolicyKey[] = "CHROMEOS_ARC_POLICY";
constexpr char kChromeOsFirmwareVersion[] = "CHROMEOS_FIRMWARE_VERSION";
constexpr char kChromeEnrollmentTag[] = "ENTERPRISE_ENROLLED";
constexpr char kHWIDKey[] = "HWID";
constexpr char kSettingsKey[] = "settings";
constexpr char kLocalStateSettingsResponseKey[] = "Local State: settings";
constexpr char kLTSChromeVersionPrefix[] = "LTS ";
constexpr char kArcStatusKey[] = "CHROMEOS_ARC_STATUS";
constexpr char kMonitorInfoKey[] = "monitor_info";
constexpr char kAccountTypeKey[] = "account_type";
constexpr char kLacrosStatus[] = "lacros_status";
constexpr char kDemoModeConfigKey[] = "demo_mode_config";
constexpr char kOnboardingTime[] = "ONBOARDING_TIME";
constexpr char kFreeDiskSpace[] = "FREE_DISK_SPACE";
constexpr char kTotalDiskSpace[] = "TOTAL_DISK_SPACE";
constexpr char kChronosHomeDirectory[] = "/home/user/chronos";
#else
constexpr char kOsVersionTag[] = "OS VERSION";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
constexpr char kUsbKeyboardDetected[] = "usb_keyboard_detected";
constexpr char kIsEnrolledToDomain[] = "enrolled_to_domain";
constexpr char kInstallerBrandCode[] = "installer_brand_code";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kUpdateErrorCode[] = "update_error_code";
constexpr char kUpdateHresult[] = "update_hresult";
constexpr char kInstallResultCode[] = "install_result_code";
constexpr char kInstallLocation[] = "install_location";
#endif
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
constexpr char kCpuArch[] = "cpu_arch";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetPrimaryAccountTypeString() {
  DCHECK(user_manager::UserManager::Get());
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  // In case we're on the login screen, we won't have a logged in user.
  if (!primary_user)
    return "none";

  switch (primary_user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return "regular";
    case user_manager::USER_TYPE_GUEST:
      return "guest";
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return "public_account";
    case user_manager::USER_TYPE_KIOSK_APP:
      return "kiosk_app";
    case user_manager::USER_TYPE_CHILD:
      return "child";
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return "arc_kiosk_app";
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return "active_directory";
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return "web_kiosk_app";
    case user_manager::NUM_USER_TYPES:
      NOTREACHED();
      break;
  }
  return std::string();
}

std::string GetEnrollmentStatusString() {
  switch (ChromeOSMetricsProvider::GetEnrollmentStatus()) {
    case EnrollmentStatus::kNonManaged:
      return "Not managed";
    case EnrollmentStatus::kManaged:
      return "Managed";
    case EnrollmentStatus::kUnused:
    case EnrollmentStatus::kErrorGettingStatus:
      return "Error retrieving status";
  }
}

std::string GetDisplayInfoString(
    const crosapi::mojom::DisplayUnitInfo& display_info) {
  std::string entry;
  if (!display_info.name.empty())
    base::StringAppendF(&entry, "%s : ", display_info.name.c_str());
  if (!display_info.edid)
    return entry;
  const crosapi::mojom::Edid& edid = *display_info.edid;
  if (!edid.manufacturer_id.empty()) {
    base::StringAppendF(&entry, "Manufacturer: %s - ",
                        edid.manufacturer_id.c_str());
  }
  if (!edid.product_id.empty()) {
    base::StringAppendF(&entry, "Product ID: %s - ", edid.product_id.c_str());
  }
  if (edid.year_of_manufacture != display::kInvalidYearOfManufacture) {
    base::StringAppendF(&entry, "Year of Manufacture: %d",
                        edid.year_of_manufacture);
  }
  return entry;
}

// Called from a worker thread via PostTaskAndReply.
void PopulateEntriesAsync(std::unique_ptr<SystemLogsResponse> response,
                          SysLogsSourceCallback callback) {
  auto populate_entries = [](SystemLogsResponse* response) {
    DCHECK(response);

    ash::system::StatisticsProvider* stats =
        ash::system::StatisticsProvider::GetInstance();
    DCHECK(stats);

    // Get the HWID.
    absl::optional<base::StringPiece> hwid =
        stats->GetMachineStatistic(ash::system::kHardwareClassKey);
    if (!hwid) {
      VLOG(1) << "Couldn't get machine statistic 'hardware_class'.";
    } else {
      response->emplace(kHWIDKey, std::string(hwid.value()));
    }

    // Get the firmware version.
    response->emplace(kChromeOsFirmwareVersion,
                      chromeos::version_loader::GetFirmware());
  };

  SystemLogsResponse* response_ptr = response.get();

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(populate_entries, response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));
}

void PopulateDiskSpaceLogsAsync(std::unique_ptr<SystemLogsResponse> response,
                                SysLogsSourceCallback callback) {
  auto on_get_free_disk_space = [](std::unique_ptr<SystemLogsResponse> response,
                                   SysLogsSourceCallback callback,
                                   absl::optional<int64_t> free_space) {
    auto on_get_total_disk_space =
        [](std::unique_ptr<SystemLogsResponse> response,
           SysLogsSourceCallback callback,
           absl::optional<int64_t> total_space) {
          if (total_space.has_value()) {
            response->emplace(kTotalDiskSpace,
                              base::NumberToString(total_space.value()));
          }
          PopulateEntriesAsync(std::move(response), std::move(callback));
        };

    if (free_space.has_value()) {
      response->emplace(kFreeDiskSpace,
                        base::NumberToString(free_space.value()));
    }

    // Might be null in some tests.
    if (ash::SpacedClient::Get()) {
      ash::SpacedClient::Get()->GetTotalDiskSpace(
          kChronosHomeDirectory,
          base::BindOnce(on_get_total_disk_space, std::move(response),
                         std::move(callback)));
    }
  };

  // Might be null in some tests.
  if (ash::SpacedClient::Get()) {
    ash::SpacedClient::Get()->GetFreeDiskSpace(
        kChronosHomeDirectory,
        base::BindOnce(on_get_free_disk_space, std::move(response),
                       std::move(callback)));
  } else {
    PopulateEntriesAsync(std::move(response), std::move(callback));
  }
}

// Called from the main (UI) thread, invokes |callback| when complete.
void PopulateMonitorInfoAsync(
    crosapi::mojom::CrosDisplayConfigController* cros_display_config_ptr,
    SystemLogsResponse* response,
    base::OnceCallback<void()> callback) {
  cros_display_config_ptr->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(
          [](SystemLogsResponse* response, base::OnceCallback<void()> callback,
             std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list) {
            std::string entry;
            for (const crosapi::mojom::DisplayUnitInfoPtr& info : info_list) {
              if (!entry.empty())
                base::StringAppendF(&entry, "\n");
              entry += GetDisplayInfoString(*info);
            }
            response->emplace(kMonitorInfoKey, entry);
            std::move(callback).Run();
          },
          response, std::move(callback)));
}

void OnPopulateMonitorInfoAsync(std::unique_ptr<SystemLogsResponse> response,
                                SysLogsSourceCallback callback) {
  PopulateDiskSpaceLogsAsync(std::move(response), std::move(callback));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetChromeVersionString() {
  // Version of the current running browser.
  std::string browser_version =
      chrome::GetVersionString(chrome::WithExtendedStable(true));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the device is receiving LTS updates, add a prefix to the version string.
  // The value of the policy is ignored here.
  std::string value;
  const bool is_lts =
      ash::CrosSettings::Get()->GetString(ash::kReleaseLtsTag, &value);
  if (is_lts)
    browser_version = kLTSChromeVersionPrefix + browser_version;

  // If lacros-chrome is allowed & supported, and launched before, which
  // is indicated by |browser_version| in BrowserManager being set to non-empty
  // string during lacros startup, attach its version in the chrome
  // version string.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      !crosapi::BrowserManager::Get()->browser_version().empty()) {
    std::string lacros_version =
        crosapi::BrowserManager::Get()->browser_version();
    return kLacrosChromeVersionPrefix + lacros_version + ", " +
           kAshChromeVersionPrefix + browser_version;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return browser_version;
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns true if the path identified by |key| with the PathService is a parent
// or ancestor of |child|.
bool IsParentOf(int key, const base::FilePath& child) {
  base::FilePath path;
  return base::PathService::Get(key, &path) && path.IsParent(child);
}

// Returns a string representing the overall install location of the browser.
// "Program Files" and "Program Files (x86)" are both considered "per-machine"
// locations (for all users), whereas anything in a user's local app data dir is
// considered a "per-user" location. This function returns an answer that gives,
// in essence, the broad category of location without checking that the browser
// is operating out of the exact expected install directory. It is interesting
// to know via feedback reports if updates are failing with
// CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY, which checks the exact directory,
// yet the reported install_location is not "unknown".
std::string DetermineInstallLocation() {
  base::FilePath exe_path;

  if (base::PathService::Get(base::FILE_EXE, &exe_path)) {
    if (IsParentOf(base::DIR_PROGRAM_FILESX86, exe_path) ||
        IsParentOf(base::DIR_PROGRAM_FILES, exe_path)) {
      return "per-machine";
    }
    if (IsParentOf(base::DIR_LOCAL_APP_DATA, exe_path))
      return "per-user";
  }
  return "unknown";
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC)
std::string MacCpuArchAsString() {
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      return "x86-64";
    case base::mac::CPUType::kTranslatedIntel:
      return "x86-64/translated";
    case base::mac::CPUType::kArm:
      return "arm64";
  }
}
#endif

}  // namespace

ChromeInternalLogSource::ChromeInternalLogSource()
    : SystemLogsSource("ChromeInternal") {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ChromeInternalLogSource::~ChromeInternalLogSource() {
}

void ChromeInternalLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  response->emplace(kChromeVersionTag, GetChromeVersionString());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  response->emplace(kChromeEnrollmentTag, GetEnrollmentStatusString());
#else
  // On ChromeOS, this will be pulled in from the LSB_RELEASE.
  std::string os_version = base::SysInfo::OperatingSystemName() + ": " +
                           base::SysInfo::OperatingSystemVersion();
  response->emplace(kOsVersionTag, os_version);
#endif

  PopulateSyncLogs(response.get());
  PopulateExtensionInfoLogs(response.get());
  PopulatePowerApiLogs(response.get());
#if BUILDFLAG(IS_WIN)
  PopulateUsbKeyboardDetected(response.get());
  PopulateEnrolledToDomain(response.get());
  PopulateInstallerBrandCode(response.get());
  PopulateLastUpdateState(response.get());
#endif

#if BUILDFLAG(IS_MAC)
  response->emplace(kCpuArch, MacCpuArchAsString());
#endif

  if (ProfileManager::GetLastUsedProfile()->IsChild())
    response->emplace("account_type", "child");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Store ARC enabled status.
  bool is_arc_enabled = arc::IsArcPlayStoreEnabledForProfile(
      ProfileManager::GetLastUsedProfile());
  response->emplace(kArcStatusKey, is_arc_enabled ? "enabled" : "disabled");
  if (is_arc_enabled) {
    PopulateArcPolicyStatus(response.get());
  }
  response->emplace(kAccountTypeKey, GetPrimaryAccountTypeString());
  response->emplace(kLacrosStatus, crosapi::browser_util::IsLacrosEnabled()
                                       ? "enabled"
                                       : "disabled");
  response->emplace(kDemoModeConfigKey, ash::DemoSession::DemoConfigToString(
                                            ash::DemoSession::GetDemoConfig()));
  PopulateLocalStateSettings(response.get());
  PopulateOnboardingTime(response.get());

  // Chain asynchronous fetchers: PopulateMonitorInfoAsync,
  // PopulateEntriesAsync, PopulateDiskSpaceAsync
  PopulateMonitorInfoAsync(
      cros_display_config_.get(), response.get(),
      base::BindOnce(&OnPopulateMonitorInfoAsync, std::move(response),
                     std::move(callback)));
#else
  // On other platforms, we're done. Invoke the callback.
  std::move(callback).Run(std::move(response));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeInternalLogSource::PopulateSyncLogs(SystemLogsResponse* response) {
#if BUILDFLAG(IS_CHROMEOS)
  // We are only interested in sync logs for the primary user profile.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
#else
  // Get logs for the last used profile since there is no notion of primary
  // profile.
  Profile* profile = ProfileManager::GetLastUsedProfile();
#endif
  if (!profile || !SyncServiceFactory::HasSyncService(profile))
    return;

  // Add sync logs to |response|.
  base::Value::Dict sync_logs = syncer::sync_ui_util::ConstructAboutInformation(
      syncer::sync_ui_util::IncludeSensitiveData(false),
      SyncServiceFactory::GetForProfile(profile),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  std::string serialized_sync_logs;
  JSONStringValueSerializer(&serialized_sync_logs).Serialize(sync_logs);
  response->emplace(kSyncDataKey, serialized_sync_logs);
}

void ChromeInternalLogSource::PopulateExtensionInfoLogs(
    SystemLogsResponse* response) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  std::string extensions_list;
  for (const scoped_refptr<const extensions::Extension>& extension :
       extension_registry->enabled_extensions()) {
    // Format the list as:
    // "extension_id" : "extension_name" : "extension_version".

    // Work around the anonymizer tool recognizing some versions as IPv4s.
    // Replaces dots "." by underscores "_".
    // We shouldn't change the anonymizer tool as it is working as intended; it
    // must err on the side of safety.
    std::string version;
    base::ReplaceChars(extension->VersionString(), ".", "_", &version);
    extensions_list += extension->id() + " : " + extension->name() +
                       " : version " + version + "\n";
  }

  if (!extensions_list.empty())
    response->emplace(kExtensionsListKey, extensions_list);
}

void ChromeInternalLogSource::PopulatePowerApiLogs(
    SystemLogsResponse* response) {
  std::string info;
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    // Some profiles cannot have extensions, such as the System Profile.
    if (extensions::ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(profile)) {
      continue;
    }

    for (const auto& it :
         extensions::PowerAPI::Get(profile)->extension_levels()) {
      if (!info.empty())
        info += ",\n";
      info += it.first + ": " + extensions::api::power::ToString(it.second);
    }
  }

  if (!info.empty())
    response->emplace(kPowerApiListKey, info);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeInternalLogSource::PopulateLocalStateSettings(
    SystemLogsResponse* response) {
  // Extract the "settings" entry in the local state and serialize back to
  // a string.
  base::Value::Dict local_state =
      g_browser_process->local_state()->GetPreferenceValues(
          PrefService::EXCLUDE_DEFAULTS);
  const base::Value::Dict* local_state_settings =
      local_state.FindDict(kSettingsKey);
  if (!local_state_settings) {
    VLOG(1) << "Failed to extract the settings entry from Local State.";
    return;
  }
  std::string serialized_settings;
  JSONStringValueSerializer serializer(&serialized_settings);
  if (!serializer.Serialize(*local_state_settings))
    return;

  response->emplace(kLocalStateSettingsResponseKey, serialized_settings);
}

void ChromeInternalLogSource::PopulateArcPolicyStatus(
    SystemLogsResponse* response) {
  response->emplace(kArcPolicyKey, arc::ArcPolicyBridge::GetForBrowserContext(
                                       ProfileManager::GetLastUsedProfile())
                                       ->get_arc_policy_for_reporting());
  response->emplace(kArcPolicyComplianceReportKey,
                    arc::ArcPolicyBridge::GetForBrowserContext(
                        ProfileManager::GetLastUsedProfile())
                        ->get_arc_policy_compliance_report());

  response->emplace(kArcDpcVersionKey,
                    arc::ArcPolicyBridge::GetForBrowserContext(
                        ProfileManager::GetLastUsedProfile())
                        ->get_arc_dpc_version());
}

void ChromeInternalLogSource::PopulateOnboardingTime(
    SystemLogsResponse* response) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile)
    return;
  base::Time time =
      profile->GetPrefs()->GetTime(ash::prefs::kOobeOnboardingTime);
  if (time.is_null())
    return;

  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  response->emplace(kOnboardingTime,
                    base::StringPrintf("%04d-%02d-%02d", exploded.year,
                                       exploded.month, exploded.day_of_month));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
void ChromeInternalLogSource::PopulateUsbKeyboardDetected(
    SystemLogsResponse* response) {
  std::string reason;
  bool result =
      base::win::IsKeyboardPresentOnSlate(ui::GetHiddenWindow(), &reason);
  reason.insert(0, result ? "Keyboard Detected:\n" : "No Keyboard:\n");
  response->emplace(kUsbKeyboardDetected, reason);
}

void ChromeInternalLogSource::PopulateEnrolledToDomain(
    SystemLogsResponse* response) {
  response->emplace(kIsEnrolledToDomain, base::win::IsEnrolledToDomain()
                                             ? "Enrolled to domain"
                                             : "Not enrolled to domain");
}

void ChromeInternalLogSource::PopulateInstallerBrandCode(
    SystemLogsResponse* response) {
  std::string brand;
  google_brand::GetBrand(&brand);
  response->emplace(kInstallerBrandCode,
                    brand.empty() ? "Unknown brand code" : brand);
}

void ChromeInternalLogSource::PopulateLastUpdateState(
    SystemLogsResponse* response) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const absl::optional<UpdateState> update_state = GetLastUpdateState();
  if (!update_state)
    return;  // There is nothing to include if no update check has completed.

  response->emplace(kUpdateErrorCode,
                    base::NumberToString(update_state->error_code));
  response->emplace(kInstallLocation, DetermineInstallLocation());

  if (update_state->error_code == GOOGLE_UPDATE_NO_ERROR)
    return;  // There is nothing more to include if the last check succeeded.

  response->emplace(kUpdateHresult,
                    base::StringPrintf("0x%08lX", update_state->hresult));
  if (update_state->installer_exit_code) {
    response->emplace(kInstallResultCode,
                      base::NumberToString(*update_state->installer_exit_code));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace system_logs
