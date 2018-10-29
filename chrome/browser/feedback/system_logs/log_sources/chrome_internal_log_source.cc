// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/about_sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/power.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
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

}  // namespace

ChromeInternalLogSource::ChromeInternalLogSource()
    : SystemLogsSource("ChromeInternal") {
#if defined(OS_CHROMEOS)
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &cros_display_config_ptr_);
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
      cros_display_config_ptr_.get(), response.get(),
      base::BindOnce(
          [](std::unique_ptr<SystemLogsResponse> response,
             SysLogsSourceCallback callback) {
            SystemLogsResponse* response_ptr = response.get();
            base::PostTaskWithTraitsAndReply(
                FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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
  if (!profile ||
      !ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(profile))
    return;

  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
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
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  bool is_data_reduction_proxy_enabled =
      prefs->HasPrefPath(prefs::kDataSaverEnabled) &&
      prefs->GetBoolean(prefs::kDataSaverEnabled);
  response->emplace(kDataReductionProxyKey,
                    is_data_reduction_proxy_enabled ? "enabled" : "disabled");
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
      base::win::IsKeyboardPresentOnSlate(&reason, ui::GetHiddenWindow());
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
#endif

}  // namespace system_logs
