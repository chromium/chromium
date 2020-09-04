// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"
#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"
#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/device_event_log_source.h"
#include "chrome/browser/chromeos/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/chromeos/system_logs/network_health_source.h"
#include "chrome/browser/chromeos/system_logs/shill_log_source.h"
#include "chrome/browser/chromeos/system_logs/touch_log_source.h"
#include "chrome/browser/chromeos/system_logs/ui_hierarchy_log_source.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)
#include "chrome/browser/feedback/system_logs/log_sources/user_log_files_log_source.h"
#endif

namespace system_logs {

#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)
namespace {

constexpr char kDefaultLogPath[] = "/home/chronos/user/lacros/lacros.log";
constexpr char kLacrosUserLogKey[] = "lacros_user_log";

}  // namespace
#endif

SystemLogsFetcher* BuildChromeSystemLogsFetcher(bool scrub_data) {
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(
      scrub_data, extension_misc::kBuiltInFirstPartyExtensionIds);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<CrashIdsSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());

#if defined(OS_CHROMEOS)
  // These sources rely on scrubbing in SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<IwlwifiDumpChecker>());
  fetcher->AddSource(std::make_unique<TouchLogSource>());

  // Data sources that directly scrub itentifiable information.
  fetcher->AddSource(std::make_unique<DebugDaemonLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<NetworkHealthSource>(scrub_data));
  fetcher->AddSource(std::make_unique<ShillLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<UiHierarchyLogSource>(scrub_data));
#endif

#if BUILDFLAG(IS_LACROS)
  fetcher->AddSource(std::make_unique<UserLogFilesLogSource>(
      base::FilePath(kDefaultLogPath), kLacrosUserLogKey));
#endif

#if defined(OS_CHROMEOS)
  if (chromeos::features::IsLacrosSupportEnabled() &&
      crosapi::browser_util::IsLacrosAllowed()) {
    fetcher->AddSource(std::make_unique<UserLogFilesLogSource>(
        base::FilePath(kDefaultLogPath), kLacrosUserLogKey));
  }
#endif  // OS_CHROMEOS

  return fetcher;
}

}  // namespace system_logs
