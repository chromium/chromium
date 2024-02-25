// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/functional/bind.h"
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

}  // namespace metrics
}  // namespace performance_manager
