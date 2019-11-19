// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/background_task_memory_metrics_emitter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/application_status_listener.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/BackgroundTaskMemoryMetricsEmitter_jni.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

using base::android::JavaParamRef;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::kMemoryHistogramPrefix;

BackgroundTaskMemoryMetricsEmitter::BackgroundTaskMemoryMetricsEmitter(
    bool is_reduced_mode,
    const std::string& task_type_affix)
    : is_reduced_mode_(is_reduced_mode), task_type_affix_(task_type_affix) {}

BackgroundTaskMemoryMetricsEmitter::~BackgroundTaskMemoryMetricsEmitter() {}

void BackgroundTaskMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Measure memory usage only if there is no Android Activity. We are only
  // interested in background tasks without UI, so bail out even if the Activity
  // is in background,
  if (GetApplicationState() !=
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES) {
    DVLOG(1)
        << "Abort BackgroundTask memory dump because Chrome has an Activity.";
    return;
  }

  // The callback keeps this object alive until the callback is invoked.
  auto callback =
      base::Bind(&BackgroundTaskMemoryMetricsEmitter::ReceivedMemoryDump, this);

  RequestGlobalDump(callback);
}

base::android::ApplicationState
BackgroundTaskMemoryMetricsEmitter::GetApplicationState() {
  return base::android::ApplicationStatusListener::GetState();
}

void BackgroundTaskMemoryMetricsEmitter::RequestGlobalDump(
    ReceivedMemoryDumpCallback callback) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump({}, std::move(callback));
}

void BackgroundTaskMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
    std::unique_ptr<GlobalMemoryDump> dump) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success || !dump)
    return;

  // Check again if there is no UI to make sure Chrome was not moved to
  // foreground while the dump was happening.
  if (GetApplicationState() !=
      base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES) {
    DVLOG(1) << "Abort BackgroundTask memory dump because Chrome created an "
                "Activity.";
    return;
  }

  const memory_instrumentation::GlobalMemoryDump::ProcessDump*
      browser_process_dump = nullptr;
  for (const auto& pmd : dump->process_dumps()) {
    // In reduced mode, do not emit if the renderer process exists, since this
    // means Chrome is not running in Reduced Mode anymore, but has transitioned
    // to Full Browser.
    if (is_reduced_mode_ &&
        pmd.process_type() ==
            memory_instrumentation::mojom::ProcessType::RENDERER) {
      DVLOG(1) << "Abort BackgroundTask memory dump because Renderer process "
                  "indicates a transition out of Reduced Mode.";
      return;
    }
    if (pmd.process_type() ==
        memory_instrumentation::mojom::ProcessType::BROWSER) {
      // There should not be two browser processes.
      DCHECK(browser_process_dump == nullptr);
      browser_process_dump = &pmd;
    }
  }
  if (browser_process_dump) {
    EmitBrowserMemoryMetrics(*browser_process_dump, /*affix=*/"");
    if (!task_type_affix_.empty())
      EmitBrowserMemoryMetrics(*browser_process_dump, task_type_affix_);
  }
}

void BackgroundTaskMemoryMetricsEmitter::EmitBrowserMemoryMetrics(
    const GlobalMemoryDump::ProcessDump& pmd,
    const std::string& affix) {
  std::string metrics_prefix = kMemoryHistogramPrefix;
  metrics_prefix += "BackgroundTask.";
  if (!affix.empty())
    metrics_prefix += (affix + ".");
  metrics_prefix += "Browser.";

  std::string metrics_suffix =
      (is_reduced_mode_ ? "ReducedMode" : "FullBrowser");

  // The histogram object cannot be cached between these calls since the name
  // is dynamic. MEMORY_METRICS_HISTOGRAM_MB is appropriate because it does not
  // cache the histogram object.
  MEMORY_METRICS_HISTOGRAM_MB(metrics_prefix + "ResidentSet." + metrics_suffix,
                              pmd.os_dump().resident_set_kb / 1024);
  MEMORY_METRICS_HISTOGRAM_MB(
      metrics_prefix + "PrivateMemoryFootprint." + metrics_suffix,
      pmd.os_dump().private_footprint_kb / 1024);
  MEMORY_METRICS_HISTOGRAM_MB(
      metrics_prefix + "SharedMemoryFootprint." + metrics_suffix,
      pmd.os_dump().shared_footprint_kb / 1024);
  MEMORY_METRICS_HISTOGRAM_MB(
      metrics_prefix + "PrivateSwapFootprint." + metrics_suffix,
      pmd.os_dump().private_footprint_swap_kb / 1024);
}

static void JNI_BackgroundTaskMemoryMetricsEmitter_ReportMemoryUsage(
    JNIEnv* env,
    jboolean is_reduced_mode,
    const JavaParamRef<jstring>& task_type_affix) {
  std::string task_type_affix_utf8 =
      task_type_affix.is_null()
          ? ""
          : base::android::ConvertJavaStringToUTF8(env, task_type_affix);
  scoped_refptr<BackgroundTaskMemoryMetricsEmitter> emitter(
      new BackgroundTaskMemoryMetricsEmitter(is_reduced_mode,
                                             task_type_affix_utf8));
  emitter->FetchAndEmitProcessMemoryMetrics();
}
