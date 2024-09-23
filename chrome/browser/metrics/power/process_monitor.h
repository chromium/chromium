// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_PROCESS_MONITOR_H_
#define CHROME_BROWSER_METRICS_POWER_PROCESS_MONITOR_H_

#include <array>
#include <map>
#include <memory>
#include <optional>

#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/scoped_multi_source_observation.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/process_type.h"

namespace base {
class ProcessMetrics;
}

enum MonitoredProcessType {
  kBrowser,
  kRenderer,
  kExtensionPersistent,
  kExtensionEvent,
  kGpu,
  kUtility,
  kNetwork,
  kOther,  // Contains all other process types that are not explicitly tracked.
  kCount,
};

MonitoredProcessType
GetMonitoredProcessTypeForNonRendererChildProcessForTesting(
    const content::ChildProcessData& data);

struct ProcessInfo {
  ProcessInfo(MonitoredProcessType type,
              std::unique_ptr<base::ProcessMetrics> process_metrics);
  ~ProcessInfo();

  MonitoredProcessType type;
  std::unique_ptr<base::ProcessMetrics> process_metrics;
  // The time at which the first process sample was taken (i.e. When the
  // constructor is called). Used to distribute the calculated resource usage of
  // the first interval over the full kLongIntervalDuration. Set to nullopt
  // after the metrics for the first interval is calculated because the
  // subsequent intervals will always take the full duration of
  // kLongIntervalDuration.
  std::optional<base::TimeTicks> first_sample_time;
};

// ProcessMonitor is a tool which allows the sampling of power-related metrics
// for all Chrome processes. The metrics sampling is driven externally by
// calling SampleAllProcesses() periodically.
class ProcessMonitor : public content::BrowserChildProcessObserver,
                       public content::RenderProcessHostCreationObserver,
                       public content::RenderProcessHostObserver {
 public:
  struct Metrics {
    Metrics();
    Metrics(const Metrics& other);
    Metrics& operator=(const Metrics& other);
    ~Metrics();

    // The percentage of time spent executing, across all threads of the
    // process, in the interval since the last time the metric was sampled. This
    // can exceed 100% in multi-thread processes running on multi-core systems.
    // nullopt if there was an error calculating the CPU usage.
    std::optional<double> cpu_usage;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
    // Returns the number of average idle cpu wakeups per second since the last
    // time the metric was sampled.
    int idle_wakeups = 0;
#endif

#if BUILDFLAG(IS_MAC)
    // The number of average "package idle exits" per second since the last
    // time the metric was sampled. See base/process/process_metrics.h for a
    // more detailed explanation.
    int package_idle_wakeups = 0;
#endif
  };

  class Observer : public base::CheckedObserver {
   public:
    // Provides aggregated sampled metrics for all Chrome process of type
    // `type`. This is called once per process type whenever
    // `SampleAllProcesses` is called.
    virtual void OnMetricsSampled(MonitoredProcessType type,
                                  const Metrics& metrics) {}

    // Provides the aggregated sampled metrics from every Chrome process. This
    // is called once whenever `SampleAllProcesses` is called.
    virtual void OnAggregatedMetricsSampled(const Metrics& metrics) {}
  };

  ProcessMonitor();

  ProcessMonitor(const ProcessMonitor&) = delete;
  ProcessMonitor& operator=(const ProcessMonitor&) = delete;

  ~ProcessMonitor() override;

  // Gather the metrics for all the processes.
  virtual void SampleAllProcesses(Observer* observer);

 private:
  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* render_process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessReady(
      content::RenderProcessHost* render_process_host) override;
  void RenderProcessExited(
      content::RenderProcessHost* render_process_host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(
      content::RenderProcessHost* render_process_host) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessExitedNormally(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void OnBrowserChildProcessExited(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info);

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      render_process_host_observations_{this};

  ProcessInfo browser_process_info_;

  std::map<content::RenderProcessHost*, ProcessInfo> render_process_infos_;

  std::map<int, ProcessInfo> browser_child_process_infos_;

  // The metrics for the processes that exited during the last interval. Added
  // to the current interval's sample and then reset to zero.
  std::array<Metrics, MonitoredProcessType::kCount> exited_processes_metrics_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_PROCESS_MONITOR_H_
