// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_util.h"

#include <memory>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/device_event_log_source.h"
#include "chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/lacros_log_files_log_source.h"
#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<SupportToolHandler> GetSupportToolHandler(bool chrome_os,
                                                          bool chrome_browser) {
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  if (chrome_os) {
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches internal Chrome logs.",
            std::make_unique<system_logs::ChromeInternalLogSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Extracts the most recent crash IDs (if any) and exports them into "
            "crash_report_ids and all_crash_report_ids files.",
            std::make_unique<system_logs::CrashIdsSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches memory usage details and exports them into mem_usage and "
            "mem_usage_with_title files.",
            std::make_unique<system_logs::MemoryDetailsLogSource>()));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    handler->AddDataCollector(std::make_unique<UiHierarchyDataCollector>());
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Gathers log data from various scripts/programs. Creates and "
            "exports data into these files: alsa controls, cras, "
            "audio_diagnostics, env, disk_usage.",
            std::make_unique<system_logs::CommandLineLogSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches memory usage details. Creates and exports data into these "
            "files: dbus_details, dbus_summary.",
            std::make_unique<system_logs::DBusLogSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches entries for 'network_event_log' and 'device_event_log'.",
            std::make_unique<system_logs::DeviceEventLogSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches debug dump information from Intel Wi-Fi NICs that will be "
            "produced when those NICs have issues such as firmware crashes. "
            "Exports the logs into a file named iwlwifi_dump.",
            std::make_unique<system_logs::IwlwifiDumpLogSource>()));
    handler->AddDataCollector(
        std::make_unique<SystemLogSourceDataCollectorAdaptor>(
            "Fetches touch events, touchscreen and touchpad logs.",
            std::make_unique<system_logs::TouchLogSource>()));
    if (crosapi::BrowserManager::Get()->IsRunning() &&
        crosapi::BrowserManager::Get()->GetFeedbackDataSupported()) {
      handler->AddDataCollector(
          std::make_unique<SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::CrosapiSystemLogSource>()));
    }
    if (crosapi::browser_util::IsLacrosEnabled()) {
      // Lacros logs are saved in the user data directory, so we provide that
      // path to the LacrosLogFilesLogSource.
      base::FilePath log_base_path = crosapi::browser_util::GetUserDataDir();
      std::string lacrosUserLogKey = "lacros_user_log";
      handler->AddDataCollector(
          std::make_unique<SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::LacrosLogFilesLogSource>(
                  log_base_path, lacrosUserLogKey)));
    }
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
    if (base::FeatureList::IsEnabled(ash::features::kRevenLogSource))
      handler->AddDataCollector(
          std::make_unique<SystemLogSourceDataCollectorAdaptor>(
              "Collect Hardware data for ChromeOS Flex devices via "
              "cros_healthd calls.",
              std::make_unique<system_logs::RevenLogSource>()));
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  return handler;
}
