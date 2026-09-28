// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/memory_metrics_logger.h"

#include <string_view>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_render_process_lifecycle.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

using memory_instrumentation::GetPrivateFootprintHistogramName;
using memory_instrumentation::HistogramProcessType;

namespace metrics {
namespace {

MemoryMetricsLogger* g_instance = nullptr;

constexpr char kRendererFootprintHistogramPrefix[] =
    "Memory.Renderer.PrivateMemoryFootprint.";

struct RendererFootprint {
  base::ProcessId pid;
  uint32_t private_footprint_kb;
};

// Returns the live RenderProcessHosts, keyed by process id.
absl::flat_hash_map<base::ProcessId, content::RenderProcessHost*>
GetHostsByPid() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::flat_hash_map<base::ProcessId, content::RenderProcessHost*>
      hosts_by_pid;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* host = iter.GetCurrentValue();
    if (host->GetProcess().IsValid()) {
      hosts_by_pid[host->GetProcess().Pid()] = host;
    }
  }
  return hosts_by_pid;
}

// Records Memory.Renderer.PrivateMemoryFootprint split by the renderer's
// lifecycle state. This has to happen on the UI thread because it touches RPH.
void RecordRendererFootprintByLifecycleState(
    std::vector<RendererFootprint> renderers,
    MemoryMetricsLogger::RecordCallback done_callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const absl::flat_hash_map<base::ProcessId, content::RenderProcessHost*>
      hosts_by_pid = GetHostsByPid();

  for (const auto& renderer : renderers) {
    // The process may have gone away between the dump and this task. In that
    // case there is no host, which is recorded as the Unknown state so that
    // every renderer in the dump is accounted for.
    auto iter = hosts_by_pid.find(renderer.pid);
    content::RenderProcessHost* host =
        iter != hosts_by_pid.end() ? iter->second : nullptr;
    MEMORY_METRICS_HISTOGRAM_MB(
        base::StrCat(
            {kRendererFootprintHistogramPrefix,
             android_webview::GetAwRenderProcessLifecycleStateString(host)}),
        renderer.private_footprint_kb / 1024);
  }

  if (done_callback) {
    std::move(done_callback).Run(true);
  }
}

// Called once the metrics have been determined. Does the actual logging.
void RecordMemoryMetricsImpl(
    MemoryMetricsLogger::RecordCallback done_callback,
    memory_instrumentation::mojom::RequestOutcome outcome,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  if (outcome != memory_instrumentation::mojom::RequestOutcome::kSuccess) {
    if (done_callback) {
      std::move(done_callback).Run(false);
    }
    return;
  }

  uint64_t total_private_footprint_kb = 0;
  uint64_t total_resident_set_kb = 0;
  std::vector<RendererFootprint> renderer_footprints;
  for (const auto& process_dump : dump->process_dumps()) {
    total_private_footprint_kb += process_dump.os_dump().private_footprint_kb;
    total_resident_set_kb += process_dump.os_dump().resident_set_kb;

    uint64_t rss_mb = process_dump.os_dump().resident_set_kb / 1024;
    uint64_t rss_peak_mb = process_dump.os_dump().peak_resident_set_kb / 1024;

    switch (process_dump.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER: {
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kBrowser),
            process_dump.os_dump().private_footprint_kb / 1024);
        MEMORY_METRICS_HISTOGRAM_MB("Memory.Browser.ResidentSet", rss_mb);
        // Peak RSS can be 0 if collection failed or is not supported by the
        // kernel. Only log valid >0 values.
        if (rss_peak_mb > 0) {
          MEMORY_METRICS_HISTOGRAM_MB("Memory.Browser.ResidentSetPeak",
                                      rss_peak_mb);
        }

        std::optional<uint64_t> malloc_pa_allocated_objects_bytes =
            process_dump.GetMetric("malloc/partitions",
                                   "allocated_objects_size");
        if (malloc_pa_allocated_objects_bytes) {
          MEMORY_METRICS_HISTOGRAM_MB(
              "Memory.Browser.PartitionAlloc.Malloc.AllocatedObjects",
              *malloc_pa_allocated_objects_bytes / (1024 * 1024));
        }
        break;
      }
      case memory_instrumentation::mojom::ProcessType::RENDERER: {
        // On the desktop this may be attributed to an 'extension', but as
        // android doesn't support extensions there is no checking.
        MEMORY_METRICS_HISTOGRAM_MB(
            GetPrivateFootprintHistogramName(HistogramProcessType::kRenderer),
            process_dump.os_dump().private_footprint_kb / 1024);
        MEMORY_METRICS_HISTOGRAM_MB("Memory.Renderer.ResidentSet", rss_mb);
        // Peak RSS can be 0 if collection failed or is not supported by the
        // kernel. Only log valid >0 values.
        if (rss_peak_mb > 0) {
          MEMORY_METRICS_HISTOGRAM_MB("Memory.Renderer.ResidentSetPeak",
                                      rss_peak_mb);
        }
        renderer_footprints.push_back(
            {process_dump.pid(), process_dump.os_dump().private_footprint_kb});
        break;
      }

      // Currently this class only records metrics for the browser and
      // renderer process, as it originated from WebView, where there are no
      // other processes.
      case memory_instrumentation::mojom::ProcessType::GPU:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::ARC:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::UTILITY:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::PLUGIN:
        [[fallthrough]];
      case memory_instrumentation::mojom::ProcessType::OTHER:
        break;
    }
  }
  if (total_private_footprint_kb) {
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.PrivateMemoryFootprint",
                                total_private_footprint_kb / 1024);
  }
  if (total_resident_set_kb) {
    MEMORY_METRICS_HISTOGRAM_MB("Memory.Total.ResidentSet",
                                total_resident_set_kb / 1024);
  }
  if (!renderer_footprints.empty()) {
    // The recording has to happen on the UI thread, but MemoryMetricsLogger
    // promises to run `done_callback` on the background TaskRunner.
    if (done_callback) {
      done_callback =
          base::BindPostTaskToCurrentDefault(std::move(done_callback));
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RecordRendererFootprintByLifecycleState,
                                  std::move(renderer_footprints),
                                  std::move(done_callback)));
  } else if (done_callback) {
    std::move(done_callback).Run(true);
  }
}

}  // namespace

// State is used to trigger logging to stop. State is accessed on both the main
// thread and the background task runner.
struct MemoryMetricsLogger::State : public base::RefCountedThreadSafe<State> {
  State() = default;

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  // MemoryInstrumentation requires a SequencedTaskRunner.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  bool stop_logging = false;

 private:
  friend class base::RefCountedThreadSafe<State>;

  ~State() = default;
};

MemoryMetricsLogger::MemoryMetricsLogger()
    : state_(base::MakeRefCounted<State>()) {
  g_instance = this;
  state_->task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
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
  if (state->stop_logging) {
    return;
  }

  state->task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MemoryMetricsLogger::RecordMemoryMetrics, state,
                     RecordCallback()),
      memory_instrumentation::GetDelayForNextMemoryLog());
}

// static
void MemoryMetricsLogger::RecordMemoryMetrics(scoped_refptr<State> state,
                                              RecordCallback done_callback) {
  auto* instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  if (!instrumentation) {
    // Content layer is not initialized yet, nothing to log.
    return;
  }
  instrumentation->RequestGlobalDump(
      {"malloc/partitions"},
      base::BindOnce(&RecordMemoryMetricsImpl, std::move(done_callback)));
  RecordMemoryMetricsAfterDelay(state);
}

}  // namespace metrics
