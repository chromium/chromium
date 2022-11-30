// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/performance_manager/metrics/memory_pressure_metrics.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace metrics {

MemoryPressureMetrics::MemoryPressureMetrics() = default;

MemoryPressureMetrics::~MemoryPressureMetrics() = default;

void MemoryPressureMetrics::OnPassedToGraph(Graph* graph) {
  graph_ = graph;

  graph_->AddSystemNodeObserver(this);
  base::SystemMemoryInfoKB mem_info = {};
  if (base::GetSystemMemoryInfo(&mem_info))
    system_ram_mb_ = mem_info.total / 1024;
}

void MemoryPressureMetrics::OnTakenFromGraph(Graph* graph) {
  graph_->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
}

void MemoryPressureMetrics::OnBeforeMemoryPressure(
    MemoryPressureLevel new_level) {
  if (new_level != MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  int total_rss_mb = 0;
  for (const auto* node : graph_->GetAllProcessNodes())
    total_rss_mb += node->GetResidentSetKb();
  total_rss_mb /= 1024;

  // Records an estimate of the total amount of RAM used by Chrome.
  // Note: The histogram will cap at 100GB.
  UMA_HISTOGRAM_COUNTS_100000("Discarding.OnCriticalPressure.TotalRSS_Mb2",
                              total_rss_mb);

  if (system_ram_mb_ != kInvalidSysRAMValue) {
    DCHECK_EQ(0, kInvalidSysRAMValue);
    int footprint_percent_of_total_ram = 100 * total_rss_mb / system_ram_mb_;
    // Records an estimate of the percentage of RAM currently used by Chrome.
    // Note that the estimate might exceeds 100% in some cases as it overcount
    // the shared memory, round the value down to 100% in this case.
    UMA_HISTOGRAM_PERCENTAGE(
        "Discarding.OnCriticalPressure.TotalRSS_PercentOfRAM2",
        std::min(100, static_cast<int>(footprint_percent_of_total_ram)));
  }
}

}  // namespace metrics
}  // namespace performance_manager
