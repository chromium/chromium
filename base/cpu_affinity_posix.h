// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CPU_AFFINITY_POSIX_H_
#define BASE_CPU_AFFINITY_POSIX_H_

#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

enum class CpuAffinityMode {
  // No restrictions on affinity.
  kDefault,
  // Restrict execution to LITTLE cores only. Only has an effect on platforms
  // where we detect presence of big.LITTLE-like CPU architectures.
  kLittleCoresOnly
};

// Sets or clears restrictions on the CPU affinity of the specified thread.
// Returns false if updating the affinity failed.
BASE_EXPORT bool SetThreadCpuAffinityMode(PlatformThreadId thread_id,
                                          CpuAffinityMode affinity);
// Like SetThreadAffinityMode, but affects all current and future threads of
// the given process. Note that this may not apply to threads that are created
// in parallel to the execution of this function.
BASE_EXPORT bool SetProcessCpuAffinityMode(ProcessHandle process_handle,
                                           CpuAffinityMode affinity);

// Return true if the current architecture has big or bigger cores.
BASE_EXPORT bool HasBigCpuCores();

// For architectures with big cores, return the affinity mode that matches
// the CPU affinity of the current thread. If no affinity mode exactly matches,
// or if the architecture doesn't have different types of cores,
// return nullopt.
BASE_EXPORT absl::optional<CpuAffinityMode> CurrentThreadCpuAffinityMode();

}  // namespace base

#endif  // BASE_CPU_AFFINITY_POSIX_H_
