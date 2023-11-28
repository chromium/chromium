// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_process_instance.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"

namespace arc {

FakeProcessInstance::FakeProcessInstance() = default;

FakeProcessInstance::~FakeProcessInstance() = default;

void FakeProcessInstance::KillProcess(uint32_t pid, const std::string& reason) {
  NOTIMPLEMENTED();
}

void FakeProcessInstance::RequestProcessList(
    RequestProcessListCallback callback) {
  NOTIMPLEMENTED();
}

void FakeProcessInstance::RequestApplicationProcessMemoryInfo(
    RequestApplicationProcessMemoryInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeProcessInstance::RequestSystemProcessMemoryInfo(
    const std::vector<uint32_t>& nspids,
    RequestSystemProcessMemoryInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeProcessInstance::ApplyHostMemoryPressureDeprecated(
    mojom::ProcessState level,
    int64_t reclaim_target,
    ApplyHostMemoryPressureCallback callback) {
  NOTIMPLEMENTED();
}

void FakeProcessInstance::ApplyHostMemoryPressure(
    mojom::PressureLevel level,
    int64_t reclaim_target,
    ApplyHostMemoryPressureCallback callback) {
  DCHECK(host_memory_pressure_checked_);
  host_memory_pressure_checked_ = false;
  host_memory_pressure_level_ = level;
  host_memory_pressure_reclaim_target_ = reclaim_target;

  DCHECK(host_memory_pressure_response_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), host_memory_pressure_response_->first,
                     host_memory_pressure_response_->second));
  host_memory_pressure_response_ = std::nullopt;
}

void FakeProcessInstance::RequestLowMemoryKillCounts(
    RequestLowMemoryKillCountsCallback callback) {
  DCHECK(low_memory_kill_counts_response_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::move(*low_memory_kill_counts_response_)));
}

bool FakeProcessInstance::CheckLastHostMemoryPressure(
    mojom::PressureLevel level,
    int64_t reclaim_target) {
  DCHECK(!host_memory_pressure_checked_);
  host_memory_pressure_checked_ = true;
  return level == host_memory_pressure_level_ &&
         reclaim_target == host_memory_pressure_reclaim_target_;
}

}  // namespace arc
