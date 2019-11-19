// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"

#include "build/build_config.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"
#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"
#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/device_event_log_source.h"
#include "chrome/browser/chromeos/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/chromeos/system_logs/touch_log_source.h"
#endif

namespace system_logs {

SystemLogsFetcher* BuildChromeSystemLogsFetcher() {
  const bool scrub_data = true;
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(
      scrub_data, extension_misc::kBuiltInFirstPartyExtensionIds);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<CrashIdsSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());

#if defined(OS_CHROMEOS)
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<IwlwifiDumpChecker>());
  fetcher->AddSource(std::make_unique<TouchLogSource>());

  // Debug Daemon data source - currently only this data source supports
  // the scrub_data parameter, but the others still get scrubbed by
  // SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<DebugDaemonLogSource>(scrub_data));
#endif

  return fetcher;
}

}  // namespace system_logs
