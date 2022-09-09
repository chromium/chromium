// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_util.h"

#include <memory>
#include <set>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/support_tool/ash/network_routes_data_collector.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
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
#include "chrome/browser/support_tool/ash/shill_data_collector.h"
#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<SupportToolHandler> GetSupportToolHandler(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    std::set<support_tool::DataCollectorType> included_data_collectors) {
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>(case_id, email_address,
                                           issue_description);
  for (const auto& data_collector_type : included_data_collectors) {
    switch (data_collector_type) {
      case support_tool::CHROME_INTERNAL:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches internal Chrome logs.",
                std::make_unique<system_logs::ChromeInternalLogSource>()));
        break;
      case support_tool::CRASH_IDS:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Extracts the most recent crash IDs (if any) and exports them into "
            "crash_report_ids and all_crash_report_ids files.",
            std::make_unique<system_logs::CrashIdsSource>()));
        break;
      case support_tool::MEMORY_DETAILS:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches memory usage details and exports them into mem_usage and "
            "mem_usage_with_title files.",
            std::make_unique<system_logs::MemoryDetailsLogSource>()));
        break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      case support_tool::CHROMEOS_UI_HIERARCHY:
        handler->AddDataCollector(std::make_unique<UiHierarchyDataCollector>());
        break;
      case support_tool::CHROMEOS_NETWORK_ROUTES:
        handler->AddDataCollector(
            std::make_unique<NetworkRoutesDataCollector>());
        break;
      case support_tool::CHROMEOS_COMMAND_LINE:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Gathers log data from various scripts/programs. Creates and "
                "exports data into these files: alsa controls, cras, "
                "audio_diagnostics, env, disk_usage.",
                std::make_unique<system_logs::CommandLineLogSource>()));
        break;
      case support_tool::CHROMEOS_DEVICE_EVENT:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches entries for 'network_event_log' and 'device_event_log'.",
            std::make_unique<system_logs::DeviceEventLogSource>()));
        break;
      case support_tool::CHROMEOS_IWL_WIFI_DUMP:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches debug dump information from Intel Wi-Fi NICs that will be "
            "produced when those NICs have issues such as firmware crashes. "
            "Exports the logs into a file named iwlwifi_dump.",
            std::make_unique<system_logs::IwlwifiDumpLogSource>()));
        break;
      case support_tool::CHROMEOS_TOUCH_EVENTS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches touch events, touchscreen and touchpad logs.",
                std::make_unique<system_logs::TouchLogSource>()));
        break;
      case support_tool::CHROMEOS_DBUS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches DBus usage statistics. Creates and exports data into "
                "these files: dbus_details, dbus_summary.",
                std::make_unique<system_logs::DBusLogSource>()));
        break;
      case support_tool::CHROMEOS_CROS_API:
        if (crosapi::BrowserManager::Get()->IsRunning() &&
            crosapi::BrowserManager::Get()->GetFeedbackDataSupported()) {
          handler->AddDataCollector(std::make_unique<
                                    SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::CrosapiSystemLogSource>()));
        }
        break;
      case support_tool::CHROMEOS_LACROS:
        if (crosapi::browser_util::IsLacrosEnabled()) {
          // Lacros logs are saved in the user data directory, so we provide
          // that path to the LacrosLogFilesLogSource.
          base::FilePath log_base_path =
              crosapi::browser_util::GetUserDataDir();
          std::string lacrosUserLogKey = "lacros_user_log";
          handler->AddDataCollector(std::make_unique<
                                    SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::LacrosLogFilesLogSource>(
                  log_base_path, lacrosUserLogKey)));
        }
        break;
      case support_tool::CHROMEOS_SHILL:
        handler->AddDataCollector(std::make_unique<ShillDataCollector>());
        break;
      case support_tool::CHROMEOS_REVEN:
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Collect Hardware data for ChromeOS Flex devices via "
                "cros_healthd calls.",
                std::make_unique<system_logs::RevenLogSource>()));
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
        break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      default:
        break;
    }
  }
  return handler;
}
