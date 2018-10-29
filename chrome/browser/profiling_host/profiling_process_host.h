// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiling_host/background_profiling_triggers.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace heap_profiling {

// Not thread safe. Should be used on the browser UI thread only.
//
// This class is responsible for:
//   * Uploading heap dumps via Slow Reports.
//   * Saving heap dumps to disk.
//   * Recording metrics.
class ProfilingProcessHost {
 public:
  // Returns a pointer to the current global profiling process host.
  static ProfilingProcessHost* GetInstance();

  // Starts background profiling and metrics, if appropriate.
  void Start();

  // Create a trace with a heap dump at the given path.
  // This is equivalent to navigating to chrome://tracing, taking a trace with
  // only the memory-infra category selected, waiting 10 seconds, and saving the
  // result to |dest|.
  // |done| will be called on the UI thread.
  using SaveTraceFinishedCallback = base::OnceCallback<void(bool success)>;
  void SaveTraceWithHeapDumpToFile(
      base::FilePath dest,
      SaveTraceFinishedCallback done,
      bool stop_immediately_after_heap_dump_for_tests);

  // Sends a message to the profiling process to report all profiled processes
  // memory data to the crash server (slow-report).
  void RequestProcessReport(std::string trigger_name);

 private:
  friend struct base::DefaultSingletonTraits<ProfilingProcessHost>;
  friend class BackgroundProfilingTriggersTest;
  friend class MemlogBrowserTest;

  ProfilingProcessHost();
  ~ProfilingProcessHost();

  // Called on the UI thread after the heap dump has been added to the trace.
  void DumpProcessFinishedUIThread();

  void SaveTraceToFileOnBlockingThread(base::FilePath dest,
                                       std::string trace,
                                       SaveTraceFinishedCallback done);

  // Reports the profiling mode.
  void ReportMetrics();

  // Handle background triggers on high memory pressure. A trigger will call
  // |RequestProcessReport| on this instance.
  BackgroundProfilingTriggers background_triggers_;

  // Every 24-hours, reports the profiling mode.
  base::RepeatingTimer metrics_timer_;

  // If this URL is non empty, then we will use it instead of the default crash
  // service URL.
  std::string upload_url_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace heap_profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
