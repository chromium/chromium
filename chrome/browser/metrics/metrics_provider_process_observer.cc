// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_provider_process_observer.h"

#include <string_view>

#include "base/check.h"
#include "base/rand_util.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/process_map.h"
#endif

namespace metrics {

namespace {

std::string_view GetProcessTypeSuffix(content::ProcessType process_type) {
  switch (process_type) {
    case content::PROCESS_TYPE_BROWSER:
      return "Browser";
    case content::PROCESS_TYPE_RENDERER:
      return "Renderer";
    case content::PROCESS_TYPE_UTILITY:
      return "Utility";
    case content::PROCESS_TYPE_GPU:
      return "Gpu";
    default:
      return "Other";
  }
}

}  // namespace

MetricsProviderProcessObserver::MetricsProviderProcessObserver(
    Delegate* delegate,
    int downsampling_factor)
    : downsampling_factor_(downsampling_factor), delegate_(*delegate) {
  CHECK(delegate);
  content::BrowserChildProcessObserver::Add(this);

  // Record metrics for the browser process.
  delegate_->StartListeningToProcess(
      static_cast<content::ChildProcessId>(
          base::GetUniqueIdForProcess().GetUnsafeValue()),
      base::GetCurrentProcId(), "Browser");

  // Record 1/`downsampling_factor_` currently active renderers.
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    content::RenderProcessHost* host = it.GetCurrentValue();
    host_observations_.AddObservation(host);
    if (host->IsReady()) {
      ProbabilisticallyListenToRenderer(host);
    }
  }

  // Record the GPU, and Record 1/`downsampling_factor_` currently active
  // utility processes.
  for (content::BrowserChildProcessHostIterator it; !it.Done(); ++it) {
    ProbabilisticallyListenToNonRenderer(it.GetData());
  }
}

MetricsProviderProcessObserver::~MetricsProviderProcessObserver() {
  content::BrowserChildProcessObserver::Remove(this);
}

void MetricsProviderProcessObserver::ProbabilisticallyListenToNonRenderer(
    const content::ChildProcessData& data) {
  if (data.GetProcess().IsValid() &&
      (data.process_type == content::PROCESS_TYPE_GPU ||
       (data.process_type == content::PROCESS_TYPE_UTILITY &&
        data.metrics_name == "network.mojom.NetworkService") ||
       (data.process_type == content::PROCESS_TYPE_UTILITY &&
        base::RandGenerator(downsampling_factor_) == 0))) {
    std::string_view process_suffix =
        (data.process_type == content::PROCESS_TYPE_UTILITY &&
         data.metrics_name == "network.mojom.NetworkService")
            ? "NetworkService"
            : GetProcessTypeSuffix(
                  static_cast<content::ProcessType>(data.process_type));

    delegate_->StartListeningToProcess(data.GetChildProcessId(),
                                       data.GetProcess().Pid(), process_suffix);
  }
}

void MetricsProviderProcessObserver::ProbabilisticallyListenToRenderer(
    content::RenderProcessHost* host) {
  if (host->IsReady() && host->GetProcess().IsValid() &&
      base::RandGenerator(downsampling_factor_) == 0) {
    bool is_extension_renderer = false;
    content::BrowserContext* browser_context = host->GetBrowserContext();
    if (browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      extensions::ProcessMap* process_map =
          extensions::ProcessMap::Get(browser_context);
      if (process_map) {
        is_extension_renderer = process_map->Contains(host->GetDeprecatedID());
      }
#endif
    }
    delegate_->StartListeningToProcess(
        host->GetID(), host->GetProcess().Pid(),
        is_extension_renderer ? "Extension" : "Renderer");
  }
}

void MetricsProviderProcessObserver::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!host_observations_.IsObservingSource(host)) {
    host_observations_.AddObservation(host);
  }
}

void MetricsProviderProcessObserver::RenderProcessReady(
    content::RenderProcessHost* host) {
  ProbabilisticallyListenToRenderer(host);
}

void MetricsProviderProcessObserver::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  delegate_->StopListeningToProcess(host->GetID());
}

void MetricsProviderProcessObserver::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  host_observations_.RemoveObservation(host);
  delegate_->StopListeningToProcess(host->GetID());
}

void MetricsProviderProcessObserver::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  ProbabilisticallyListenToNonRenderer(data);
}

void MetricsProviderProcessObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  delegate_->StopListeningToProcess(data.GetChildProcessId());
}

void MetricsProviderProcessObserver::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  delegate_->StopListeningToProcess(data.GetChildProcessId());
}

void MetricsProviderProcessObserver::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  delegate_->StopListeningToProcess(data.GetChildProcessId());
}

void MetricsProviderProcessObserver::BrowserChildProcessExitedNormally(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  delegate_->StopListeningToProcess(data.GetChildProcessId());
}

}  // namespace metrics
