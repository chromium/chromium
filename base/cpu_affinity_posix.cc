// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu_affinity_posix.h"

#include <sched.h>

#include "base/cpu.h"
#include "base/process/internal_linux.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

namespace {

const cpu_set_t& AllCores() {
  static const cpu_set_t kAllCores = []() {
    cpu_set_t set;
    CPU_ZERO(&set);
    const std::vector<CPU::CoreType>& core_types = CPU::GetGuessedCoreTypes();
    if (core_types.empty()) {
      memset(&set, 0xff, sizeof(set));
    } else {
      for (size_t index = 0; index < core_types.size(); index++)
        CPU_SET(index, &set);
    }
    return set;
  }();
  return kAllCores;
}

const cpu_set_t& LittleCores() {
  static const cpu_set_t kLittleCores = []() {
    const std::vector<CPU::CoreType>& core_types = CPU::GetGuessedCoreTypes();
    if (core_types.empty())
      return AllCores();

    cpu_set_t set;
    CPU_ZERO(&set);
    for (size_t core_index = 0; core_index < core_types.size(); core_index++) {
      switch (core_types[core_index]) {
        case CPU::CoreType::kUnknown:
        case CPU::CoreType::kOther:
        case CPU::CoreType::kSymmetric:
          // In the presence of an unknown core type or symmetric architecture,
          // fall back to allowing all cores.
          return AllCores();
        case CPU::CoreType::kBigLittle_Little:
        case CPU::CoreType::kBigLittleBigger_Little:
          CPU_SET(core_index, &set);
          break;
        case CPU::CoreType::kBigLittle_Big:
        case CPU::CoreType::kBigLittleBigger_Big:
        case CPU::CoreType::kBigLittleBigger_Bigger:
          break;
      }
    }
    return set;
  }();
  return kLittleCores;
}

}  // anonymous namespace

bool HasBigCpuCores() {
  static const bool kHasBigCores = []() {
    const std::vector<CPU::CoreType>& core_types = CPU::GetGuessedCoreTypes();
    if (core_types.empty())
      return false;
    for (CPU::CoreType core_type : core_types) {
      switch (core_type) {
        case CPU::CoreType::kUnknown:
        case CPU::CoreType::kOther:
        case CPU::CoreType::kSymmetric:
          return false;
        case CPU::CoreType::kBigLittle_Little:
        case CPU::CoreType::kBigLittleBigger_Little:
        case CPU::CoreType::kBigLittle_Big:
        case CPU::CoreType::kBigLittleBigger_Big:
        case CPU::CoreType::kBigLittleBigger_Bigger:
          return true;
      }
    }
    return false;
  }();
  return kHasBigCores;
}

bool SetThreadCpuAffinityMode(PlatformThreadId thread_id,
                              CpuAffinityMode affinity) {
  int result = 0;
  switch (affinity) {
    case CpuAffinityMode::kDefault: {
      const cpu_set_t& all_cores = AllCores();
      result = sched_setaffinity(thread_id, sizeof(all_cores), &all_cores);
      break;
    }
    case CpuAffinityMode::kLittleCoresOnly: {
      const cpu_set_t& little_cores = LittleCores();
      result =
          sched_setaffinity(thread_id, sizeof(little_cores), &little_cores);
      break;
    }
  }
  return result == 0;
}

bool SetProcessCpuAffinityMode(ProcessHandle process_handle,
                               CpuAffinityMode affinity) {
  bool any_threads = false;
  bool result = true;

  internal::ForEachProcessTask(
      process_handle, [&any_threads, &result, affinity](
                          PlatformThreadId tid, const FilePath& /*task_path*/) {
        any_threads = true;
        result &= SetThreadCpuAffinityMode(tid, affinity);
      });

  return any_threads && result;
}

absl::optional<CpuAffinityMode> CurrentThreadCpuAffinityMode() {
  if (HasBigCpuCores()) {
    cpu_set_t set;
    sched_getaffinity(PlatformThread::CurrentId(), sizeof(set), &set);
    if (CPU_EQUAL(&set, &AllCores()))
      return CpuAffinityMode::kDefault;
    if (CPU_EQUAL(&set, &LittleCores()))
      return CpuAffinityMode::kLittleCoresOnly;
  }
  return absl::nullopt;
}

}  // namespace base
