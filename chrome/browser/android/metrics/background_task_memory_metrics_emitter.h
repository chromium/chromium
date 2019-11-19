// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_BACKGROUND_TASK_MEMORY_METRICS_EMITTER_H_
#define CHROME_BROWSER_ANDROID_METRICS_BACKGROUND_TASK_MEMORY_METRICS_EMITTER_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

// This class asynchronously fetches memory metrics for each process, and then
// emits UMA metrics only for the browser process.
// Each instance is self-owned, and will delete itself once it has finished
// emitting metrics.
// This class is a simplified version of |ProcessMemoryMetricsEmitter|.
class BackgroundTaskMemoryMetricsEmitter
    : public base::RefCountedThreadSafe<BackgroundTaskMemoryMetricsEmitter> {
 public:
  struct PageInfo;

  BackgroundTaskMemoryMetricsEmitter(bool is_reduced_mode,
                                     const std::string& task_type_affix);

  // This must be called on the main thread of the browser process.
  void FetchAndEmitProcessMemoryMetrics();

 protected:
  using ReceivedMemoryDumpCallback = base::OnceCallback<
      void(bool, std::unique_ptr<memory_instrumentation::GlobalMemoryDump>)>;

  virtual ~BackgroundTaskMemoryMetricsEmitter();

  // Virtual for testing.
  virtual base::android::ApplicationState GetApplicationState();

  // Virtual for testing.
  virtual void RequestGlobalDump(ReceivedMemoryDumpCallback callback);

  // Callback invoked when memory_instrumentation has the
  // memory dump ready.
  void ReceivedMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

 private:
  void EmitBrowserMemoryMetrics(
      const memory_instrumentation::GlobalMemoryDump::ProcessDump& pmd,
      const std::string& affix);

  friend class base::RefCountedThreadSafe<BackgroundTaskMemoryMetricsEmitter>;
  bool is_reduced_mode_;
  std::string task_type_affix_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskMemoryMetricsEmitter);
};

#endif  // CHROME_BROWSER_ANDROID_METRICS_BACKGROUND_TASK_MEMORY_METRICS_EMITTER_H_
