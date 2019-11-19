// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/utils.h"

#include "base/hash/md5.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace resource_coordinator {

std::string SerializeOriginIntoDatabaseKey(const url::Origin& origin) {
  return base::MD5String(origin.host());
}

bool URLShouldBeStoredInLocalDatabase(const GURL& url) {
  // Only store information for the HTTP(S) sites for now.
  return url.SchemeIsHTTPOrHTTPS();
}

int GetPrivateMemoryKB(base::ProcessHandle handle) {
  // Private memory footprint of a process is calculated using its private bytes
  // and swap bytes. ProcessMetrics::GetTotalsSummary() is more precise in
  // getting the private bytes but can be very slow under heavy memory pressure.
  // Instead, use anonymous RSS as a faster estimation of private bytes for the
  // process.
  auto dump = memory_instrumentation::mojom::RawOSMemDump::New();
  dump->platform_private_footprint =
      memory_instrumentation::mojom::PlatformPrivateFootprint::New();
  bool success = memory_instrumentation::OSMetrics::FillOSMemoryDump(
      base::GetProcId(handle), dump.get());

  // Failed to get private memory for the process, e.g. the process has died.
  if (!success)
    return 0;

  uint64_t total_freed_bytes =
      dump->platform_private_footprint->rss_anon_bytes +
      dump->platform_private_footprint->vm_swap_bytes;
  return base::saturated_cast<int>(total_freed_bytes / 1024);
}

TabLifecycleUnitSource* GetTabLifecycleUnitSource() {
  DCHECK(g_browser_process);
  auto* source = g_browser_process->resource_coordinator_parts()
                     ->tab_lifecycle_unit_source();
  DCHECK(source);
  return source;
}

}  // namespace resource_coordinator
