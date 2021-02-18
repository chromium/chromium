// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/ash/system_logs/device_event_log_source.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/ash/system_logs/shill_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/ash/system_logs/ui_hierarchy_log_source.h"
#endif

namespace system_logs {

SystemLogsFetcher* BuildAboutSystemLogsFetcher() {
  const bool scrub_data = false;
  // We aren't anonymizing, so we can pass null for the 1st party IDs.
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(scrub_data, nullptr);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These sources rely on scrubbing in SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<TouchLogSource>());

  // Data sources that directly scrub itentifiable information.
  fetcher->AddSource(std::make_unique<DebugDaemonLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<NetworkHealthSource>(scrub_data));
  fetcher->AddSource(std::make_unique<ShillLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<UiHierarchyLogSource>(scrub_data));
#endif

  return fetcher;
}

}  // namespace system_logs
