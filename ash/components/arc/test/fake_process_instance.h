// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_

#include <deque>
#include <optional>
#include <utility>

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

  void set_apply_host_memory_pressure_response(uint32_t killed,
                                               uint64_t reclaimed) {
    host_memory_pressure_response_ = std::pair(killed, reclaimed);
  }

  void set_request_low_memory_kill_counts_response(
      arc::mojom::LowMemoryKillCountsPtr response) {
    low_memory_kill_counts_response_ = std::move(response);
  }

 private:
  // State to save the most recent call to HostMemoryPressure.
  bool host_memory_pressure_checked_ = true;
  mojom::PressureLevel host_memory_pressure_level_;
  int64_t host_memory_pressure_reclaim_target_;

  // Response to next call to  ApplyHostMemoryPressure.
  std::optional<std::pair<uint32_t, uint64_t>> host_memory_pressure_response_;

  // Response to next call to RequestLowMemoryKillCounts.
  std::optional<arc::mojom::LowMemoryKillCountsPtr>
      low_memory_kill_counts_response_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_PROCESS_INSTANCE_H_
