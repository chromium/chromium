// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "ash/public/mojom/constants.mojom.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/system_connector.h"
#include "services/service_manager/public/cpp/connector.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/google/google_update_win.h"
#endif
#include "ui/base/win/hidden_window.h"
#endif

namespace system_logs {

namespace {

constexpr char kSyncDataKey[] = "about_sync_data";
constexpr char kExtensionsListKey[] = "extensions";
constexpr char kPowerApiListKey[] = "chrome.power extensions";
constexpr char kDataReductionProxyKey[] = "data_reduction_proxy";
constexpr char kChromeVersionTag[] = "CHROME VERSION";
#if defined(OS_CHROMEOS)
constexpr char kChromeOsFirmwareVersion[] = "CHROMEOS_FIRMWARE_VERSION";
constexpr char kChromeEnrollmentTag[] = "ENTERPRISE_ENROLLED";
constexpr char kHWIDKey[] = "HWID";
constexpr char kSettingsKey[] = "settings";
constexpr char kLocalStateSettingsResponseKey[] = "Local State: settings";
constexpr char kArcStatusKey[] = "CHROMEOS_ARC_STATUS";
constexpr char kMonitorInfoKey[] = "monitor_info";
constexpr char kAccountTypeKey[] = "account_type";
constexpr char kDemoModeConfigKey[] = "demo_mode_config";
#else
constexpr char kOsVersionTag[] = "OS VERSION";
#endif
#if defined(OS_WIN)
constexpr char kUsbKeyboardDetected[] = "usb_keyboard_detected";
constexpr char kIsEnrolledToDomain[] = "enrolled_to_domain";
constexpr char kInstallerBrandCode[] = "installer_brand_code";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kUpdateErrorCode[] = "update_error_code";
constexpr char kUpdateHresult[] = "update_hresult";
constexpr char kInstallResultCode[] = "install_result_code";
constexpr char kInstallLocation[] = "install_location";
#endif
#endif

#if defined(OS_CHROMEOS)

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
    case user_manager::USER_TYPE_SUPERVISED:
      return "supervised";
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
    case ChromeOSMetricsProvider::NON_MANAGED:
      return "Not managed";
    case ChromeOSMetricsProvider::MANAGED:
      return "Managed";
    case ChromeOSMetricsProvider::UNUSED:
    case ChromeOSMetricsProvider::ERROR_GETTING_ENROLLMENT_STATUS:
    case ChromeOSMetricsProvider::ENROLLMENT_STATUS_MAX:
      return "Error retrieving status";
  }
  // For compilers that don't recognize all cases handled above.
  NOTREACHED();
  return std::string();
}

std::string GetDisplayInfoString(
    const ash::mojom::DisplayUnitInfo& display_info) {
  std::string entry;
  if (!display_info.name.empty())
    base::StringAppendF(&entry, "%s : ", display_info.name.c_str());
  if (!display_info.edid)
    return entry;
  const ash::mojom::Edid& edid = *display_info.edid;
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

// Called from the main (UI) thread, invokes |callback| when complete.
void PopulateMonitorInfoAsync(
    ash::mojom::CrosDisplayConfigController* cros_display_config_ptr,
    SystemLogsResponse* response,
    base::OnceCallback<void()> callback) {
  cros_display_config_ptr->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(
          [](SystemLogsResponse* response, base::OnceCallback<void()> callback,
             std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
            std::string entry;
            for (const ash::mojom::DisplayUnitInfoPtr& info : info_list) {
              if (!entry.empty())
                base::StringAppendF(&entry, "\n");
              entry += GetDisplayInfoString(*info);
            }
            response->emplace(kMonitorInfoKey, entry);
            std::move(callback).Run();
          },
          response, std::move(callback)));
}

// Called from a worker thread via PostTaskAndReply.
void PopulateEntriesAsync(SystemLogsResponse* response) {
  DCHECK(response);

  chromeos::system::StatisticsProvider* stats =
      chromeos::system::StatisticsProvider::GetInstance();
  DCHECK(stats);

  // Get the HWID.
  std::string hwid;
  if (!stats->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid))
    VLOG(1) << "Couldn't get machine statistic 'hardware_class'.";
  else
    response->emplace(kHWIDKey, hwid);

  // Get the firmware version.
  response->emplace(kChromeOsFirmwareVersion,
                    chromeos::version_loader::GetFirmware());
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

ChromeInternalLogSource::ChromeInternalLogSource()
    : SystemLogsSource("ChromeInternal") {
#if defined(OS_CHROMEOS)
  content::GetSystemConnector()->Connect(
      ash::mojom::kServiceName,
      cros_display_config_.BindNewPipeAndPassReceiver());
#endif
}

