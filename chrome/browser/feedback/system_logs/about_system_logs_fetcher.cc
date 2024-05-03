// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/related_website_sets_source.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "chrome/browser/feedback/system_logs/log_sources/chrome_root_store_log_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/ash/system_logs/device_data_manager_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/input_event_converter_log_source.h"
#include "chrome/browser/ash/system_logs/keyboard_info_log_source.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#include "chrome/browser/ash/system_logs/shill_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/ash/system_logs/traffic_counters_log_source.h"
#include "chrome/browser/ash/system_logs/ui_hierarchy_log_source.h"
#endif

namespace system_logs {

SystemLogsFetcher* BuildAboutSystemLogsFetcher(content::WebUI* web_ui) {
  const bool scrub_data = false;
  // We aren't anonymizing, so we can pass null for the 1st party IDs.
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(scrub_data, nullptr);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());
  fetcher->AddSource(std::make_unique<RelatedWebsiteSetsSource>(
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(Profile::FromWebUI(web_ui))));

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  fetcher->AddSource(std::make_unique<ChromeRootStoreLogSource>());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These sources rely on scrubbing in SystemLogsFetcher.
  fetcher->AddSource(std::make_unique<BluetoothLogSource>());
  fetcher->AddSource(std::make_unique<CommandLineLogSource>());
  fetcher->AddSource(std::make_unique<DBusLogSource>());
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
  fetcher->AddSource(std::make_unique<RevenLogSource>());
#endif

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
  fetcher->AddSource(std::make_unique<ShillLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<UiHierarchyLogSource>(scrub_data));
  fetcher->AddSource(std::make_unique<KeyboardInfoLogSource>());
#endif

  return fetcher;
}

}  // namespace system_logs
