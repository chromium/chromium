// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/performance_log_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/supervised_user/core/common/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/file_path.h"
#include "chrome/browser/ash/system_logs/app_service_log_source.h"
#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/ash/system_logs/device_data_manager_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/input_event_converter_log_source.h"
#include "chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/ash/system_logs/keyboard_info_log_source.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#include "chrome/browser/ash/system_logs/shill_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/ash/system_logs/traffic_counters_log_source.h"
#include "chrome/browser/ash/system_logs/ui_hierarchy_log_source.h"
#include "chrome/browser/ash/system_logs/virtual_keyboard_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/feedback/system_logs/log_sources/family_info_log_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

namespace system_logs {

SystemLogsFetcher* BuildChromeSystemLogsFetcher(Profile* profile,
                                                bool scrub_data) {
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(
      scrub_data, extension_misc::kBuiltInFirstPartyExtensionIds);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<CrashIdsSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());
  fetcher->AddSource(std::make_unique<PerformanceLogSource>());

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  // Identity manager is not available for Guest profile in ChromeOS ash.
  if (identity_manager) {
    fetcher->AddSource(std::make_unique<FamilyInfoLogSource>(
        identity_manager, profile->GetURLLoaderFactory(),
        *profile->GetPrefs()));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These sources rely on scrubbing in SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<BluetoothLogSource>());
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<IwlwifiDumpChecker>());
  fetcher->AddSource(std::make_unique<TouchLogSource>());
  fetcher->AddSource(std::make_unique<InputEventConverterLogSource>());
  fetcher->AddSource(std::make_unique<ConnectedInputDevicesLogSource>());
  fetcher->AddSource(
      std::make_unique<DeviceDataManagerInputDevicesLogSource>());
  fetcher->AddSource(std::make_unique<TrafficCountersLogSource>());

  // Data sources that directly scrub itentifiable information.
  fetcher->AddSource(std::make_unique<DebugDaemonLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<NetworkHealthSource>(
      scrub_data, /*include_guid_when_not_scrub=*/false));

  fetcher->AddSource(std::make_unique<VirtualKeyboardLogSource>());
  fetcher->AddSource(std::make_unique<AppServiceLogSource>());
  fetcher->AddSource(std::make_unique<KeyboardInfoLogSource>());
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
  fetcher->AddSource(std::make_unique<RevenLogSource>());
#endif

  fetcher->AddSource(std::make_unique<ShillLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<UiHierarchyLogSource>(scrub_data));
#endif

  return fetcher;
}

}  // namespace system_logs
