// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_process_instance.h"

#include <memory>
#include <utility>

#include "base/check_op.h"

namespace arc {

FakeProcessInstance::FakeProcessInstance() = default;

FakeProcessInstance::~FakeProcessInstance() = default;

void FakeProcessInstance::KillProcess(uint32_t pid, const std::string& reason) {
  DCHECK(false);
}

void FakeProcessInstance::RequestProcessList(
    RequestProcessListCallback callback) {
  DCHECK(false);
}

void FakeProcessInstance::RequestApplicationProcessMemoryInfo(
    RequestApplicationProcessMemoryInfoCallback callback) {
  DCHECK(false);
}

void FakeProcessInstance::RequestSystemProcessMemoryInfo(
    const std::vector<uint32_t>& nspids,
    RequestSystemProcessMemoryInfoCallback callback) {
  DCHECK(false);
}

void FakeProcessInstance::ApplyHostMemoryPressure(
    mojom::ProcessState level,
    int64_t reclaim_target,
    ApplyHostMemoryPressureCallback callback) {
  DCHECK(host_memory_pressure_checked_);
  host_memory_pressure_checked_ = false;
  host_memory_pressure_level_ = level;
  host_memory_pressure_reclaim_target_ = reclaim_target;
  host_memory_pressure_callback_ = std::move(callback);
}

bool FakeProcessInstance::CheckLastHostMemoryPressure(mojom::ProcessState level,
                                                      int64_t reclaim_target) {
  DCHECK(!host_memory_pressure_checked_);
  host_memory_pressure_checked_ = true;
  return level == host_memory_pressure_level_ &&
         reclaim_target == host_memory_pressure_reclaim_target_;
}

void FakeProcessInstance::RunHostMemoryPressureCallback(uint32_t killed,
                                                        uint64_t reclaimed) {
  DCHECK(host_memory_pressure_callback_);
  // NB: two moves, one to reset the unique_ptr, and one to reset the callback.
  std::move(host_memory_pressure_callback_).Run(killed, reclaimed);
}

}  // namespace arc
