// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/process_type.h"

namespace base {
class ProcessMetrics;
}

namespace performance_monitor {

enum ProcessSubtypes {
  kProcessSubtypeUnknown,
  kProcessSubtypeExtensionPersistent,
  kProcessSubtypeExtensionEvent,
  kProcessSubtypeNetworkProcess,
};

struct ProcessInfo {
  ProcessInfo(int process_type,
              ProcessSubtypes process_subtype,
              std::unique_ptr<base::ProcessMetrics> process_metrics);
  ~ProcessInfo();

  int process_type;
  ProcessSubtypes process_subtype;
  std::unique_ptr<base::ProcessMetrics> process_metrics;
};

// ProcessMonitor is a tool which periodically monitors performance metrics
// of all the Chrome processes for histogram logging and possibly taking action
// upon noticing serious performance degradation.
class ProcessMonitor : public content::BrowserChildProcessObserver,
                       public content::RenderProcessHostCreationObserver,
                       public content::RenderProcessHostObserver {
 public:
  // The interval at which ProcessMonitor performs its timed collections.
  static constexpr base::TimeDelta kGatherInterval = base::Minutes(2);

  struct Metrics {
    Metrics();
    Metrics(const Metrics& other);
    Metrics& operator=(const Metrics& other);
    ~Metrics();

    // The percentage of time spent executing, across all threads of the
    // process, in the interval since the last time the metric was sampled. This
    // can exceed 100% in multi-thread processes running on multi-core systems.
    double cpu_usage = 0.0;

#if BUILDFLAG(IS_WIN)
    // The percentage of time spent executing, across all threads of the
    // process, in the interval since the last time the metric was sampled. This
    // can exceed 100% in multi-thread processes running on multi-core systems.
    //
    // Calculated using the more precise QueryProcessCycleTime.
    //
    // TODO(pmonette): Replace the regular version of |cpu_usage| with the
    // precise one and remove the extra field once we've validated that the
    // precise version is indeed better.
    double precise_cpu_usage = 0.0;
#endif

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

    // "Energy Impact" is a synthetic power estimation metric displayed by macOS
    // in Activity Monitor and the battery menu.
    double energy_impact = 0.0;
#endif
  };

  class Observer : public base::CheckedObserver {
   public:
    // Provides the sampled metrics for every Chrome process. This is called
    // once per process at a regular interval.
    virtual void OnMetricsSampled(int process_type,
                                  ProcessSubtypes process_subtype,
                                  const Metrics& metrics) {}

    // Provides the aggregated sampled metrics from every Chrome process. This
    // is called once at a regular interval regardless of the number of
    // processes.
    virtual void OnAggregatedMetricsSampled(const Metrics& metrics) {}
  };

  // Creates and returns the application-wide ProcessMonitor. Can only be called
  // if no ProcessMonitor instances exists in the current process. The caller
  // owns the created instance. The current process' instance can be retrieved
  // with Get().
  static std::unique_ptr<ProcessMonitor> Create();

  // Returns the application-wide ProcessMonitor if one exists; otherwise
  // returns nullptr.
  static ProcessMonitor* Get();

  ProcessMonitor(const ProcessMonitor&) = delete;
  ProcessMonitor& operator=(const ProcessMonitor&) = delete;

  ~ProcessMonitor() override;

  // Start the cycle of metrics gathering.
  void StartGatherCycle();

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ProcessMonitor();

  base::ObserverList<Observer>& GetObserversForTesting() {
    return observer_list_;
  }

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

  // Gather the metrics for all the child processes.
  void SampleAllProcesses();

  void SampleProcess();

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      render_process_host_observations_{this};

  ProcessInfo browser_process_info_;

  std::map<content::RenderProcessHost*, ProcessInfo> render_process_infos_;

  std::map<int, ProcessInfo> browser_child_process_infos_;

  // The timer to signal ProcessMonitor to perform its timed collections.
  base::RepeatingTimer repeating_timer_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
