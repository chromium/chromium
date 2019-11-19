// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_

#include <memory>
#include <set>

#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"

namespace base {
class PersistentHistogramAllocator;
}

// SubprocessMetricsProvider gathers and merges histograms stored in shared
// memory segments between processes. Merging occurs when a process exits,
// when metrics are being collected for upload, or when something else needs
// combined metrics (such as the chrome://histograms page).
class SubprocessMetricsProvider
    : public metrics::MetricsProvider,
      public base::StatisticsRecorder::HistogramProvider,
      public content::BrowserChildProcessObserver,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  SubprocessMetricsProvider();
  ~SubprocessMetricsProvider() override;

  // Merge histograms for all subprocesses. This is used by tests that don't
  // have access to the internal instance of this class.
  static void MergeHistogramDeltasForTesting();

 private:
  friend class SubprocessMetricsProviderTest;

  // Indicates subprocess to be monitored with unique id for later reference.
  // Metrics reporting will read histograms from it and upload them to UMA.
  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator);

  // Indicates that a subprocess has exited and is thus finished with the
  // allocator it was using.
  void DeregisterSubprocessAllocator(int id);

  // Merge all histograms of a given allocator to the global StatisticsRecorder.
  // This is called periodically during UMA metrics collection (if enabled) and
  // possibly on-demand for other purposes.
  void MergeHistogramDeltasFromAllocator(
      int id,
      base::PersistentHistogramAllocator* allocator);

  // metrics::MetricsProvider:
  void MergeHistogramDeltas() override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Gets a histogram allocator from a subprocess. This must be called on
  // the IO thread.
  static std::unique_ptr<base::PersistentHistogramAllocator>
  GetSubprocessHistogramAllocatorOnIOThread(int id);

  THREAD_CHECKER(thread_checker_);

  // All of the shared-persistent-allocators for known sub-processes.
  using AllocatorByIdMap =
      base::IDMap<std::unique_ptr<base::PersistentHistogramAllocator>, int>;
  AllocatorByIdMap allocators_by_id_;

  // Track all observed render processes to un-observe them on exit.
  ScopedObserver<content::RenderProcessHost, content::RenderProcessHostObserver>
      scoped_observer_{this};

  base::WeakPtrFactory<SubprocessMetricsProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SubprocessMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_
