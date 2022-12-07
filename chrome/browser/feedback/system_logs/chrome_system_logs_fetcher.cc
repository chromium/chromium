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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/file_path.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#include "chrome/browser/ash/system_logs/shill_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/ash/system_logs/traffic_counters_log_source.h"
#include "chrome/browser/ash/system_logs/ui_hierarchy_log_source.h"
#include "chrome/browser/ash/system_logs/virtual_keyboard_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/feedback/system_logs/log_sources/lacros_log_files_log_source.h"
#endif

namespace system_logs {

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

constexpr char kLacrosUserLogKey[] = "lacros_user_log";

}  // namespace
#endif

SystemLogsFetcher* BuildChromeSystemLogsFetcher(bool scrub_data) {
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(
      scrub_data, extension_misc::kBuiltInFirstPartyExtensionIds);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<CrashIdsSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());
  fetcher->AddSource(std::make_unique<PerformanceLogSource>());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These sources rely on scrubbing in SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<BluetoothLogSource>());
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<IwlwifiDumpChecker>());
  fetcher->AddSource(std::make_unique<TouchLogSource>());
  fetcher->AddSource(std::make_unique<ConnectedInputDevicesLogSource>());
  fetcher->AddSource(std::make_unique<TrafficCountersLogSource>());

  // Data sources that directly scrub itentifiable information.
  fetcher->AddSource(std::make_unique<DebugDaemonLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<NetworkHealthSource>(
      scrub_data, /*include_guid_when_not_scrub=*/false));

  fetcher->AddSource(std::make_unique<VirtualKeyboardLogSource>());
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
  fetcher->AddSource(std::make_unique<RevenLogSource>());
#endif

  fetcher->AddSource(std::make_unique<ShillLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<UiHierarchyLogSource>(scrub_data));

  // Add CrosapiSystemLogSource to get lacros system information log data
  // if Lacros is running and the crosapi version supports the Lacros remote
  // data source.
  if (crosapi::BrowserManager::Get()->IsRunning() &&
      crosapi::BrowserManager::Get()->GetFeedbackDataSupported()) {
    fetcher->AddSource(std::make_unique<CrosapiSystemLogSource>());
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled()) {
    // Lacros logs are saved in the user data directory, so we provide that
    // path to the LacrosLogFilesLogSource.
    base::FilePath log_base_path = crosapi::browser_util::GetUserDataDir();
    fetcher->AddSource(std::make_unique<LacrosLogFilesLogSource>(
        log_base_path, kLacrosUserLogKey));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return fetcher;
}

}  // namespace system_logs
