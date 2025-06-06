// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <lib/zx/object.h>
#include <lib/zx/process.h>
#include <lib/zx/thread.h>
#include <zircon/errors.h>
#include <zircon/process.h>
#include <zircon/rights.h>
#include <zircon/types.h>

#include <optional>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace base {

std::optional<TimeDelta> PlatformThreadMetrics::GetCumulativeCPUUsage() {
  TRACE_EVENT("base", "Thread::GetCumulativeCPUUsage");
  zx::thread thread;
  zx_status_t status =
      zx::unowned_process(zx_process_self())
          ->get_child(tid_.raw(), ZX_RIGHT_SAME_RIGHTS, &thread);
  if (status != ZX_OK) {
    return std::nullopt;
  }
  zx_info_task_runtime_t stats;
  status = thread.get_info(ZX_INFO_TASK_RUNTIME, &stats, sizeof(stats), nullptr,
                           nullptr);
  if (status != ZX_OK) {
    return std::nullopt;
  }
  return TimeDelta::FromZxDuration(stats.cpu_time);
}

}  // namespace base
