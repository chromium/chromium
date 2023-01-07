// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include <windows.h>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_monitor {

namespace {

const DWORDLONG kMBBytes = 1024 * 1024;

MetricEvaluatorsHelperWin* g_metric_evaluator_instance = nullptr;

}  // namespace

MetricEvaluatorsHelperWin::MetricEvaluatorsHelperWin() {
  DCHECK(!g_metric_evaluator_instance);
  g_metric_evaluator_instance = this;
}

MetricEvaluatorsHelperWin::~MetricEvaluatorsHelperWin() {
  DCHECK_EQ(this, g_metric_evaluator_instance);
  g_metric_evaluator_instance = nullptr;
}

absl::optional<int> MetricEvaluatorsHelperWin::GetFreePhysicalMemoryMb() {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return absl::nullopt;

  return (mem_status.ullAvailPhys / kMBBytes);
}

}  // namespace performance_monitor
