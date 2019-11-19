// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/memory_metrics_logger.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

using memory_instrumentation::GetPrivateFootprintHistogramName;
using memory_instrumentation::HistogramProcessType;

namespace android_webview {
namespace {

MemoryMetricsLogger* g_instance = nullptr;

// Called once the metrics have been determined. Does the actual logging.
void RecordMemoryMetricsImpl(
    MemoryMetricsLogger::RecordCallback done_callback,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  if (!success) {
    if (done_callback)
      std::move(done_callback).Run(false);
    return;
  }

  uint64_t total_private_footprint_kb = 0;
  for (const auto& process_dump : dump->process_dumps()) {
    total_private_footprint_kb += process_dump.os_dump().private_footprint_kb;
    switch (process_dump.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kBrowser),
            process_dump.os_dump().private_footprint_kb / 1024);
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        // On the desktop this may be attributed to an 'extension', but as
        // android doesn't support extensions there is no checking.
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kRenderer),
            process_dump.os_dump().private_footprint_kb / 1024);
        break;
      }

      // WebView only supports the browser and possibly renderer process.
      case memory_instrumentation::mojom::ProcessType::GPU:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::ARC:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::UTILITY:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
        FALLTHROUGH;
      case memory_instrumentation::mojom::ProcessType::OTHER:
        NOTREACHED();
        break;
    }
  }
  if (total_private_footprint_kb) {
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.PrivateMemoryFootprint",
                                total_private_footprint_kb / 1024);
  }
  if (done_callback)
    std::move(done_callback).Run(true);
}

}  // namespace

// State is used to trigger logging to stop. State is accessed on both the main
// thread and the background task runner.
struct MemoryMetricsLogger::State : public base::RefCountedThreadSafe<State> {
  State() = default;

  // MemoryInstrumentation requires a SequencedTaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  bool stop_logging = false;

 private:
  friend class base::RefCountedThreadSafe<State>;

  ~State() = default;

  DISALLOW_COPY_AND_ASSIGN(State);
};

MemoryMetricsLogger::MemoryMetricsLogger()
    : state_(base::MakeRefCounted<State>()) {
  g_instance = this;
  state_->task_runner = base::CreateSequencedTaskRunner({base::ThreadPool()});
  state_->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetricsAfterDelay,
                     state_));
}

MemoryMetricsLogger::~MemoryMetricsLogger() {
  g_instance = nullptr;
  state_->stop_logging = true;
}

// static
MemoryMetricsLogger* MemoryMetricsLogger::GetInstanceForTesting() {
  return g_instance;
}

void MemoryMetricsLogger::ScheduleRecordForTesting(
    RecordCallback done_callback) {
  state_->task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetrics,
                                state_, std::move(done_callback)));
}

// static
void MemoryMetricsLogger::RecordMemoryMetricsAfterDelay(
    scoped_refptr<State> state) {
  if (state->stop_logging)
    return;

  state->task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetrics, state,
                     RecordCallback()),
      memory_instrumentation::GetDelayForNextMemoryLog());
}

// static
void MemoryMetricsLogger::RecordMemoryMetrics(scoped_refptr<State> state,
                                              RecordCallback done_callback) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump({}, base::BindOnce(&RecordMemoryMetricsImpl,
                                             std::move(done_callback)));
  RecordMemoryMetricsAfterDelay(state);
}

}  // namespace android_webview
