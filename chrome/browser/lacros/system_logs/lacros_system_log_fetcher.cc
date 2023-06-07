// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/ozone_wayland_state_dump_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"

namespace system_logs {

SystemLogsFetcher* BuildLacrosSystemLogsFetcher(bool scrub_data) {
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(
      scrub_data, extension_misc::kBuiltInFirstPartyExtensionIds);

  fetcher->AddSource(std::make_unique<ChromeInternalLogSource>());
  fetcher->AddSource(std::make_unique<CrashIdsSource>());
  fetcher->AddSource(std::make_unique<DeviceEventLogSource>());
  fetcher->AddSource(std::make_unique<MemoryDetailsLogSource>());
  fetcher->AddSource(std::make_unique<OzoneWaylandStateDumpSource>());
  return fetcher;
}

}  // namespace system_logs
