// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <mach/mach.h>
#include <pthread.h>

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace base {

// static
std::unique_ptr<PlatformThreadMetrics> PlatformThreadMetrics::CreateFromHandle(
    PlatformThreadHandle handle) {
  if (handle.is_null()) {
    return nullptr;
  }
  return WrapUnique(new PlatformThreadMetrics(handle));
}

std::optional<TimeDelta> PlatformThreadMetrics::GetCumulativeCPUUsage() {
  TRACE_EVENT("base", "Thread::GetCumulativeCPUUsage");
  mach_port_t thread = pthread_mach_thread_np(handle_.platform_handle());
  if (thread == MACH_PORT_NULL) {
    return std::nullopt;
  }

  thread_basic_info_data_t thread_info_data;
  mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kr = thread_info(
      thread, THREAD_BASIC_INFO,
      reinterpret_cast<thread_info_t>(&thread_info_data), &thread_info_count);
  if (kr != KERN_SUCCESS) {
    return std::nullopt;
  }

  TimeDelta cpu_time = Seconds(thread_info_data.user_time.seconds);
  cpu_time += Microseconds(thread_info_data.user_time.microseconds);
  cpu_time += Seconds(thread_info_data.system_time.seconds);
  cpu_time += Microseconds(thread_info_data.system_time.microseconds);
  return cpu_time;
}

}  // namespace base
