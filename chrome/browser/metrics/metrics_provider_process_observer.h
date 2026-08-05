// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_PROVIDER_PROCESS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_METRICS_PROVIDER_PROCESS_OBSERVER_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "base/scoped_multi_source_observation.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/child_process_id.h"

namespace metrics {

// Observes process creations and destructions (browser, renderer, GPU, and
// utility processes) and notifies a Delegate to start/stop listening to process
// performance metrics.
class MetricsProviderProcessObserver
    : public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver,
      public content::BrowserChildProcessObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Invoked when the related `MetricsProviderProcessObserver` registers an
    // event that would indicate that a process is ready to be monitored for
    // metrics. Note that this will not be called for every process that is
    // created, and will instead be scaled in the case of utility and renderer
    // processes by the observer's `downsampling_factor`.
    virtual void StartListeningToProcess(
        content::ChildProcessId content_id,
        base::ProcessId pid,
        std::string_view process_type_suffix) = 0;
    // Invoked when the related `MetricsProviderProcessObserver` registers an
    // event that would indicate that a process is no longer active. This can
    // happen multiple times for a given process, and can be called for
    // processes for which StartListeningToProcess() was not called. The
    // Observer does not keep track of such matching of events, this should be
    // handled by the Delegate. These calls are not downsampled by the
    // observer's `downsampling_factor`.
    virtual void StopListeningToProcess(content::ChildProcessId content_id) = 0;
  };

  // The `MetricsProviderProcessObserver` will call StartListeningToProcess()
  // for many of the non-transient process types unconditionally (browser, gpu,
  // network service), but for renderer+utility processes, will only call on the
  // Delegate to monitor `1/downsampling_factor` of the processes.
  MetricsProviderProcessObserver(Delegate* delegate, int downsampling_factor);
  ~MetricsProviderProcessObserver() override;

  MetricsProviderProcessObserver(const MetricsProviderProcessObserver&) =
      delete;
  MetricsProviderProcessObserver& operator=(
      const MetricsProviderProcessObserver&) = delete;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

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

 private:
  // Depending on the type of the child process, listen to the given process's
  // metrics, or, in the case of miscellaneous utility processes, listen to
  // them only 1/`downsampling_factor` of the time.
  void ProbabilisticallyListenToNonRenderer(
      const content::ChildProcessData& data);

  // Listen to renderers only 1/`downsampling_factor` of the time.
  void ProbabilisticallyListenToRenderer(content::RenderProcessHost* host);

  // The downsampling factor is the ratio of the odds for which a renderer or
  // utility process will have its data collected. These metrics will be
  // recorded for the browser process, the GPU process, the network service,
  // but any additional process (i.e. renderers, extensions, utilities) will
  // only be recorded `1/downsampling_factor_` of the time.
  const int downsampling_factor_;
  const raw_ref<Delegate> delegate_;
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observations_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_METRICS_PROVIDER_PROCESS_OBSERVER_H_