ChromeInternalLogSource::~ChromeInternalLogSource() {
}

void ChromeInternalLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  response->emplace(kChromeVersionTag, chrome::GetVersionString());

#if defined(OS_CHROMEOS)
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
  PopulateDataReductionProxyLogs(response.get());
#if defined(OS_WIN)
  PopulateUsbKeyboardDetected(response.get());
  PopulateEnrolledToDomain(response.get());
  PopulateInstallerBrandCode(response.get());
  PopulateLastUpdateState(response.get());
#endif

  if (ProfileManager::GetLastUsedProfile()->IsChild())
    response->emplace("account_type", "child");

#if defined(OS_CHROMEOS)
  // Store ARC enabled status.
  response->emplace(kArcStatusKey, arc::IsArcPlayStoreEnabledForProfile(
                                       ProfileManager::GetLastUsedProfile())
                                       ? "enabled"
                                       : "disabled");
  response->emplace(kAccountTypeKey, GetPrimaryAccountTypeString());
  response->emplace(kDemoModeConfigKey,
                    chromeos::DemoSession::DemoConfigToString(
                        chromeos::DemoSession::GetDemoConfig()));
  PopulateLocalStateSettings(response.get());

  // Chain asynchronous fetchers: PopulateMonitorInfoAsync, PopulateEntriesAsync
  PopulateMonitorInfoAsync(
      cros_display_config_.get(), response.get(),
      base::BindOnce(
          [](std::unique_ptr<SystemLogsResponse> response,
             SysLogsSourceCallback callback) {
            SystemLogsResponse* response_ptr = response.get();
            base::PostTaskAndReply(
                FROM_HERE,
                {base::ThreadPool(), base::MayBlock(),
                 base::TaskPriority::BEST_EFFORT},
                base::BindOnce(&PopulateEntriesAsync, response_ptr),
                base::BindOnce(std::move(callback), std::move(response)));
          },
          std::move(response), std::move(callback)));
#else
  // On other platforms, we're done. Invoke the callback.
  std::move(callback).Run(std::move(response));
#endif  // defined(OS_CHROMEOS)
}

void ChromeInternalLogSource::PopulateSyncLogs(SystemLogsResponse* response) {
  // We are only interested in sync logs for the primary user profile.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || !ProfileSyncServiceFactory::HasSyncService(profile))
    return;

  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> sync_logs(
      syncer::sync_ui_util::ConstructAboutInformation(service,
                                                      chrome::GetChannel()));

  // Remove identity section.
  base::ListValue* details = NULL;
  sync_logs->GetList(syncer::sync_ui_util::kDetailsKey, &details);
  if (!details)
    return;
  for (auto it = details->begin(); it != details->end(); ++it) {
    base::DictionaryValue* dict = NULL;
    if (it->GetAsDictionary(&dict)) {
      std::string title;
      dict->GetString("title", &title);
      if (title == syncer::sync_ui_util::kIdentityTitle) {
        details->Erase(it, NULL);
        break;
      }
    }
  }

  // Add sync logs to logs.
  std::string sync_logs_string;
  JSONStringValueSerializer serializer(&sync_logs_string);
  serializer.Serialize(*sync_logs.get());

  response->emplace(kSyncDataKey, sync_logs_string);
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

void ChromeInternalLogSource::PopulateDataReductionProxyLogs(
    SystemLogsResponse* response) {
  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              ProfileManager::GetActiveUserProfile());
  bool data_saver_enabled =
      data_reduction_proxy_settings &&
      data_reduction_proxy_settings->IsDataReductionProxyEnabled();
  response->emplace(kDataReductionProxyKey,
                    data_saver_enabled ? "enabled" : "disabled");
}

#if defined(OS_CHROMEOS)
void ChromeInternalLogSource::PopulateLocalStateSettings(
    SystemLogsResponse* response) {
  // Extract the "settings" entry in the local state and serialize back to
  // a string.
  std::unique_ptr<base::DictionaryValue> local_state =
      g_browser_process->local_state()->GetPreferenceValues(
          PrefService::EXCLUDE_DEFAULTS);
  const base::DictionaryValue* local_state_settings = nullptr;
  if (!local_state->GetDictionary(kSettingsKey, &local_state_settings)) {
    VLOG(1) << "Failed to extract the settings entry from Local State.";
    return;
  }
  std::string serialized_settings;
  JSONStringValueSerializer serializer(&serialized_settings);
  if (!serializer.Serialize(*local_state_settings))
    return;

  response->emplace(kLocalStateSettingsResponseKey, serialized_settings);
}

#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
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
  const base::Optional<UpdateState> update_state = GetLastUpdateState();
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
#endif  // defined(OS_WIN)

}  // namespace system_logs
