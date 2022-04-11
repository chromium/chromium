// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder.h"

#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"

namespace performance_monitor {

ProcessMetricsRecorder::ProcessMetricsRecorder(
    ProcessMonitor* process_monitor) {
  process_monitor_observation_.Observe(process_monitor);
}

ProcessMetricsRecorder::~ProcessMetricsRecorder() = default;

void ProcessMetricsRecorder::OnMetricsSampled(
    int process_type,
    ProcessSubtypes process_subtype,
    const ProcessMonitor::Metrics& metrics) {
  // The histogram macros don't support variables as histogram names,
  // hence the macro duplication for each process type.
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      RecordProcessHistograms("BrowserProcess", metrics);
      break;
    case content::PROCESS_TYPE_RENDERER:
      RecordProcessHistograms("RendererProcess", metrics);
      break;
    case content::PROCESS_TYPE_GPU:
      RecordProcessHistograms("GPUProcess", metrics);
      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      RecordProcessHistograms("PPAPIProcess", metrics);
      break;
    case content::PROCESS_TYPE_UTILITY:
      RecordProcessHistograms("UtilityProcess", metrics);
      break;
    default:
      break;
  }

  switch (process_subtype) {
    case kProcessSubtypeUnknown:
      break;
    case kProcessSubtypeExtensionPersistent:
      RecordProcessHistograms("RendererExtensionPersistentProcess", metrics);
      break;
    case kProcessSubtypeExtensionEvent:
      RecordProcessHistograms("RendererExtensionEventProcess", metrics);
      break;
    case kProcessSubtypeNetworkProcess:
      RecordProcessHistograms("NetworkProcess", metrics);
      break;
  }
}

}  // namespace performance_monitor
