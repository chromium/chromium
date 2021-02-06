// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_MEMORY_PRESSURE_METRICS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_MEMORY_PRESSURE_METRICS_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {
namespace metrics {

// Record some memory pressure related metrics.
class MemoryPressureMetrics : public GraphOwned {
 public:
  MemoryPressureMetrics();
  ~MemoryPressureMetrics() override;
  MemoryPressureMetrics(const MemoryPressureMetrics& other) = delete;
  MemoryPressureMetrics& operator=(const MemoryPressureMetrics&) = delete;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void set_system_ram_mb_for_testing(int system_ram_mb) {
    system_ram_mb_ = system_ram_mb;
  }

 private:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
  static constexpr int kInvalidSysRAMValue = 0;

  void OnMemoryPressure(MemoryPressureLevel pressure_level);

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  Graph* graph_ = nullptr;
  int system_ram_mb_ = kInvalidSysRAMValue;
};

}  // namespace metrics
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_MEMORY_PRESSURE_METRICS_H_
