// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_

#include "ash/components/arc/mojom/process.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeProcessInstance : public mojom::ProcessInstance {
 public:
  FakeProcessInstance();
  ~FakeProcessInstance() override;

  FakeProcessInstance(const FakeProcessInstance&) = delete;
  FakeProcessInstance& operator=(const FakeProcessInstance&) = delete;

  void KillProcess(uint32_t pid, const std::string& reason) override;
  void RequestProcessList(RequestProcessListCallback callback) override;
  void RequestApplicationProcessMemoryInfo(
      RequestApplicationProcessMemoryInfoCallback callback) override;
  void RequestSystemProcessMemoryInfo(
      const std::vector<uint32_t>& nspids,
      RequestSystemProcessMemoryInfoCallback callback) override;
  void ApplyHostMemoryPressureDeprecated(
      mojom::ProcessState level,
      int64_t reclaim_target,
      ApplyHostMemoryPressureCallback callback) override;
  void ApplyHostMemoryPressure(
      mojom::PressureLevel level,
      int64_t reclaim_target,
      ApplyHostMemoryPressureCallback callback) override;
  void RequestLowMemoryKillCounts(
      RequestLowMemoryKillCountsCallback callback) override;

  // Returns true if the last call to HostMemoryPressure had matching level and
  // reclaim_target arguments.
  bool CheckLastHostMemoryPressure(mojom::PressureLevel level,
                                   int64_t reclaim_target);

  // Returns false when there is a call to HostMemoryPressue not followed by
  // CheckLastHostMemoryPressure.
  bool IsLastHostMemoryPressureChecked() {
    return host_memory_pressure_checked_;
  }

  // Executes the callback from the last call to HostMemoryPressure.
  void RunHostMemoryPressureCallback(uint32_t killed, uint64_t reclaimed);

  // Executes the callback from the last call to RequestLowMemoryKillCounts.
  void RunRequestLowMemoryKillCountsCallback(
      mojom::LowMemoryKillCountsPtr counts);

 private:
  // State to save the most recent call to HostMemoryPressure.
  bool host_memory_pressure_checked_ = true;
  mojom::PressureLevel host_memory_pressure_level_;
  int64_t host_memory_pressure_reclaim_target_;
  ApplyHostMemoryPressureCallback host_memory_pressure_callback_;

  // State to save the most recent call to RequestLowMemoryKillCounts.
  RequestLowMemoryKillCountsCallback request_low_memory_kill_counts_callback_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_
